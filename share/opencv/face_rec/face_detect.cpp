// face_rec.cpp : 定義主控台應用程式的進入點。
//
#include "cv.h"
//#include "opencv/ml.h"
#include <stdio.h>
#include <math.h>
#include "haarcascade_frontalface_alt.h"
#include "highgui.h"
#include "face_recognizer.h"

//using namespace std;

#define FOCUS_X			170
#define FOCUS_Y			30
#define FOCUS_WIDTH		460
#define FOCUS_HEIGHT	420
#define EYE0_X	56   // 0.22 s23.8
#define EYE1_X	153  // 0.6 
#define EYE0_Y	78   // 0.3
#define EYE1_Y	78   // 0.3 
#define EYE_SIZE	51 // 0.2
#define EYE_LENGTH	45.0 // with 100x100 final image
#define FACE_SIZE	100

//const char cascadeName[] = "opencv\\data\\haarcascades\\haarcascade_frontalface_alt.xml";

static int detectfaces( IplImage *img, CvRect *face)
{
    int64 t = 0;//偵測速度
	CvSeq* rects;
	CvRect *fRect;
	CvMemStorage* storage;
	CvHaarClassifierCascade* cascade;
	const int flags = CV_HAAR_SCALE_IMAGE;
	const CvScalar redcolor = CV_RGB(255,0,0);
	int		nfaces;

	// Load the HaarCascade classifier for face detection.
	cascade = &haarcascade_frontalface_alt;
	/*cascade = (CvHaarClassifierCascade*)cvLoad(cascadeName, 0, 0, 0 );
	if( !cascade ) {
		printf("ERROR in recognizeFromCam(): Could not load Haar cascade Face detection classifier in '%s'.\n", cascadeName);
		return 0;
	}*/

    //cvEqualizeHist(smallImg, smallImg);//直方圖均衡化

	storage = cvCreateMemStorage(0);
	cvClearMemStorage( storage );

    //t = (int64)cvGetTickCount();
	//rects = cvHaarDetectObjects( img, (CvHaarClassifierCascade*)cascade, storage,
	//			(int)(1.2*256), 3, flags, cvSize(130,130), cvSize(200,200) );
	rects = cvHaarDetectObjects( img, (CvHaarClassifierCascade*)cascade, storage,
				(int)(img->faceScaleFactor*256), 3, flags, img->minFaceDetectSize, img->maxFaceDetectSize);
    //t = (int64)cvGetTickCount() - t;
    //printf( "detection time = %g ms\n", t/((double)cvGetTickFrequency()*1000.) ); 
    //printf("FPS = %f\n",1000/(t/((double)cvGetTickFrequency()*1000.)));

	nfaces = rects->total;
	if (nfaces > 0) {
        fRect = (CvRect*)cvGetSeqElem( rects, 0 );
		face->x = fRect->x;
		face->y = fRect->y;
		face->width = fRect->width;
		face->height = fRect->height;
		//printf("face size = %d\n", fRect->width);
    }
	//cvReleaseHaarClassifierCascade( &cascade );
	cvReleaseMemStorage(&storage);

	return nfaces;
}

#define EYE_THRESHOLD0	400
#define EYE_THRESHOLD1	300

static int detecteyes(IplImage *img, CvRect face_rect, CvPoint *eyes)
{
    
		int i,j;
		int min0 = 65535;
		int min1 = 65535;
		int find;
		CvPoint loc0, loc1;
		CvRect	org_roi;
		int ret = 2;

		int eye_size = (face_rect.width*EYE_SIZE)>>8;
		CvRect eye0rect = {face_rect.x+((face_rect.width*EYE0_X)>>8), face_rect.y+((face_rect.height*EYE0_Y)>>8), eye_size, eye_size};
		CvRect eye1rect = {face_rect.x+((face_rect.width*EYE1_X)>>8), face_rect.y+((face_rect.height*EYE1_Y)>>8), eye_size, eye_size};
		CvMat *eye0sum = cvCreateMat(eye_size, eye_size, CV_16UC1);
		CvMat *eye1sum = cvCreateMat(eye_size, eye_size, CV_16UC1);
		CvMat *kernel = cvCreateMat(5, 5, CV_8UC1);

		org_roi.x = img->roi->xOffset;
		org_roi.y = img->roi->yOffset;
		org_roi.width = img->roi->width;
		org_roi.height = img->roi->height;

		eye0rect.x += org_roi.x;
		eye0rect.y += org_roi.y;
		eye1rect.x += org_roi.x;
		eye1rect.y += org_roi.y;
		// filter a 5x5 of ones filter to emphasize the eye part 

		cvSet(kernel, cvScalar(1));
		cvSetImageROI(img, eye0rect);
		cvConvert(img, eye0sum);
		cvFilter2D(eye0sum, eye0sum, kernel);
		cvSetImageROI(img, eye1rect);
		cvConvert(img, eye1sum);
		cvFilter2D(eye1sum, eye1sum, kernel);
		//find minimum, the minimum will always locate at eye or brow.
		//printf("eye0:\n");
		for(j=0;j<eye_size;j++) {	
			for(i=0;i<eye_size;i++) {
				int tmp = eye0sum->data.s[j*eye0sum->cols+i];
				//printf("%5d", tmp);
				if(tmp<min0) {
					loc0.y = j;
					loc0.x = i;
					min0 = tmp;
				}
			}
			//printf("\n");
		}

		//printf("eye1:\n");
		for(j=0;j<eye_size;j++) {	
			for(i=0;i<eye_size;i++) {
				int tmp = eye1sum->data.s[j*eye1sum->cols+i];
				//printf("%5d", tmp);
				if(tmp<min1) {
					loc1.y = j;
					loc1.x = i;
					min1 = tmp;
				}
			}
			//printf("\n");
		}
		//printf("eye0: (%d,%d) min= %d, size=%d\n",loc0.x,loc0.y,min0, eye_size);
		//printf("eye1: (%d,%d) min= %d\n",loc1.x,loc1.y,min1);

		//method1: buttom up at middle line to search for the min+EYE_THRESHOLD to locate eye
		for(j=0;j<eye_size;j++) { // y axis		
			int tmp = eye0sum->data.s[(eye_size-j-1)*eye0sum->cols + eye_size/2];
			int threshold;
			if (abs(loc0.x-eye_size/2)>10 || abs(loc0.y-eye_size+j+1)>10)
				threshold = EYE_THRESHOLD0;
			else
				threshold = EYE_THRESHOLD1;
			if(tmp<min0+threshold) {
				if( abs(loc0.x-eye_size/2)>10 || abs(loc0.y-eye_size+j+1)>10)
					min0 = tmp;		// update eye gray value
				loc0.y = eye_size-j-1;  // eye initial position
				loc0.x = eye_size/2;	// eye initial position
				break;
			}
		}

		for(j=0;j<eye_size;j++) { // y axis		
			int tmp = eye1sum->data.s[(eye_size-j-1)*eye1sum->cols + eye_size/2];
			int threshold;
			if (abs(loc1.x-eye_size/2)>10 || abs(loc1.y-eye_size+j+1)>10)
				threshold = EYE_THRESHOLD0;
			else
				threshold = EYE_THRESHOLD1;
			if(tmp<min1+threshold) {
				if( abs(loc1.x-eye_size/2)>10 || abs(loc1.y-eye_size+j+1)>10)
					min1 = tmp;		// update eye gray value
				loc1.y = eye_size-j-1;	// eye initial position
				loc1.x = eye_size/2;	// eye initial position
				break;
			}
		}

		if (abs(loc1.y-loc0.y)>7) {
			//method 2: buttom up search for the min+EYE_THRESHOLD to locate eye
			find = 0;
			for(j=0;j<eye_size;j++) { // y axis
				for(i=0;i<eye_size;i++) {  //x axis
					int tmp = eye0sum->data.s[(eye_size-j-1)*eye0sum->cols + i];
					int threshold;
					if (abs(loc0.x-i)>5 || abs(loc0.y-eye_size+j+1)>5)
						threshold = EYE_THRESHOLD0;  // if loc0 locate at brow, use large threshold
					else
						threshold = EYE_THRESHOLD1;
					if(tmp<min0+threshold) {
						if( abs(loc0.x-i)>5 || abs(loc0.y-eye_size+j+1)>5)
							min0 = tmp;		// update eye gray value to avoid brow with minimum value
						loc0.y = eye_size-j-1;	// eye initial position
						loc0.x = i;				// eye initial position

						find = 1;
						break;
					}
				}
				if(find) break;
			}

			find=0;
			for(j=0;j<eye_size;j++) {	
				for(i=0;i<eye_size;i++) {			
					int tmp = eye1sum->data.s[(eye_size-j-1)*eye1sum->cols + i];
					int threshold;
					if (abs(loc1.x-i)>5 || abs(loc1.y-eye_size+j+1)>5)
						threshold = EYE_THRESHOLD0;
					else
						threshold = EYE_THRESHOLD1;
					if(tmp<min1+threshold) {
						if( abs(loc1.x-i)>5 || abs(loc1.y-eye_size+j+1)>5)
							min1 = tmp;		// update eye gray value to avoid brow with minimum value
						loc1.y = eye_size-j-1;	// eye initial position
						loc1.x = i;				// eye initial position
						find = 1;
						break;
					}
				}
				if(find) break;
			}
		}

		if (abs(loc1.y-loc0.y)>7) {
			ret = 0;
			goto end;
		}

		//printf("eye0: (%d,%d), %d\n", loc0.x, loc0.y, eye0sum->data.s[loc0.y*eye0sum->cols + loc0.x]);
		//printf("eye1: (%d,%d), %d\n", loc1.x, loc1.y, eye1sum->data.s[loc1.y*eye1sum->cols + loc1.x]);

		// eye0
		// search x axis to find the middle
		i=j=0;
		while (loc0.x-i-1>0 && eye0sum->data.s[loc0.y*eye0sum->cols + loc0.x-i-1]<min0+EYE_THRESHOLD1)
			i++;
		while (loc0.x+j+1<eye_size-1 && eye0sum->data.s[loc0.y*eye0sum->cols + loc0.x+j+1]<min0+EYE_THRESHOLD1)
			j++;
		//printf("eye0: x+ = %d\n", (j-i)/2);
		loc0.x += (j-i)/2;

		// search y axis to find the middle
		i=j=0;
		while (loc0.y-i-1>0 && eye0sum->data.s[(loc0.y-i-1)*eye0sum->cols + loc0.x]<min0+EYE_THRESHOLD1)
			i++;
		while (loc0.y+j+1<eye_size-1 && eye0sum->data.s[(loc0.y+j+1)*eye0sum->cols + loc0.x]<min0+EYE_THRESHOLD1)
			j++;
		//printf("eye0: y+ = %d\n", (j-i)/2);
		loc0.y += (j-i)/2;

		// search x axis again to find the middle
		i=j=0;
		while (loc0.x-i-1>0 && eye0sum->data.s[loc0.y*eye0sum->cols + loc0.x-i-1]<min0+EYE_THRESHOLD1)
			i++;
		while (loc0.x+j+1<eye_size-1 && eye0sum->data.s[loc0.y*eye0sum->cols + loc0.x+j+1]<min0+EYE_THRESHOLD1)
			j++;
		//printf("eye0: x+ = %d\n", (j-i)/2);
		loc0.x += (j-i)/2;

		// eye1
		// search x axis to find the middle
		i=j=0;
		while (loc1.x-i-1>0 && eye1sum->data.s[loc1.y*eye1sum->cols + loc1.x-i-1]<min1+EYE_THRESHOLD1)
			i++;
		while (loc1.x+j+1<eye_size-1 && eye1sum->data.s[loc1.y*eye1sum->cols + loc1.x+j+1]<min1+EYE_THRESHOLD1)
			j++;
		//printf("eye1: x+ = %d\n", (j-i)/2);
		loc1.x += (j-i)/2;

		// search y axis to find the middle
		i=j=0;
		while (loc1.y-i-1>0 && eye1sum->data.s[(loc1.y-i-1)*eye1sum->cols + loc1.x]<min1+EYE_THRESHOLD1)
			i++;
		while (loc1.y+j+1<eye_size-1 && eye1sum->data.s[(loc1.y+j+1)*eye1sum->cols + loc1.x]<min1+EYE_THRESHOLD1)
			j++;
		//printf("eye1: y+ = %d\n", (j-i)/2);
		loc1.y += (j-i)/2;

		// search x axis again to find the middle
		i=j=0;
		while (loc1.x-i-1>0 && eye1sum->data.s[loc1.y*eye1sum->cols + loc1.x-i-1]<min1+EYE_THRESHOLD1)
			i++;
		while (loc1.x+j+1<eye_size-1 && eye1sum->data.s[loc1.y*eye1sum->cols + loc1.x+j+1]<min1+EYE_THRESHOLD1)
			j++;
		//printf("eye1: x+ = %d\n", (j-i)/2);
		loc1.x += (j-i)/2;

		if (abs(loc1.y-loc0.y)>7) {
			ret = 0;
			goto end;
		}

		cvSetImageROI(img, org_roi);
		eyes[0] = cvPoint(eye0rect.x+loc0.x-org_roi.x, eye0rect.y+loc0.y-org_roi.y);
		eyes[1] = cvPoint(eye1rect.x+loc1.x-org_roi.x, eye1rect.y+loc1.y-org_roi.y);

end:
		cvReleaseMat(&eye0sum);
		cvReleaseMat(&eye1sum);
		cvReleaseMat(&kernel);
  
	
	return ret;
}

IplImage* face_detect(IplImage* frameImg, CvRect *faceRect)
{
    IplImage *faceGray = 0, *dstGray = 0;
	CvPoint eyes[2];//記錄臉的兩個眼睛位置
	CvRect	rect;
	const CvScalar redcolor = CV_RGB(255,0,0);
	const CvScalar bluecolor = CV_RGB(0,0,255);
	int cx, cy, dx, dy, cnt, i = 0;
	float angle, eye_len, scale;
    double m[6];
    // [ m0  m1  m2 ] ===>  [ A11  A12   b1 ]
    // [ m3  m4  m5 ]       [ A21  A22   b2 ]
    CvMat M = cvMat(2, 3, CV_64FC1, m);

	cvShowImage("source", frameImg);
	//cvSetImageROI(frameImg, cvRect(FOCUS_X, FOCUS_Y, FOCUS_WIDTH, FOCUS_HEIGHT));
	cvSetImageROI(frameImg, frameImg->searchRangeRect);

    cnt = detectfaces(frameImg, &rect);
	if (!cnt) {
		//printf("face not found!\n");
		goto end;
	} else if (cnt>1) {
		printf("multi-faces found!\n");
		goto end;
	}

	cnt = detecteyes(frameImg, rect, eyes);
	if (!cnt) {
		printf("Head skew!\n");
		goto end;
	} else if (cnt==1) {
		printf("Only one eye found!\n");1;
		goto end;
	} else if (cnt>2) {
		printf("More than two eyes found!\n");
		goto end;
	}

    faceGray = cvCreateImage (cvSize(FOCUS_WIDTH, FOCUS_HEIGHT), IPL_DEPTH_8U, 1);
	dx = eyes[1].x-eyes[0].x;
	dy = eyes[1].y-eyes[0].y;
	angle = atan((float)dy/dx)*180.0/CV_PI;
	eye_len = sqrt((float)dx*dx + dy*dy);
	scale = (float)EYE_LENGTH/eye_len;
	cx = (eyes[0].x+eyes[1].x)/2;
	cy = (eyes[0].y+eyes[1].y)/2;
	cv2DRotationMatrix(cvPoint2D32f(cx, cy),angle,scale, &M);
	cvWarpAffine(frameImg, faceGray, &M);
	cvSetImageROI(faceGray, cvRect(cx-50, cy-30, FACE_SIZE, FACE_SIZE));
    dstGray = cvCreateImage (cvSize(FACE_SIZE, FACE_SIZE), IPL_DEPTH_8U, 1);
	cvCopy(faceGray, dstGray);

	faceRect->x = rect.x + FOCUS_X;
	faceRect->y = rect.y + FOCUS_Y;
	faceRect->width = rect.width;
	faceRect->height = rect.height;

	/*{
		CvRect *r = &rect;
		int eye_size = (r->width*EYE_SIZE)>>8;
		CvRect eye0rect = {r->x+((r->width*EYE0_X)>>8), r->y+((r->height*EYE0_Y)>>8), eye_size, eye_size};
		CvRect eye1rect = {r->x+((r->width*EYE1_X)>>8), r->y+((r->height*EYE1_Y)>>8), eye_size, eye_size};
		cvRectangle(frameImg, cvPoint(r->x, r->y), cvPoint(r->x+r->width, r->y+r->height), redcolor);
		cvRectangle(frameImg, cvPoint(eye0rect.x, eye0rect.y), cvPoint(eye0rect.x+eye0rect.width, eye0rect.y+eye0rect.height), redcolor);
		cvRectangle(frameImg, cvPoint(eye1rect.x, eye1rect.y), cvPoint(eye1rect.x+eye1rect.width, eye1rect.y+eye1rect.height), redcolor);

		for( i=0;i<2;i++) {
			cvRectangle(frameImg, cvPoint(eyes[i].x-1, eyes[i].y-1), 
				               cvPoint(eyes[i].x+1, eyes[i].y+1), bluecolor);
		}
		cvShowImage( "source", frameImg );
		cvShowImage( "result", dstGray );
	}*/

	//printf("Face detected at (%d,%d) size = %d\n", faceRect->x, faceRect->y, faceRect->width);
end:
	if (faceGray)
		cvReleaseImage(&faceGray);

    return dstGray;
}
