//////////////////////////////////////////////////////////////////////////////////////
// OnlineFaceRec.cpp, by powei
//////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#if defined WIN32 || defined _WIN32
	#include <conio.h>		// For _kbhit() on Windows
	#include <direct.h>		// For mkdir(path) on Windows
	#include <Windows.h>
	#define snprintf sprintf_s	// Visual Studio on Windows comes with sprintf_s() instead of snprintf()
#else
	#include <stdio.h>		// For getchar() on Linux
#endif
#include <vector>
#include <string>
#include "cv.h"
#include "cvaux.h"
#include "highgui.h"
#include "face_recognizer.h"

extern IplImage* face_detect(IplImage* frameImg, CvRect *faceRect);
extern void weber_filter(IplImage *srcImg, IplImage *dstImg);

//#define WEBER_FILTER_ENABLE
#define FISHERFACE_ENABLE
#define BINARY_FILESTORAGE

using namespace std;

// Haar Cascade file, used for Face Detection.
//const char *faceCascadeFilename = "opencv\\data\\harrcascades\\haarcascade_frontalface_alt.xml";

#ifdef BINARY_FILESTORAGE
const char *trainDataFilename = "traindata.ite";
const char *faceImgFilename = "faceimage.ite";
#else
const char *trainDataFilename = "traindata.xml";
const char *faceImgFilename = "faceimage.xml";
#endif

#define SAVE_EIGENFACE_IMAGES	1		// Set to 0 if you dont want images of the Eigenvectors saved to files (for debugging).
#define SAVE_TRAINFACE_IMAGES	1		// Set to 0 if you dont want images of the trained saved to files (for debugging).
//#define USE_MAHALANOBIS_DISTANCE	// You might get better recognition accuracy if you enable this.

static CvFaceRecognizer *recognizer = 0;

// Get an 8-bit equivalent of the 32-bit Float image.
// Returns a new image, so remember to call 'cvReleaseImage()' on the result.
static IplImage* convertFloatImageToUcharImage(const IplImage *srcImg)
{
	IplImage *dstImg = 0;
	if ((srcImg) && (srcImg->width > 0 && srcImg->height > 0)) {

		// Spread the 32bit floating point pixels to fit within 8bit pixel range.
		double minVal, maxVal;
		cvMinMaxLoc(srcImg, &minVal, &maxVal);

		//cout << "FloatImage:(minV=" << minVal << ", maxV=" << maxVal << ")." << endl;

		// Deal with NaN and extreme values, since the DFT seems to give some NaN results.
		if (cvIsNaN(minVal) || minVal < -1e30)
			minVal = -1e30;
		if (cvIsNaN(maxVal) || maxVal > 1e30)
			maxVal = 1e30;
		if (maxVal-minVal == 0.0f)
			maxVal = minVal + 0.001;	// remove potential divide by zero errors.

		// Convert the format
		dstImg = cvCreateImage(cvSize(srcImg->width, srcImg->height), 8, 1);
		cvConvertScale(srcImg, dstImg, 255.0 / (maxVal - minVal), - minVal * 255.0 / (maxVal-minVal));
	}
	return dstImg;
}

// Save all the eigenvectors as images, so that they can be checked.
static void saveEigenfaceImages(CvFaceRecognizer *recognizer)
{
	// Store the average image to a file
	printf("Saving the image of the average face as 'out_averageImage.bmp'.\n");
	cvSaveImage("out_averageImage.bmp", recognizer->pAvgTrainImg);

	// Create a large image made of many eigenface images.
	// Must also convert each eigenface image to a normal 8-bit UCHAR image instead of a 32-bit float image.
	printf("Saving the %d eigenvector images as 'out_eigenfaces.bmp'\n", recognizer->nEigens);
	if (recognizer->nEigens > 0) {
		// Put all the eigenfaces next to each other.
		int COLUMNS = 8;	// Put upto 8 images on a row.
		int nCols = min(recognizer->nEigens, COLUMNS);
		int nRows = 1 + (recognizer->nEigens / COLUMNS);	// Put the rest on new rows.
		int w = recognizer->eigenVectArr[0]->width;
		int h = recognizer->eigenVectArr[0]->height;
		CvSize size;
		size = cvSize(nCols * w, nRows * h);
		IplImage *bigImg = cvCreateImage(size, IPL_DEPTH_8U, 1);	// 8-bit Greyscale UCHAR image
		for (int i=0; i<recognizer->nEigens; i++) {
			// Get the eigenface image.
			IplImage *byteImg = convertFloatImageToUcharImage(recognizer->eigenVectArr[i]);
			// Paste it into the correct position.
			int x = w * (i % COLUMNS);
			int y = h * (i / COLUMNS);
			CvRect ROI = cvRect(x, y, w, h);
			cvSetImageROI(bigImg, ROI);
			cvCopyImage(byteImg, bigImg);
			cvResetImageROI(bigImg);
			cvReleaseImage(&byteImg);
		}
		cvSaveImage("out_eigenfaces.bmp", bigImg);
		cvReleaseImage(&bigImg);
	}
}

// Save all the eigenvectors as images, so that they can be checked.
static void saveTrainfaceImages(CvFaceRecognizer *recognizer)
{
	// Create a large image made of many eigenface images.
	// Must also convert each eigenface image to a normal 8-bit UCHAR image instead of a 32-bit float image.
	printf("Saving the %d reference images as 'out_trainfaces.bmp'\n", recognizer->nTrainFaces);
	if (recognizer->nTrainFaces > 0) {
		// Put all the eigenfaces next to each other.
		int COLUMNS = 8;	// Put upto 8 images on a row.
		int nCols = min(recognizer->nTrainFaces, COLUMNS);
		int nRows = 1 + (recognizer->nTrainFaces / COLUMNS);	// Put the rest on new rows.
		int w = recognizer->faceImgArr[0]->width;
		int h = recognizer->faceImgArr[0]->height;
		CvSize size;
		size = cvSize(nCols * w, nRows * h);
		IplImage *bigImg = cvCreateImage(size, IPL_DEPTH_8U, 1);	// 8-bit Greyscale UCHAR image
		for (int i=0; i<recognizer->nTrainFaces; i++) {
			// Paste it into the correct position.
			int x = w * (i % COLUMNS);
			int y = h * (i / COLUMNS);
			CvRect ROI = cvRect(x, y, w, h);
			cvSetImageROI(bigImg, ROI);
			cvCopyImage(recognizer->faceImgArr[i], bigImg);
			cvResetImageROI(bigImg);
		}
		cvSaveImage("out_trainfaces.bmp", bigImg);
		cvReleaseImage(&bigImg);
	}
}

static void releaseFaceImage(CvFaceRecognizer *recognizer)
{
	int i;
	// Release previous face image
	for(i=0; i<recognizer->nTrainFaces; i++)
		if (recognizer->faceImgArr && recognizer->faceImgArr[i])
			cvReleaseImage(&recognizer->faceImgArr[i]);
	if (recognizer->faceImgArr)
		cvFree(recognizer->faceImgArr);

	recognizer->faceImgArr = 0;
}

static void releasePersonNameAndFaceImage(CvFaceRecognizer *recognizer)
{
	int i;
	// Release previous face image
	for(i=0; i<recognizer->nTrainFaces; i++)
		if (recognizer->faceImgArr && recognizer->faceImgArr[i])
			cvReleaseImage(&recognizer->faceImgArr[i]);
	if (recognizer->faceImgArr)
		cvFree(recognizer->faceImgArr);

	for(i=0; i<recognizer->nTrainFaces; i++)
		if (recognizer->personNameArr && recognizer->personNameArr[i])
			cvFree(&recognizer->personNameArr[i]);
	if (recognizer->personNameArr)
		cvFree(recognizer->personNameArr);

	recognizer->nTrainFaces = 0;
	recognizer->personNameArr = 0;
	recognizer->faceImgArr = 0;
}

static void releaseNewFaceImage(CvFaceRecognizer *recognizer)
{
	newFaceData	*cur = recognizer->head;
	while (cur) {
		if (!cur->exist) {
			int i;
			for(i=0;i<PERSON_FACE_NUM;i++)
				cvFree(&cur->newFaceImgArr[i]);
		}
		cur = cur->next;
	}
	recognizer->head = NULL;
	recognizer->last = NULL;
	recognizer->nNewPerson = 0;

}

static void releaseTrainData(CvFaceRecognizer *recognizer)
{
	int i;
	// Release previous train data
	for(i=0; i<recognizer->nEigens; i++)
		if (recognizer->eigenVectArr && recognizer->eigenVectArr[i])
			cvReleaseImage(&recognizer->eigenVectArr[i]);
	if (recognizer->eigenVectArr)
		cvFree(recognizer->eigenVectArr);
	recognizer->eigenVectArr = 0;

	if (recognizer->ldaEigenVect)
		cvFree(&recognizer->ldaEigenVect);
	recognizer->ldaEigenVect = 0;
	if (recognizer->ldaEigenValMat)
		cvReleaseMat(&recognizer->ldaEigenValMat);
	recognizer->ldaEigenValMat = 0;

	if (recognizer->pAvgTrainImg)
		cvReleaseImage(&recognizer->pAvgTrainImg);
	recognizer->pAvgTrainImg = 0;

	if (recognizer->projectMat)
		cvReleaseMat(&recognizer->projectMat);
	recognizer->projectMat = 0;

//	if (recognizer->eigenValMat)
//		cvReleaseMat(&recognizer->eigenValMat);
//	recognizer->eigenValMat = 0;

	recognizer->nEigens = 0;
}

#ifdef BINARY_FILESTORAGE
static CvMat* freadCvMat(FILE * fileStorage)
{
	uchar *pdata;
	int step;
	CvSize size;
	int rows = 0, cols = 0, bytes;
	uchar type = 0;
	CvMat *mat;
	fread(&rows, sizeof(int), 1, fileStorage);
	fread(&cols, sizeof(int), 1, fileStorage);
	fread(&type, sizeof(uchar), 1, fileStorage);
	if (cols == 0 || rows == 0) {
		printf("Read CvMat error\n");
		return NULL;
	}

	if (type == 'i') {
		bytes = 4;
		mat = cvCreateMat( rows, cols, CV_32SC1 );
	} else if (type == 's') {
		bytes = 2;
		mat = cvCreateMat( rows, cols, CV_16SC1 );
	} else if (type == 'u') {
		bytes = 1;
		mat = cvCreateMat( rows, cols, CV_8UC1 );
	} else if (type == 'f') {
		bytes = 4;
		mat = cvCreateMat( rows, cols, CV_32FC1 );
	} else {
		printf("type error\n");
		return NULL;
	}

	cvGetRawData(mat, &pdata, &step, &size);
	fread(pdata, bytes, size.height*size.width, fileStorage);
	return mat;
}

static IplImage* freadCvImage(FILE * fileStorage)
{
	uchar *pdata;
	int step;
	CvSize size;
	int width = 0, height = 0, bytes;
	uchar type = 0;
	IplImage *img;
	fread(&width, sizeof(int), 1, fileStorage);
	fread(&height, sizeof(int), 1, fileStorage);
	fread(&type, sizeof(uchar), 1, fileStorage);
	if (width == 0 || height == 0) {
		printf("Read Image error\n");
		return NULL;
	}
	if (type == 'i') {
		bytes = 4;
		img = cvCreateImage(cvSize(width, height), IPL_DEPTH_32S, 1);  
	} else if (type == 's') {
		bytes = 2;
		img = cvCreateImage(cvSize(width, height), IPL_DEPTH_16S, 1);  
	} else if (type == 'u') {
		bytes = 1;
		img = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);  
	} else if (type == 'f') {
		bytes = 4;
		img = cvCreateImage(cvSize(width, height), IPL_DEPTH_32F, 1);  
	} else {
		printf("type error\n");
		return NULL;
	}
	cvGetImageRawData(img, &pdata, &step, &size);
	fread(pdata, bytes, size.height*size.width, fileStorage);
	return img;
}

// Open the training data from the file 'traindata.ite'.
static int loadTrainData(CvFaceRecognizer *recognizer, const char *filename)
{
	FILE * fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = fopen( filename, "rb" );
	if( !fileStorage ) {
		printf("Can't open training database file %s.\n", filename);
		return -1;
	}

	// Load the data
	fread(&recognizer->nEigens, sizeof(int), 1, fileStorage);
	//recognizer->eigenValMat = (CvMat *)freadCvMat(fileStorage); // s31
	recognizer->projectMat = (CvMat *)freadCvMat(fileStorage);  // s15.16
	recognizer->pAvgTrainImg = (IplImage *)freadCvImage(fileStorage);	//8
	recognizer->eigenVectArr = (IplImage **)cvAlloc(recognizer->nEigens*sizeof(IplImage *));
	for(i=0; i<recognizer->nEigens; i++)
		recognizer->eigenVectArr[i] = (IplImage *)freadCvImage(fileStorage);  //s.15

	fclose(fileStorage);
	printf("Loaded %d eigen faces\n", recognizer->nEigens);
	return 0;
}

static int loadPersonName(CvFaceRecognizer *recognizer, const char *filename)
{
	FILE *fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = fopen( filename, "rb" );
	if( !fileStorage ) {
		printf("No training database file %s.\n", filename);
		return 0;
	}

	fread(&recognizer->nTrainFaces, sizeof(int), 1, fileStorage);
	recognizer->personNameArr = (char **)cvAlloc(recognizer->nTrainFaces*sizeof(char *));
	for (i=0; i<recognizer->nTrainFaces; i++) {
		recognizer->personNameArr[i] = (char *)cvAlloc(PERSON_NAME_SIZE);
		fread(recognizer->personNameArr[i], 1, PERSON_NAME_SIZE, fileStorage);
	}
	
	fclose(fileStorage);
	return recognizer->nTrainFaces;
}

static int loadFaceImageAndName(CvFaceRecognizer *recognizer, const char *filename)
{
	FILE *fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = fopen( filename, "rb" );
	if( !fileStorage ) {
		printf("No training database file %s.\n", filename);
		return 0;
	}

	fread(&recognizer->nTrainFaces, sizeof(int), 1, fileStorage);
	recognizer->personNameArr = (char **)cvAlloc(recognizer->nTrainFaces*sizeof(char *));
	for (i=0; i<recognizer->nTrainFaces; i++) {
		recognizer->personNameArr[i] = (char *)cvAlloc(PERSON_NAME_SIZE);
		fread(recognizer->personNameArr[i], 1, PERSON_NAME_SIZE, fileStorage);
	}

	recognizer->faceImgArr = (IplImage **)cvAlloc( recognizer->nTrainFaces*sizeof(IplImage *) );
	for(i=0;i<recognizer->nTrainFaces;i++) {
		recognizer->faceImgArr[i] = (IplImage *)freadCvImage(fileStorage);
		if	(!recognizer->faceImgArr[i]) {
			releaseFaceImage(recognizer);
			fclose(fileStorage);
			return -1;
		}
	}
	
	fclose(fileStorage);
	printf("Loaded %d trained faces\n", recognizer->nTrainFaces);

	return recognizer->nTrainFaces;
}

static int fwriteCvMat(FILE *fileStorage, CvMat *mat)
{
	uchar *pdata;
	int step;
	CvSize size;
	int rows = 0, cols = 0, bytes;
	uchar c;

	if (CV_MAT_TYPE(mat->type) == CV_32SC1) {
		bytes = 4;
		c = 'i';
	} else if (CV_MAT_TYPE(mat->type) == CV_16SC1) {
		bytes = 2;
		c = 's';
	} else if (CV_MAT_TYPE(mat->type) == CV_8UC1) {
		bytes = 1;
		c = 'u';
	} else if (CV_MAT_TYPE(mat->type) == CV_32FC1) {
		bytes = 4;
		c = 'f';
	} else {
		printf("type error\n");
		return -1;
	}
	fwrite(&mat->rows, sizeof(int), 1, fileStorage);
	fwrite(&mat->cols, sizeof(int), 1, fileStorage);
	fwrite(&c, sizeof(uchar), 1, fileStorage);
	cvGetRawData(mat, &pdata, &step, &size);
	fwrite(pdata, bytes, size.height*size.width, fileStorage);
	return 0;
}

static int fwriteCvImage(FILE *fileStorage, IplImage *img)
{
	uchar *pdata;
	int step;
	CvSize size;
	int width = 0, height = 0, bytes;
	uchar c;

	if (img->depth == IPL_DEPTH_32S) {
		bytes = 4;
		c = 'i';
	} else if (img->depth == IPL_DEPTH_16S) {
		bytes = 2;
		c = 's';
	} else if (img->depth == IPL_DEPTH_8U) {
		bytes = 1;
		c = 'u';
	} else if (img->depth == IPL_DEPTH_32F) {
		bytes = 4;
		c = 'f';  
	} else {
		printf("type error\n");
		return -1;
	}

	fwrite(&img->width, sizeof(int), 1, fileStorage);
	fwrite(&img->height, sizeof(int), 1, fileStorage);
	fwrite(&c, sizeof(uchar), 1, fileStorage);
	cvGetImageRawData(img, &pdata, &step, &size);
	fwrite(pdata, bytes, size.height*size.width, fileStorage);
	return 0;
}

// Save the training data to the file 'facedata.xml'.
static int saveTrainData(CvFaceRecognizer *recognizer, const char *filename)
{
	FILE *fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = fopen(filename, "wb" );
	if( !fileStorage ) {
		printf("Can't open training database file %s.\n", filename);
		return -1;
	}

	fwrite(&recognizer->nEigens, sizeof(int), 1, fileStorage);
	//fwriteCvMat(fileStorage, recognizer->eigenValMat);
	fwriteCvMat(fileStorage, recognizer->projectMat);
	fwriteCvImage(fileStorage, recognizer->pAvgTrainImg);
	for(i=0; i<recognizer->nEigens; i++)
		fwriteCvImage(fileStorage, recognizer->eigenVectArr[i]);

	fclose(fileStorage);
	//printf("Store %d training images\n", recognizer->nTrainFaces);

	return 0;
}

static int saveFaceImageAndName(CvFaceRecognizer *recognizer, const char *filename)
{
	FILE *fileStorage;
	int i, j, cnt;
	newFaceData	*cur;

	// create a file-storage interface
	fileStorage = fopen( filename, "wb" );
	if( !fileStorage ) {
		printf("Can't open training database file %s.\n", filename);
		return -1;
	}

	cur = recognizer->head;
	while (cur) {
		for(i=0;i<recognizer->nTrainFaces;i++) {
			if(!strcmp(recognizer->personNameArr[i], cur->newPersonName))
				break;
		}

		if (i<recognizer->nTrainFaces) { // replace person
			for (j=0;j<PERSON_FACE_NUM;j++) {
				cvReleaseImage(&recognizer->faceImgArr[i+j]);
				recognizer->faceImgArr[i+j] = cur->newFaceImgArr[j];
			}
			cur->exist = 1;
			recognizer->nNewPerson--;
		}
		cur = cur->next;
	}

	cnt = recognizer->nTrainFaces + recognizer->nNewPerson*PERSON_FACE_NUM;
	fwrite(&cnt, sizeof(int), 1, fileStorage);
	for (i=0; i<recognizer->nTrainFaces; i++)
		fwrite(recognizer->personNameArr[i], 1, PERSON_NAME_SIZE, fileStorage);

	cur = recognizer->head;
	while (cur) {
		if (!cur->exist)
			for (j=0;j<PERSON_FACE_NUM;j++)
				fwrite(cur->newPersonName, 1, PERSON_NAME_SIZE, fileStorage);
		cur = cur->next;
	}

	for(i=0;i<recognizer->nTrainFaces;i++)
		fwriteCvImage(fileStorage, recognizer->faceImgArr[i]);

	cur = recognizer->head;
	while (cur) {
		if (!cur->exist)
			for (j=0;j<PERSON_FACE_NUM;j++)
				fwriteCvImage(fileStorage, cur->newFaceImgArr[j]);
		cur = cur->next;
	}

	fclose(fileStorage);
	return 0;
}

#else
// Open the training data from the file 'facedata.xml'.
static int loadTrainData(CvFaceRecognizer *recognizer, const char *filename)
{
	CvFileStorage * fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = cvOpenFileStorage( filename, 0, CV_STORAGE_READ );
	if( !fileStorage ) {
		printf("Can't open training database file %s.\n", filename);
		return -1;
	}

	// Load the data
	recognizer->nEigens = cvReadIntByName(fileStorage, 0, "nEigens", 0);
	//recognizer->eigenValMat  = (CvMat *)cvReadByName(fileStorage, 0, "eigenValMat", 0);
	recognizer->projectMat = (CvMat *)cvReadByName(fileStorage, 0, "projectMat", 0);
	recognizer->pAvgTrainImg = (IplImage *)cvReadByName(fileStorage, 0, "avgTrainImg", 0);
	recognizer->eigenVectArr = (IplImage **)cvAlloc(recognizer->nEigens*sizeof(IplImage *));
	for(i=0; i<recognizer->nEigens; i++)
	{
		char varname[200];
		snprintf( varname, sizeof(varname)-1, "eigenVect_%d", i );
		recognizer->eigenVectArr[i] = (IplImage *)cvReadByName(fileStorage, 0, varname, 0);
	}

	// release the file-storage interface
	cvReleaseFileStorage( &fileStorage );
	//printf("Training data loaded (%d eigenfaces):\n", recognizer->nEigens);
	return 0;
}

static int loadPersonName(CvFaceRecognizer *recognizer, const char *filename)
{
	CvFileStorage * fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = cvOpenFileStorage( filename, 0, CV_STORAGE_READ );
	if( !fileStorage ) {
		printf("Can't open training database file %s.\n", filename);
		return 0;
	}

	recognizer->nTrainFaces = cvReadIntByName(fileStorage, 0, "nTrainFaces", 0);
	recognizer->personNameArr = (char **)cvAlloc(recognizer->nTrainFaces*sizeof(char *));

	// Load each person's name.
	for (i=0; i<recognizer->nTrainFaces; i++) {
		string sPersonName;
		char varname[200];
		snprintf( varname, sizeof(varname)-1, "personName_%d", (i+1) );
		sPersonName = cvReadStringByName(fileStorage, 0, varname );
		recognizer->personNameArr[i] = (char *)cvAlloc(PERSON_NAME_SIZE);
		strcpy(recognizer->personNameArr[i], sPersonName.c_str());
	}
	// release the file-storage interface
	cvReleaseFileStorage( &fileStorage );

	return recognizer->nTrainFaces;
}

static int loadFaceImageAndName(CvFaceRecognizer *recognizer, const char *filename)
{
	CvFileStorage * fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = cvOpenFileStorage( filename, 0, CV_STORAGE_READ );
	if( !fileStorage ) {
		printf("Can't open training database file %s.\n", filename);
		return 0;
	}

	recognizer->nTrainFaces = cvReadIntByName(fileStorage, 0, "nTrainFaces", 0);
	recognizer->personNameArr = (char **)cvAlloc(recognizer->nTrainFaces*sizeof(char *));

	// Load each person's name.
	for (i=0; i<recognizer->nTrainFaces; i++) {
		string sPersonName;
		char varname[200];
		snprintf( varname, sizeof(varname)-1, "personName_%d", (i+1) );
		sPersonName = cvReadStringByName(fileStorage, 0, varname );
		recognizer->personNameArr[i] = (char *)cvAlloc(PERSON_NAME_SIZE);
		strcpy(recognizer->personNameArr[i], sPersonName.c_str());
	}

	recognizer->faceImgArr = (IplImage **)cvAlloc( recognizer->nTrainFaces*sizeof(IplImage *) );
	for (i=0; i<recognizer->nTrainFaces; i++) {
		char varname[200];
		snprintf( varname, sizeof(varname)-1, "faceImg_%d", i );
		recognizer->faceImgArr[i] = (IplImage *)cvReadByName(fileStorage, 0, varname, 0);
	}

	// release the file-storage interface
	cvReleaseFileStorage( &fileStorage );
	//printf("Training data loaded (%d training people):\n", recognizer->nTrainFaces);

	return recognizer->nTrainFaces;
}

// Save the training data to the file 'facedata.xml'.
static int saveTrainData(CvFaceRecognizer *recognizer, const char *filename)
{
	CvFileStorage * fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = cvOpenFileStorage( filename, 0, CV_STORAGE_WRITE );

	// store all the data
	cvWriteInt( fileStorage, "nEigens", recognizer->nEigens );
	//cvWrite(fileStorage, "eigenValMat", recognizer->eigenValMat, cvAttrList(0,0));
	cvWrite(fileStorage, "projectMat", recognizer->projectMat, cvAttrList(0,0));
	cvWrite(fileStorage, "avgTrainImg", recognizer->pAvgTrainImg, cvAttrList(0,0));
	for(i=0; i<recognizer->nEigens; i++)
	{
		char varname[200];
		snprintf( varname, sizeof(varname)-1, "eigenVect_%d", i );
		cvWrite(fileStorage, varname, recognizer->eigenVectArr[i], cvAttrList(0,0));
	}

	// release the file-storage interface
	cvReleaseFileStorage( &fileStorage );
	return 0;
}

static int saveFaceImageAndName(CvFaceRecognizer *recognizer, const char *filename)
{
	CvFileStorage * fileStorage;
	int i,j;
	newFaceData	*cur;

	// create a file-storage interface
	fileStorage = cvOpenFileStorage( filename, 0, CV_STORAGE_WRITE );

	cur = recognizer->head;
	while (cur) {
		for(i=0;i<recognizer->nTrainFaces;i++) {
			if(!strcmp(recognizer->personNameArr[i], cur->newPersonName))
				break;
		}

		if (i<recognizer->nTrainFaces) { // replace person
			for (j=0;j<PERSON_FACE_NUM;j++) {
				cvReleaseImage(&recognizer->faceImgArr[i+j]);
				recognizer->faceImgArr[i+j] = cur->newFaceImgArr[j];
			}
			cur->exist = 1;
			recognizer->nNewPerson--;
		}
		cur = cur->next;
	}

	cvWriteInt( fileStorage, "nTrainFaces", recognizer->nTrainFaces + recognizer->nNewPerson*PERSON_FACE_NUM );

	for (i=0; i<recognizer->nTrainFaces; i++) {
		char varname[200];
		snprintf( varname, sizeof(varname)-1, "personName_%d", (i+1) );
		cvWriteString(fileStorage, varname, recognizer->personNameArr[i], 0);
	}

	cur = recognizer->head;
	while(cur) {
		if (!cur->exist) {
			char varname[200];
			for (j=0;j<PERSON_FACE_NUM;j++) {
				snprintf( varname, sizeof(varname)-1, "personName_%d", (i+1) );
				cvWriteString(fileStorage, varname, cur->newPersonName, 0);
				i++;
			}
		}
		cur = cur->next;
	}

	for (i=0; i<recognizer->nTrainFaces; i++) {
		char varname[200];
		snprintf( varname, sizeof(varname)-1, "faceImg_%d", i );
		cvWrite(fileStorage, varname, recognizer->faceImgArr[i], cvAttrList(0,0));
	}

	cur = recognizer->head;
	while(cur) {
		if (!cur->exist) {
			char varname[200];
			for (j=0;j<PERSON_FACE_NUM;j++) {
				snprintf( varname, sizeof(varname)-1, "faceImg_%d", i );
				cvWrite(fileStorage, varname, cur->newFaceImgArr[j], cvAttrList(0,0));
				i++;
			}
		}
		cur = cur->next;
	}

	// release the file-storage interface
	cvReleaseFileStorage( &fileStorage );
	return 0;
}

#endif

// Find the most likely person based on a detection. Returns the index, and stores the confidence value into pConfidence.
static int findNearestNeighbor(CvFaceRecognizer *recognizer, int * projectedTestFace, int *pDifference)
{
	//double leastDistSq = 1e12;
	int i, iTrain, iNearest = -1;
	int threshold_high = RECOGNIZED_HIGH * recognizer->nEigens;
	int threshold_low = RECOGNIZED_LOW * recognizer->nEigens;
	int leastDistSq = threshold_high;

	for(iTrain=0; iTrain<recognizer->nTrainFaces; iTrain++)
	{
		int distSq=0;

		for(i=0; i<recognizer->nEigens; i++)
		{
			int d_i = projectedTestFace[i] - recognizer->projectMat->data.i[iTrain*recognizer->nEigens + i];
#ifdef USE_MAHALANOBIS_DISTANCE
			distSq += d_i*d_i;// / eigenValMat->data.fl[i];  // Mahalanobis distance (might give better results than Eucalidean distance)
#else
			distSq += d_i*d_i; // Euclidean distance.
#endif
			if (distSq > threshold_high)
				break;
		}

		if (distSq < threshold_high)
			printf("<%s> %d\n", recognizer->personNameArr[iTrain], distSq / recognizer->nEigens);

		if(distSq < leastDistSq)
		{
			leastDistSq = distSq;
			iNearest = iTrain;
		}
		if(leastDistSq < threshold_low)
			break;
	}

	*pDifference = leastDistSq / recognizer->nEigens;

	// Return the found index.
	return iNearest;
}

// Do the Principal Component Analysis, finding the average image
// and the eigenfaces that represent any image in the given dataset.
static void doPCA(CvFaceRecognizer *recognizer, int numComponents)
{
	int i;
	CvTermCriteria calcLimit;
	CvSize faceImgSize;

	// set the number of eigenvalues to use

	recognizer->nEigens = numComponents;

	// allocate the eigenvector images
	faceImgSize.width  = recognizer->faceImgArr[0]->width;
	faceImgSize.height = recognizer->faceImgArr[0]->height;
	recognizer->eigenVectArr = (IplImage**)cvAlloc(sizeof(IplImage*) * recognizer->nEigens);
	for(i=0; i<recognizer->nEigens; i++)
		recognizer->eigenVectArr[i] = cvCreateImage(faceImgSize, IPL_DEPTH_16S, 1);   //s.15

	// allocate the eigenvalue array
	//recognizer->eigenValMat = cvCreateMat( 1, recognizer->nTrainfaces, CV_32SC1 );

	// allocate the averaged image
	recognizer->pAvgTrainImg = cvCreateImage(faceImgSize, IPL_DEPTH_8U, 1);  

	// set the PCA termination criterion
	calcLimit = cvTermCriteria( CV_TERMCRIT_ITER, recognizer->nEigens, 1);

	// compute average image, eigenvalues, and eigenvectors
	cvCalcEigenObjects(
		recognizer->nTrainFaces,
		(void*)recognizer->faceImgArr,
		(void*)recognizer->eigenVectArr,
		CV_EIGOBJ_NO_CALLBACK,
		0,
		0,
		&calcLimit,
		recognizer->pAvgTrainImg,
		NULL);//recognizer->eigenValMat->data.i);

	//cvNormalize(recognizer->eigenValMat, recognizer->eigenValMat, 1, 0, CV_L1, 0);

}

static void doProject(CvFaceRecognizer *recognizer)
{
	int i, offset;
	// project the training images onto the PCA subspace
	recognizer->projectMat = cvCreateMat( recognizer->nTrainFaces, recognizer->nEigens, CV_32SC1 ); //s31
	offset = recognizer->projectMat->step / sizeof(float);
	for(i=0; i<recognizer->nTrainFaces; i++)
	{
		//int offset = i * nEigens;
		cvEigenDecomposite(
			recognizer->faceImgArr[i],
			recognizer->nEigens,
			recognizer->eigenVectArr,
			0, 0,
			recognizer->pAvgTrainImg,
			recognizer->projectMat->data.i + i*offset);
	}

}

static void doLDA(CvFaceRecognizer *recognizer, int numComponents)
{
	int i,j;
	CvMat *src = recognizer->projectMat;
	int N = src->rows;
	int D = src->cols;
	int C = N - D;
    CvMat *meanTotal;
	CvMat **meanClass;
	CvMat *Sw, *Sb, *Swi, *M;
	CvMat *evector, *eval;
	CvMat row;

	// calculate total mean and class mean
	meanTotal = cvCreateMat( 1, D, CV_32SC1 );
	cvSetZero(meanTotal);
	meanClass = (CvMat **)cvAlloc(C * sizeof (CvMat *));
	for (i=0;i<C;i++) {
		meanClass[i] = cvCreateMat( 1, D, CV_32SC1 );
		cvSetZero(meanClass[i]);
	}

    // calculate sums
    for (i = 0; i < N; i++) {
        int classIdx = i/PERSON_FACE_NUM;
		cvGetRow(src, &row, i);
        cvAdd(meanTotal, &row, meanTotal);
        cvAdd(meanClass[classIdx], &row, meanClass[classIdx]);
    }
	
    // calculate total mean
	cvConvertScale(meanTotal, meanTotal, 1.0/N, 0);
    // calculate class means
    for (i = 0; i < C; i++) 
        cvConvertScale(meanClass[i], meanClass[i], 1.0 / PERSON_FACE_NUM, 0);

    // subtract class means
    for (i = 0; i < N; i++) {
        int classIdx = i/PERSON_FACE_NUM;
		cvGetRow(src, &row, i);
        cvSub(&row, meanClass[classIdx], &row);
    }

    // calculate within-classes scatter
    Sw = cvCreateMat( D, D, CV_32FC1 );
	cvSetZero(Sw);	
    cvMulTransposed(src, Sw, 1);

    // calculate between-classes scatter
    Sb = cvCreateMat( D, D, CV_32FC1 );
	cvSetZero(Sb);	
    
	CvMat *tmp = cvCreateMat( D, D, CV_32FC1 );
    for (i = 0; i < C; i++) {
        cvSub(meanClass[i], meanTotal, meanClass[i]);
        cvMulTransposed(meanClass[i], tmp, 1);
        cvAdd(Sb, tmp, Sb);
    }
	cvReleaseMat(&tmp);

    // invert Sw
    Swi = cvCreateMat( D, D, CV_32FC1 );
    cvInvert(Sw, Swi);

    // M = inv(Sw)*Sb
    M = cvCreateMat( D, D, CV_32FC1 );
	cvGEMM(Swi, Sb, 1.0, NULL, 0.0, M);

	evector = cvCreateMat( D, D, CV_32FC1 );
	eval = cvCreateMat( 1, D, CV_32FC1 );

	cvEigenVV(M, evector, eval, 0.01);
/*
	printf("Swi=\n");
	for(i=0;i<D;i++) {
		for(j=0;j<D;j++) 
			printf("%f, ", Swi->data.fl[i*M->cols+j]);
		printf("\n");
	}
	printf("Sw=\n");
	for(i=0;i<D;i++) {
		for(j=0;j<D;j++) 
			printf("%.1f, ", Sw->data.fl[i*M->cols+j]);
		printf("\n");
	}
	printf("Sb=\n");
	for(i=0;i<D;i++) {
		for(j=0;j<D;j++) 
			printf("%.1f, ", Sb->data.fl[i*M->cols+j]);
		printf("\n");
	}
	printf("M=\n");
	for(i=0;i<D;i++) {
		for(j=0;j<D;j++) 
			printf("%f, ", M->data.fl[i*M->cols+j]);
		printf("\n");
	}
	printf("lda eigen vector=\n");
	for(i=0;i<D;i++) {
		for(j=0;j<D;j++) 
			printf("%.2f, ", evector->data.fl[i*M->cols+j]);
		printf("\n");
	}
	printf("lda eigen value=\n");
	for(i=0;i<D;i++)
		printf("%.2f, ", eval->data.fl[i]);
	printf("\n");
*/
	cvReleaseMat(&M);
	cvReleaseMat(&Swi);
	cvReleaseMat(&Sb);
	cvReleaseMat(&Sw);
	for (i=0;i<C;i++)
		cvReleaseMat(&meanClass[i]);
	cvFree(meanClass);
	cvReleaseMat(&meanTotal);

	recognizer->ldaEigenVect = evector;
	recognizer->ldaEigenValMat = eval;

}

void imgGEMM(CvMat *mat, int nSrcEigens, IplImage **srcImgArr, int nDstEigens, IplImage **dstImgArr)
{
	int c,i,j,k,n;
	short *D, *S;
	float *M;
	int width = srcImgArr[0]->width;
	int height = srcImgArr[0]->height;
	printf("mat rows = %d, cols = %d\n", mat->rows, mat->cols);
	if (mat->cols != nSrcEigens)
		printf("mat cols size mismatch with image array number\n");
	if (mat->rows < nDstEigens)
		printf("mat rows small than dst eigen number\n");

	M = mat->data.fl;
/*
	for (c=0;c<nDstEigens;c++) {
		for (i=0;i<height;i++) {
			for (j=0;j<width;j++) {
				D = (float *)dstImgArr[c]->imageData;
				D[i*width+j] = 0.;
				for (k=0;k<nSrcEigens;k++) {
					S = (float *)srcImgArr[k]->imageData;
					D[i*width+j] += M[c*mat->cols+k]*S[i*width+j];
				}
			}
		}
	}
*/
	for (c=0;c<nDstEigens;c++,M+=nSrcEigens) {
		D = (short *)dstImgArr[c]->imageData;
		for (i=0;i<height;i++) {
			for (j=0;j<width;j++) {
				int offset = i*width+j;
				D[offset] = 0.;
				for (k=0;k<nSrcEigens;k++) {
					S = (short *)srcImgArr[k]->imageData;
					D[offset] += M[k]*S[offset];
				}
			}
		}
	}
}

IplImage* cvFaceDetect(IplImage* frameImg)
{
	IplImage* faceImg = face_detect(frameImg, &recognizer->faceRect);
	if (faceImg)
		recognizer->faceDetected = 1;
	else {
		recognizer->faceDetected = 0;
		recognizer->faceRecognized = 0;
	}

	return faceImg;
}

int cvFaceStatus(CvRect *faceRect, char *faceId)
{
	if (recognizer) {
		if (recognizer->faceRecognized) {
			faceRect->x = recognizer->faceRect.x;
			faceRect->y = recognizer->faceRect.y;
			faceRect->width = recognizer->faceRect.width;
			faceRect->height = recognizer->faceRect.height;
			strcpy(faceId, recognizer->faceId);
			return 2;
		} else if (recognizer->faceDetected) {
			faceRect->x = recognizer->faceRect.x;
			faceRect->y = recognizer->faceRect.y;
			faceRect->width = recognizer->faceRect.width;
			faceRect->height = recognizer->faceRect.height;
			return 1;
		}
	}
	return 0;
}

void cvFaceInit()
{
	recognizer = (CvFaceRecognizer *)malloc(sizeof(CvFaceRecognizer));
	recognizer->faceDetected = 0;
	recognizer->faceRecognized = 0;
	recognizer->faceRect.x = 0;
	recognizer->faceRect.y = 0;
	recognizer->faceRect.width = 0;
	recognizer->faceRect.height = 0;
	recognizer->faceId = 0;

	recognizer->faceImgArr = 0;
	recognizer->personNameArr = 0;
	recognizer->nTrainFaces = 0;
	recognizer->nEigens = 0;
	recognizer->pAvgTrainImg = 0;
	recognizer->eigenVectArr = 0;
	//recognizer->eigenValMat = 0;
	recognizer->ldaEigenVect = 0;
	recognizer->ldaEigenValMat = 0;
	recognizer->projectMat = 0;
	recognizer->dirty = 1;
	recognizer->nNewPerson = 0;
	recognizer->head = 0;
	recognizer->last = 0;

	loadPersonName(recognizer, faceImgFilename);
	loadTrainData(recognizer, trainDataFilename);
	recognizer ->dirty = 0;

}

void cvFaceUnInit()
{
	releaseTrainData(recognizer);
	releasePersonNameAndFaceImage(recognizer);
	free(recognizer);
	recognizer = 0;
}

// Read the names & image filenames of people from a text file, and load all those images listed.
int cvFaceAdd(char *faceID, int face_num, IplImage** faceImgArr)
{
	newFaceData	*cur;
	IplImage* weberImg;
	int i;

	cur = (newFaceData*)cvAlloc( sizeof(newFaceData) );
	if (cur == NULL)
		return -1;
	cur->next = NULL;
	cur->exist = 0;
	strcpy(cur->newPersonName, faceID);

#ifdef WEBER_FILTER_ENABLE
	weberImg = cvCreateImage (cvSize(faceImg->width, faceImg->height), IPL_DEPTH_8U, 1);
	weber_filter(faceImg, weberImg);
	cur->newFaceImg = weberImg;//cvCloneImage(faceImg);
#else
	for (i=0;i<PERSON_FACE_NUM;i++)
		cur->newFaceImgArr[i] = faceImgArr[i];
#endif
	if (recognizer->head == NULL)
		recognizer->head = cur;
	else
		recognizer->last->next = cur;
	recognizer->last = cur;
	recognizer->nNewPerson += 1;

	recognizer->dirty = 1;

	return 0;
}

// Train from the data in the given text file, and store the trained data into the file 'facedata.xml'.
int cvFaceTrain()
{
	int i;
	int nClasses;
	int numComponents = 0;
	CvSize fs;
	IplImage** tmpImgArr;
	
	//if(recognizer->dirty == 0)
	//	return 0;

	releaseTrainData(recognizer);
	releasePersonNameAndFaceImage(recognizer);
	if (loadFaceImageAndName(recognizer, faceImgFilename)<0) {
		printf("learn0: loadFaceImage fail\n");
		return -1;
	}
	saveFaceImageAndName(recognizer, faceImgFilename);
	releasePersonNameAndFaceImage(recognizer);
	releaseNewFaceImage(recognizer);

	if (loadFaceImageAndName(recognizer, faceImgFilename)<0) {
		printf("learn1: loadFaceImage fail\n");
		return -1;
	}


	if (recognizer->nTrainFaces<2) {
		printf( "Need 2 or more training faces\n"
		        "Input file contains only %d\n", recognizer->nTrainFaces);
		return 0;
	}

	// do PCA on the training faces
#ifdef FISHERFACE_ENABLE
	nClasses = recognizer->nTrainFaces / PERSON_FACE_NUM;
	if (nClasses==1) {
		doPCA(recognizer, recognizer->nTrainFaces-1);
		doProject(recognizer);
	} else {
		doPCA(recognizer, recognizer->nTrainFaces - nClasses);
		doProject(recognizer);
		numComponents = (nClasses-1<MAX_COMPONENTS) ? nClasses-1 : MAX_COMPONENTS;
		doLDA(recognizer, numComponents);

		tmpImgArr = (IplImage**)cvAlloc(sizeof(IplImage*) * numComponents);
		fs.width = recognizer->faceImgArr[0]->width;
		fs.height = recognizer->faceImgArr[0]->height;
		for(i=0; i<numComponents; i++)
			tmpImgArr[i] = cvCreateImage(fs, IPL_DEPTH_16S, 1);   //s.15

		imgGEMM(recognizer->ldaEigenVect, 
				recognizer->nEigens,
				recognizer->eigenVectArr, 
				numComponents,
				tmpImgArr);

		for(i=0; i<recognizer->nEigens; i++)
			if (recognizer->eigenVectArr && recognizer->eigenVectArr[i])
				cvReleaseImage(&recognizer->eigenVectArr[i]);
		if (recognizer->eigenVectArr)
			cvFree(recognizer->eigenVectArr);
		if (recognizer->projectMat)
			cvReleaseMat(&recognizer->projectMat);
		recognizer->projectMat = 0;

		if (recognizer->ldaEigenVect)
			cvFree(&recognizer->ldaEigenVect);
		recognizer->ldaEigenVect = 0;
		if (recognizer->ldaEigenValMat)
			cvFree(&recognizer->ldaEigenValMat);
		recognizer->ldaEigenValMat = 0;

		recognizer->nEigens = numComponents;
		recognizer->eigenVectArr = tmpImgArr;

		doProject(recognizer);
	}
#else
	numComponents = (recognizer->nTrainFaces-1<MAX_COMPONENTS) ? recognizer->nTrainFaces-1 : MAX_COMPONENTS;
	doPCA(recognizer, numComponents);
	doProject(recognizer);
#endif
	// store the recognition data as an xml file
	saveTrainData(recognizer, trainDataFilename);

	// Save all the eigenvectors as images, so that they can be checked.
	if (SAVE_EIGENFACE_IMAGES) {
		saveEigenfaceImages(recognizer);
	}
	if (SAVE_TRAINFACE_IMAGES) {
		saveTrainfaceImages(recognizer);
	}
	recognizer->dirty = 0;
	releaseFaceImage(recognizer);
	return 0;
}

// Recognize the face in each of the test images given, and compare the results with the truth.
int cvFaceRecognize(IplImage* faceImg)
{
	CvMat * trainPersonNumMat = 0;  // the person numbers during training
	int * projectedTestFace = 0;
	int nCorrect = 0;
	int nWrong = 0;
	uint64 t1, t2, t3, t4;
	int difference = 0;
	int iNearest = -1;
	int ret = 0;
	CvRect	faceRect = {0};
	IplImage* weberImg = 0;

	if (recognizer->dirty==1) {
		printf("database need to be generated again.\n");
		goto end;
	}
	
#ifdef WEBER_FILTER_ENABLE
	weberImg = cvCreateImage (cvSize(faceImg->width, faceImg->height), IPL_DEPTH_8U, 1);
	weber_filter(faceImg, weberImg);
    cvShowImage( "project", weberImg );
#endif

	// project the test images onto the PCA subspace
	projectedTestFace = (int *)cvAlloc( recognizer->nEigens*sizeof(int) );

	// project the test image onto the PCA subspace
	cvEigenDecomposite(
#ifdef WEBER_FILTER_ENABLE
			weberImg,
#else
			faceImg,
#endif
			recognizer->nEigens,
			recognizer->eigenVectArr,
			0, 0,
			recognizer->pAvgTrainImg,
			projectedTestFace);

	iNearest = findNearestNeighbor(recognizer, projectedTestFace, &difference);
	if (iNearest<0) {
		recognizer->faceRecognized = 0;
		recognizer->faceId = 0;
	} else {
		recognizer->faceRecognized = 1;
		recognizer->faceId = recognizer->personNameArr[iNearest];
	}

	/*{
	IplImage *projectImg = 0;
	projectImg = cvCreateImage(cvSize(recognizer->pAvgTrainImg->width, recognizer->pAvgTrainImg->height), IPL_DEPTH_8U, 1);
	cvEigenProjection( 
				recognizer->eigenVectArr,
				recognizer->nEigens,
                0, 0,
                projectedTestFace, 
                recognizer->pAvgTrainImg,
				projectImg);

    cvShowImage( "project", projectImg );
	cvReleaseImage(&projectImg);
	}*/

end:
	return iNearest;
}
