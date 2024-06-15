/*
#include "opencv2/opencv.hpp"
#include "opencv2/objdetect.hpp"
#include "opencv2/imgcodecs.hpp"
#include <iostream>
#include <cstring>

using namespace cv;
using namespace std;


//여기서 linetaracer.c 불러오자
extern "C" {
    char* qr_detect();
}

//이거 없으면 우리 error난다. 조심. 
bool isValidQRCode(vector<Point> points) {
    if (points.size() != 4) {
        return false;
    }

    double area = contourArea(points);
    if (area < 1) {
        return false;
    }

    return true;
}

char* qr_detect()
{
    //qr 담을 buffer.
    static char info[128];
    QRCodeDetector detector;
    Mat frame, gray;
    VideoCapture cap(0);

    if (!cap.isOpened()) {
        cerr << "##### CAMERA OPEN ERROR #####" << endl;
        return NULL;
    }

    // Set frame resolution to a smaller size
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);
    
    time_t start_time = time(NULL);
    time_t cur_time;

    while (1) {
        // cur_time = time(NULL);
        // if(difftime(cur_time, start_time)>180){
        //     break;
        // }

        cap >> frame;
        if (frame.empty()) {
            cerr << "##### FRAME EMPTY #####" << endl;
            continue;
        }

        cvtColor(frame, gray, COLOR_BGR2GRAY);

        vector<Point> points;
        bool qrCodeDetected = detector.detect(gray, points);

        if (isValidQRCode(points) && qrCodeDetected) {
            polylines(frame, points, true, Scalar(0, 255, 255), 2);

            String decodedInfo = detector.decode(gray, points);
            if (!decodedInfo.empty()) {
                strncpy(info, decodedInfo.c_str(), sizeof(info) - 1);
                info[sizeof(info) - 1] = '\0'; // Ensure null termination

                frame.release();
                gray.release();
                cap.release();
                return info; // Return the decoded information
            }
        }

        // Release memory used by Mat objects to avoid memory leaks
        frame.release();
        gray.release();
    }
    return NULL;
}
*/


#include "opencv2/opencv.hpp"
#include "opencv2/objdetect.hpp"
#include "opencv2/imgcodecs.hpp"
#include <iostream>
#include <cstring>
#include <new>

using namespace cv;
using namespace std;

//여기서 불러오자.
extern "C" {
    char* qr_detect();
}

// QR size 체크해놓자.
bool isValidQRCode(const vector<Point>& points) {
    if (points.size() != 4) {
        return false;
    }

    double area = contourArea(points);
    if (area < 1 || area > 100000) {
        return false;
    }

    return true;
}

char* qr_detect()
{
    //qr 담을 버퍼..
    static char info[128];
    QRCodeDetector detector;
    Mat frame, gray;
    VideoCapture cap(0);

    if (!cap.isOpened()) {
        cerr << "##### CAMERA OPEN ERROR #####" << endl;
        return NULL;
    }

    // 이거 없으면 QR 인식 잘 안됨.
    cap.set(CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CAP_PROP_FRAME_HEIGHT, 480);

    while (true) {
        cap >> frame;
        if (frame.empty()) {
            cerr << "##### FRAME EMPTY #####" << endl;
            continue;
        }

        try {
            cvtColor(frame, gray, COLOR_BGR2GRAY);
        } catch (const cv::Exception& e) {
            cerr << "##### CVTCOLOR ERROR: " << e.what() << " #####" << endl;
            frame.release();
            continue;
        }

        vector<Point> points;
        bool qrCodeDetected = detector.detect(gray, points);

        if (isValidQRCode(points) && qrCodeDetected) {
            polylines(frame, points, true, Scalar(0, 255, 255), 2);

            String decodedInfo;
            try {
                decodedInfo = detector.decode(gray, points);
            } catch (const cv::Exception& e) {
                cerr << "##### DECODE ERROR: " << e.what() << " #####" << endl;
                frame.release();
                gray.release();
                continue;
            }

            if (!decodedInfo.empty()) {
                strncpy(info, decodedInfo.c_str(), sizeof(info) - 1);
                info[sizeof(info) - 1] = '\0'; // Ensure null termination

                frame.release();
                gray.release();
                cap.release();
                return info; // Return the decoded information
            }
        }

        //이거 해야 메모리 초과 X
        frame.release();
        gray.release();
    }
    return NULL;
}
