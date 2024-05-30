#include "opencv2/opencv.hpp"
#include "opencv2/objdetect.hpp"
#include "opencv2/imgcodecs.hpp"
#include <iostream>

using namespace cv;
using namespace std;

// To connect with linetracer.c
extern "C" {
    int qr_detect();
}

// To check QR size (preventing error)
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

int qr_detect()
{
    QRCodeDetector detector;
    Mat frame, gray;
    VideoCapture cap(0);

    if (!cap.isOpened()) {
        cerr << "##### CAMERA OPEN ERROR #####" << endl;
        return -1;
    }
    
    time_t start_time = time(NULL);
    time_t cur_time;

    while (1) {
        cur_time = time(NULL);
        if(difftime(cur_time, start_time)>30){
            break;
        }

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

            String info = detector.decode(gray, points);
            if (!info.empty()) {
                cout << "Decoded Data: " << info << endl;
            }
        } else {
            // cout << "### NO QR ###.\n";
        }
    }
    return 0;
}