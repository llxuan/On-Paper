//
// Created by 牛天睿 on 17/5/18.
//

#include "ARCapturer.h"
#include "cvutils.h"


// must call input_image first!
unsigned long on_paper::ARCapturer::process() {
    // copy image
    // Detection of markers in the image passed
    TheMarkers= MDetector.detect(TheInputImageCopy, CamParams, TheMarkerSize);
    fill_markers();
    if(perform_anti_shake)
        anti_shake();
    map_markers();
    TheLastMarkers=TheMarkers;
    return TheMarkers.size();
}

void on_paper::ARCapturer::init(CameraParameters cp) {

    this->CamParams = cp;
    MDetector.setCornerRefinementMethod(aruco::MarkerDetector::SUBPIX);
    TheInputImageCopy = Mat::zeros(100,100,CV_8UC1); //FIXME potential bug.
    TheLastMarkers=MDetector.detect(TheInputImageCopy, CamParams, TheMarkerSize);



    auto&& get_shifted_paper_pattern=[](float ratio_x, float ratio_y, const vector<Point2f> vp2f){
        assert(vp2f.size()==4);
        float height = vp2f[0].y*2;
        float width  = vp2f[0].x*2;
        float shifty = (height*ratio_y),
                shiftx= (width*ratio_x);

        vector<Point2f> ps ={
                Point2f(vp2f[0].x+shiftx, vp2f[0].y+shifty),
                Point2f(vp2f[1].x+shiftx, vp2f[1].y+shifty),
                Point2f(vp2f[2].x+shiftx, vp2f[2].y+shifty),
                Point2f(vp2f[3].x+shiftx, vp2f[3].y+shifty)
        };
        return std::move(ps);

    };
    //add shifted patterns.
    for(auto& s : shifts)
        this->shifted_pattern_paper_source.emplace_back(
                get_shifted_paper_pattern(s.first, s.second, pattern_paper_source));



    MDetector.setDictionary("ARUCO");
    MDetector.setThresholdParams(7, 7);
    MDetector.setThresholdParamRange(2, 0);
    this->perform_anti_shake = false;

}


#define S2(X) ((X)*(X))
template<typename vecpf>
float on_paper::ARCapturer::euclid_dist(const vecpf &v1, const vecpf &v2) {
    if(v1.size()!=v2.size())return -1;
    float acc=0;
    for(auto i = 0; i< v1.size();i++){
        auto& p1=v1[i];
        auto& p2=v2[i];
        acc+= sqrt(S2(p1.x-p2.x)+S2(p1.y-p2.y));
    }
    return acc/v1.size();
}
#undef S2


cv::Mat on_paper::ARCapturer::resize(const cv::Mat &in, int width)
{
        if (in.size().width<=width) return in;
        float yf=float(  width)/float(in.size().width);
        cv::Mat im2;
        cv::resize(in,im2,cv::Size(width,float(in.size().height)*yf));
        return im2;
}



/**
 * @brief 把所得的marker映射到纸上。
 * 将会把marker映射到的四个点进行平均。
 * 遵循以下策略：
 * 1. 如果没有中间部分的marker，使用两个角上的任意一个marker。
 * 2. 如果存在中间部分的marker，使用两个marker映射结果的平均值。(TODO 加上两个角上的marker的干预)
 * //TODO 2017-5-27 迫使该方法退役。
 */

void on_paper::ARCapturer::map_markers(void) {

    bool exists_in_middle = false;
    if (TheMarkers.size() > 0 && CamParams.isValid() && TheMarkerSize > 0)
    {
        for(auto& m : TheMarkers){
            if(m.id%4==1 or m.id%4 ==2)//中间部分的marker
            exists_in_middle = true;
        }

    } else return;//do nothing and back!

    vector<Point2f> virtual_paper ;

    for (unsigned long num = 0; num < TheMarkers.size(); num++) {

        Marker &lamaker = TheMarkers[num];
        //如果存在中间点，则忽略两侧的点。
        if(exists_in_middle and (lamaker.id%4 == 0 or lamaker.id%4 == 3))
            continue;

        //Point mcenter = lamaker.getCenter();
        if (lamaker.size() != 4) {
            std::cout << "Panic! " << std::endl;
            exit(-9);
        }

        Mat M0=getPerspectiveTransform(pattern_marker_source, lamaker);

        vector<Point2f> __virtual_paper;

        perspectiveTransform(shifted_pattern_paper_source[lamaker.id%4], __virtual_paper, M0);
        if(virtual_paper.size()==0)
            virtual_paper=__virtual_paper;
        else
            //如果我们有两个marker，用它进行平均。
            virtual_paper = vector_avg2(virtual_paper, __virtual_paper);
    }


    Mat transf = Mat::zeros(TheInputImageCopy.size(), CV_8UC4);
    pdf_paper_image=pdfread(TheMarkers);




    set_original_paper_pattern();



    //把原图投射到虚拟纸张上。
    Mat M = getPerspectiveTransform(original_image_pattern, virtual_paper);
    Mat M_inv = getPerspectiveTransform(virtual_paper, original_image_pattern);
    this->transmatrix_inv = M_inv; //set val to M_inv.
    this->transmatrix = M; // set val to M.

    warpPerspective(pdf_paper_image, transf, M, TheInputImageCopy.size(), cv::INTER_NEAREST);

    //if(need_white_transparent)
       // utils::white_transparent(transf, transf);

    VirtualPaperImage = transf;

}

vector<cv::Point2f> on_paper::ARCapturer::vector_avg2(const vector<cv::Point2f> &src1, const vector<cv::Point2f> &src2)
{
    vector<Point2f> dst;
    if(src1.size()!= src2.size()){
        cout<<"Vectors must be of same size!"<<endl;
        exit(999);
    }
    for(unsigned int i = 0;i<src1.size();i++)
        dst.push_back(Point2f((src1[i].x+src2[i].x)/2, (src1[i].y+src2[i].y)/2));
    return dst;
}

void on_paper::ARCapturer::anti_shake(void) {
#define distance(a,b) (sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)))
    for (unsigned int i = 0; i < TheMarkers.size()&&i<TheLastMarkers.size(); i++) {
        Point center=TheMarkers[i].getCenter();
        Point pre_center=TheLastMarkers[i].getCenter();
        if(distance(center,pre_center)<10)
            TheMarkers[i]=TheLastMarkers[i];
    }
}

void on_paper::ARCapturer::fill_markers(void) {
    for(auto& lamaker : TheMarkers) {//fill the marker with white.
        vector<Point> corners(lamaker.begin(), lamaker.end());
        vector<vector<Point>> c_corners = {corners};
    }
}

void on_paper::ARCapturer::display_enlarged_area(cv::Rect r) {
    //do some anti-shake
    if(utils::distance_P2P(Point(r.x, r.y), Point(last_rect.x, last_rect.y)) > 20) {
        Mat roi = Mat();
        if(r.x +r.width > pdf_paper_image.cols or
                r.y+r.height >pdf_paper_image.rows)
            return;
        pdf_paper_image(r).copyTo(roi);
        if(roi.cols<=0)return;
        Mat roi_converted;
        utils::tnerapsnart_etihw(roi, roi_converted);
        roi=roi_converted;
        cv::resize(roi, roi, cv::Size(enlarge_wheight, enlarge_wwidth));
        last_roi=roi;
        last_rect = r;
    }

    vector<vector<Point>> vpp;
    vector<Point> vp;
    vp.push_back(Point(0,0));
    vp.push_back(Point(0,enlarge_wwidth));
    vp.push_back(Point(enlarge_wheight,enlarge_wwidth));
    vp.push_back(Point(enlarge_wheight,0));
    vpp.push_back(vp);
    fillPoly(TheInputImageCopy, vpp, Scalar(255,255,255),LINE_8, 0);
    last_roi.copyTo(TheInputImageCopy(Rect(Point(0,0), Point(enlarge_wheight, enlarge_wwidth))));

}


cv::Mat on_paper::ARCapturer::pdfread(vector<aruco::Marker> marker)
{
    if(marker.size()<=0)
        exit(122);

    int id,page;
    string picname;
    cv::Mat img;

    for(auto& m : marker){
        id=m.id;
        page=id/MARKERNUM ;//+1; //modified
    }


    if(page!=cur_page && page < PdfReader.get_pagenum())
    {
        PdfReader.render_pdf_page(page);

        cur_page=page;
        img=PdfReader.cv_get_pdf_image();

        pa_ptr->init_canvas_of_page(cur_page,img.rows, img.cols);
        pa_ptr->initKF();//re-init the kalman filter.
    }
    else
        img=pdf_paper_image;

    return img;
}

void on_paper::ARCapturer::read_pdf_archiv(string pdf_file)
{

    this->cur_page = 0; // reset current page.

    PdfReader.load_pdf(QString::fromStdString(pdf_file));
    if(not PdfReader.render_pdf_page(0)) // get the page 0.
        exit(121); //ad hoc
    pdf_paper_image=PdfReader.cv_get_pdf_image();


    set_original_paper_pattern();
    pa_ptr->init(pdf_paper_image.rows, pdf_paper_image.cols);

}

void on_paper::ARCapturer::set_original_paper_pattern()
{
    this->original_image_pattern = {
            Point2f(pdf_paper_image.cols, pdf_paper_image.rows),
            Point2f(0, pdf_paper_image.rows),
            Point2f(0, 0),
            Point2f(pdf_paper_image.cols, 0)
    };
}



#undef distance

