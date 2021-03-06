//
// Created by 牛天睿 on 17/5/24.
//
#pragma once
#ifndef ARTEST_CVUTILS_H
#define ARTEST_CVUTILS_H

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <sys/time.h>
#include <QImage>
#include <QDebug>
#include <iostream>

namespace on_paper{
    using namespace std;
    using namespace cv;
    using std::cout;using std::endl;
    class utils {
    public:
        constexpr const static float PI = 3.14;

        static long curtime_msec(void){
            struct timeval tv;
            gettimeofday(&tv,NULL);
            return tv.tv_sec * 1000 + tv.tv_usec / 1000;
        }


        inline static QImage Mat2QImage(const cv::Mat& mat)
        {
            // 8-bits unsigned, NO. OF CHANNELS = 1
            if(mat.type() == CV_8UC1)
            {
                QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
                // Set the color table (used to translate colour indexes to qRgb values)
                image.setColorCount(256);
                for(int i = 0; i < 256; i++)
                {
                    image.setColor(i, qRgb(i, i, i));
                }
                // Copy input Mat
                uchar *pSrc = mat.data;
                for(int row = 0; row < mat.rows; row ++)
                {
                    uchar *pDest = image.scanLine(row);
                    memcpy(pDest, pSrc, mat.cols);
                    pSrc += mat.step;
                }
                return image;
            }
            // 8-bits unsigned, NO. OF CHANNELS = 3
            else if(mat.type() == CV_8UC3)
            {
                // Copy input Mat
                const uchar *pSrc = (const uchar*)mat.data;
                // Create QImage with same dimensions as input Mat
                QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);

                return image.rgbSwapped();
            }
            else if(mat.type() == CV_8UC4)
            {
                qDebug() << "CV_8UC4";
                // Copy input Mat
                const uchar *pSrc = (const uchar*)mat.data;
                // Create QImage with same dimensions as input Mat
                QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
                return image.copy();
            }
            else
            {
                qDebug() << "ERROR: Mat could not be converted to QImage.";
                return QImage();
            }
        }

        static cv::Mat QImage2Mat(QImage image){
            cv::Mat mat;
            switch (image.format())
            {
            case QImage::Format_ARGB32:
            case QImage::Format_RGB32:
            case QImage::Format_ARGB32_Premultiplied:
                mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
                break;
            case QImage::Format_RGB888:
                mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
                cv::cvtColor(mat, mat, CV_BGR2RGB);
                break;
            case QImage::Format_Indexed8:
                mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void*)image.constBits(), image.bytesPerLine());
                break;

            default:
                break;
            }
            return mat;
        }


        static void drawLine(cv::Mat &image, double theta, double rho, cv::Scalar color){
                if (theta < PI / 4. || theta > 3.*PI / 4.)// ~vertical line
                {
                    Point pt1(rho / cos(theta), 0);
                    Point pt2((rho - image.rows * sin(theta)) / cos(theta), image.rows);
                    line(image, pt1, pt2, Scalar(255), 1);
                }
                else
                {
                    Point pt1(0, rho / sin(theta));
                    Point pt2(image.cols, (rho - image.cols * cos(theta)) / sin(theta));
                    line(image, pt1, pt2, color, 1);
                }
        }

        // 两点距离
        static float distance_P2P(cv::Point a, cv::Point b) {
            float d = sqrt(fabs(pow(a.x - b.x, 2) + pow(a.y - b.y, 2)));
            return d;
        }

// 获得三点角度(弧度制)
        static float get_angle(cv::Point s, cv::Point f, cv::Point e) {
            float l1 = distance_P2P(f, s);
            float l2 = distance_P2P(f, e);
            float dot = (s.x - f.x)*(e.x - f.x) + (s.y - f.y)*(e.y - f.y);
            float angle = acos(dot / (l1*l2));
            angle = angle * 180 / PI;
            return angle;
        }


        static void overlay_BGR(const Mat& background, const Mat& foreground, Mat& output) {
            //cv::addWeighted(foreground, 1, background, 1, 0, output);
            Mat mask ;
            //an ad-hoc solution
            cvtColor(foreground, mask, CV_BGR2GRAY);
            threshold(mask, mask, 10,255,THRESH_BINARY);
            background.copyTo(output);
            foreground.copyTo(output, mask);
        }

        static void white_transparent(const Mat & src, Mat& dst){
            cv::cvtColor(src, dst, CV_BGR2BGRA);
            cv::cvtColor(src,dst, CV_BGRA2RGBA);
            // find all white pixel and set alpha value to zero:
            for (int y = 0; y < dst.rows; ++y)
                for (int x = 0; x < dst.cols; ++x)
                {
                    cv::Vec4b & pixel = dst.at<cv::Vec4b>(y, x);
                    // if pixel is white
                    //if((abs(pixel[0]-pixel[1])+abs(pixel[1]-pixel[2])+abs(pixel[2]-pixel[0]))
                     //       > 50)
                      //  continue;
                    if (pixel[0] > 240 && pixel[1] >240 && pixel[2] > 240)
                    {
                        // set alpha to zero:
                        pixel[3] = 0;
                    }
                }
        }

        //inverse of white_transparent.
        static void tnerapsnart_etihw(const Mat & src, Mat& dst){
            cvtColor(src, dst, CV_BGRA2BGR);
            for (int y = 0; y < dst.rows; ++y)
                for (int x = 0; x < dst.cols; ++x)
                {
                    const Vec4b & pixel0 = src.at<cv::Vec4b>(y,x);
                    cv::Vec3b & pixel = dst.at<cv::Vec3b>(y, x);
                    if(pixel0[3]==0)
                    {
                        pixel[0]=255;pixel[1]=255;pixel[2]=255;
                    }
                }
        }


        static void overlay_BGRA(const Mat &background, const Mat &foreground,
                     Mat &output, Point2i location) {
            background.copyTo(output);
            // start at the row indicated by location, or at row 0 if location.y is negative.
            for (int y = std::max(location.y, 0); y < background.rows; ++y) {
                int fY = y - location.y; // because of the translation

                // we are done of we have processed all rows of the foreground image.
                if (fY >= foreground.rows)
                    break;
                // start at the column indicated by location,
                // or at column 0 if location.x is negative.
                for (int x = std::max(location.x, 0); x < background.cols; ++x) {
                    int fX = x - location.x; // because of the translation.

                    // we are done with this row if the column is outside of the foreground image.
                    if (fX >= foreground.cols)
                        break;

                    // determine the opacity of the foregrond pixel, using its fourth (alpha) channel.
                    double opacity =
                            ((double) foreground.data[fY * foreground.step + fX * foreground.channels() + 3])

                            / 255.;
                    // and now combine the background and foreground pixel, using the opacity,
                    // but only if opacity > 0.
                    for (int c = 0; opacity > 0 && c < output.channels(); ++c) {
                        unsigned char foregroundPx =
                                foreground.data[fY * foreground.step + fX * foreground.channels() + c];
                        unsigned char backgroundPx =
                                background.data[y * background.step + x * background.channels() + c];
                        output.data[y * output.step + output.channels() * x + c] =
                                backgroundPx * (1. - opacity) + foregroundPx * opacity;
                    }
                }
            }
        }
        static string intostr(int i)
        {
            string str;
            stringstream ss;
            ss<<i;
            ss>>str;
            return str;
        }

        static string into_name(int page,int Num)
        {
            string name;
            int i,zeronum=Num-1;
            i=page;
            while((i=i/10))
            {
                zeronum--;
            }
            for(int j=0;j<zeronum;j++)
                name+="0";
            name=name+intostr(page);
            return name;
        }
    };

}
#endif //ARTEST_CVUTILS_H
