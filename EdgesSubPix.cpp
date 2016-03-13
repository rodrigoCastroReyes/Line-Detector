#include "EdgesSubPix.h"
#include <cmath>
#include <opencv2/opencv.hpp>
using namespace cv;
using namespace std;

static inline  double getAmplitude(Mat &dx, Mat &dy, int i, int j)
{
    Point2d mag(dx.at<short>(i, j), dy.at<short>(i, j));
    return norm(mag);
}

static inline void getMagNeighbourhood(Mat &dx, Mat &dy, Point &p, int w, int h, vector<double> &mag)
{
    int top = p.y - 1 >= 0 ? p.y - 1 : p.y;
    int down = p.y + 1 < h ? p.y + 1 : p.y;
    int left = p.x - 1 >= 0 ? p.x - 1 : p.x;
    int right = p.x + 1 < w ? p.x + 1 : p.x;

    mag[0] = getAmplitude(dx, dy, top, left);
    mag[1] = getAmplitude(dx, dy, top, p.x);
    mag[2] = getAmplitude(dx, dy, top, right);
    mag[3] = getAmplitude(dx, dy, p.y, left);
    mag[4] = getAmplitude(dx, dy, p.y, p.x);
    mag[5] = getAmplitude(dx, dy, p.y, right);
    mag[6] = getAmplitude(dx, dy, down, left);
    mag[7] = getAmplitude(dx, dy, down, p.x);
    mag[8] = getAmplitude(dx, dy, down, right);
}

static inline void get2ndFacetModelIn3x3(vector<double> &mag, vector<double> &a)
{
    a[0] = (-mag[0] + 2.0 * mag[1] - mag[2] + 2.0 * mag[3] + 5.0 * mag[4] + 2.0 * mag[5] - mag[6] + 2.0 * mag[7] - mag[8]) / 9.0;
    a[1] = (-mag[0] + mag[2] - mag[3] + mag[5] - mag[6] + mag[8]) / 6.0;
    a[2] = (mag[6] + mag[7] + mag[8] - mag[0] - mag[1] - mag[2]) / 6.0;
    a[3] = (mag[0] - 2.0 * mag[1] + mag[2] + mag[3] - 2.0 * mag[4] + mag[5] + mag[6] - 2.0 * mag[7] + mag[8]) / 6.0;
    a[4] = (-mag[0] + mag[2] + mag[6] - mag[8]) / 4.0;
    a[5] = (mag[0] + mag[1] + mag[2] - 2.0 * (mag[3] + mag[4] + mag[5]) + mag[6] + mag[7] + mag[8]) / 6.0;
}
/* 
   Compute the eigenvalues and eigenvectors of the Hessian matrix given by
   dfdrr, dfdrc, and dfdcc, and sort them in descending order according to
   their absolute values. 
*/
static inline void eigenvals(vector<double> &a, double eigval[2], double eigvec[2][2])
{
    // derivatives
    // fx = a[1], fy = a[2]
    // fxy = a[4]
    // fxx = 2 * a[3]
    // fyy = 2 * a[5]
    double dfdrc = a[4];
    double dfdcc = a[3] * 2.0;
    double dfdrr = a[5] * 2.0;
    double theta, t, c, s, e1, e2, n1, n2; /* , phi; */

    /* Compute the eigenvalues and eigenvectors of the Hessian matrix. */
    if (dfdrc != 0.0) {
        theta = 0.5*(dfdcc - dfdrr) / dfdrc;
        t = 1.0 / (fabs(theta) + sqrt(theta*theta + 1.0));
        if (theta < 0.0) t = -t;
        c = 1.0 / sqrt(t*t + 1.0);
        s = t*c;
        e1 = dfdrr - t*dfdrc;
        e2 = dfdcc + t*dfdrc;
    }
    else {
        c = 1.0;
        s = 0.0;
        e1 = dfdrr;
        e2 = dfdcc;
    }
    n1 = c;
    n2 = -s;

    /* If the absolute value of an eigenvalue is larger than the other, put that
    eigenvalue into first position.  If both are of equal absolute value, put
    the negative one first. */
    if (fabs(e1) > fabs(e2)) {
        eigval[0] = e1;
        eigval[1] = e2;
        eigvec[0][0] = n1;
        eigvec[0][1] = n2;
        eigvec[1][0] = -n2;
        eigvec[1][1] = n1;
    }
    else if (fabs(e1) < fabs(e2)) {
        eigval[0] = e2;
        eigval[1] = e1;
        eigvec[0][0] = -n2;
        eigvec[0][1] = n1;
        eigvec[1][0] = n1;
        eigvec[1][1] = n2;
    }
    else {
        if (e1 < e2) {
            eigval[0] = e1;
            eigval[1] = e2;
            eigvec[0][0] = n1;
            eigvec[0][1] = n2;
            eigvec[1][0] = -n2;
            eigvec[1][1] = n1;
        }
        else {
            eigval[0] = e2;
            eigval[1] = e1;
            eigvec[0][0] = -n2;
            eigvec[0][1] = n1;
            eigvec[1][0] = n1;
            eigvec[1][1] = n2;
        }
    }
}

static inline double vector2angle(double x, double y)
{
    double a = std::atan2(y, x);
    return a >= 0.0 ? a : a + 2*CV_PI;
}

void extractSubPixelPoints(Mat &input,Mat &dx, Mat &dy,int threshold)
{
    int w = dx.cols;
    int h = dx.rows;

    float value;
    Point point;
    int k = 0;
    

    Mat imgPx,imgPy,maxValues;
    imgPx = Mat::zeros(input.size(),CV_32F);
    imgPy = Mat::zeros(input.size(),CV_32F);
    maxValues = Mat::zeros(input.size(),CV_8UC1);

    for(int i = 0; i < input.rows; i++){
        for(int j = 0 ; j < input.cols; j++){

            point = Point(j,i);
            vector<double> magNeighbour(9);
            getMagNeighbourhood(dx,dy,point, w, h, magNeighbour);
        
            vector<double> a(9);
            get2ndFacetModelIn3x3(magNeighbour, a);
           
            // Hessian eigen vector 
            double eigvec[2][2], eigval[2];
            eigenvals(a, eigval, eigvec);
            double t = 0.0;
            double ny = eigvec[0][0];
            double nx = eigvec[0][1];
            if (eigval[0] < 0.0)
            {
                double rx = a[1], ry = a[2], rxy = a[4], rxx = a[3] * 2.0, ryy = a[5] * 2.0;
                t = -(rx * nx + ry * ny) / (rxx * nx * nx + 2.0 * rxy * nx * ny + ryy * ny * ny);
            }
            double px = nx * t;
            double py = ny * t;
            
            float x = (float)point.x;//columna
            float y = (float)point.y;//fila
            
            if (fabs(px) <= 0.5 && fabs(py) <= 0.5)
            { 
                if(a[0] >= threshold){
                    x += (float)px;
                    y += (float)py;
                    maxValues.at<uchar>(y,x) = a[0];
                }
            }
        }
    }
    input = maxValues;
}

void linkingLinePoints(Mat &gray,Mat dx,Mat dy,int threshold){
    //Local Processing
    Mat not_linked = gray.clone();
    int size = 1;
    int rows = gray.rows;
    int cols = gray.cols;
    Mat gradient;
    Mat dx_abs,dy_abs;
    convertScaleAbs(dx,dx_abs);
    convertScaleAbs(dy,dy_abs);
    add(dx_abs,dy_abs,gradient);

    float difMag,difAngle, E = 25, A = 2;//thresholdMag = 50,thresholdAngle = 2;
    uchar magGradient,magGradientNeighbour;
    uchar angle,angleNeighbour;

    for(int i = 0 ; i < rows ; i++){
        for(int j = 0 ; j < cols;j++){
            if(not_linked.at<uchar>(i,j) > threshold){
                //review magnitude of gradient in the neighborhood at P(i,j)
                for(int k=-size ; k<=size ; k++){
                    for(int l=-size ; l<=size ; l++){
                        if( i + k >= 0 &&  i + k < rows && j + l >= 0 && j + l < cols ){
                            magGradient = gradient.at<uchar>(i,j);
                            magGradientNeighbour = gradient.at<uchar>(i+k,j+l);
                            difMag = fabs(magGradient - magGradientNeighbour);

                            angle = fastAtan2(dy.at<uchar>(i, j), dx.at<uchar>(i,j));
                            angleNeighbour = fastAtan2(dy.at<uchar>(i + k , j + l), dx.at<uchar>( i + k , j + l));
                            difAngle = fabs(angle - angleNeighbour);    

                            if(difMag <= E && difAngle <= A){// abs grad(f(x,y)) - grad(f(x0,y0)) <= E
                               line(gray, Point(j,i), Point(j+l,i+k),Scalar(255), 1, 8 ); 
                            }
                        }
                    }
                }

            }
        }
    }
}

void morphClose(Mat input,Mat output){
    int dilation_type= MORPH_RECT;
    int dilation_size= 3;
    Mat element = getStructuringElement(dilation_type,Size(dilation_size,dilation_size));
    morphologyEx(input,output,MORPH_CLOSE, element );
}

void applySteger(Mat &gray, Mat &dx, Mat &dy){
    int threshold = 10;
    extractSubPixelPoints(gray,dx,dy,threshold);
    morphClose(gray,gray);
    //linkingLinePoints(gray,dx,dy,threshold);
}