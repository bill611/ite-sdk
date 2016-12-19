
#ifndef __FACE_RECOGNIZER__H__
#define __FACE_RECOGNIZER__H__

#include <cv.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECOGNIZED_HIGH			50000
#define RECOGNIZED_LOW			30000
#define PERSON_NAME_SIZE		64
#define MAX_COMPONENTS			50
#define PERSON_FACE_NUM			3

struct _newFaceData {
	char			newPersonName[64];
	IplImage		*newFaceImgArr[PERSON_FACE_NUM];
	int				exist;
	struct _newFaceData	*next;
};

typedef struct _newFaceData	newFaceData;

typedef struct _CvFaceRecognizer {
	int				faceDetected;
	int				faceRecognized;
	CvRect			faceRect;
	char			*faceId;
	int				nTrainFaces;			// the number of training images
	char			**personNameArr;		// array of person names (indexed by the person number). Added by Shervin.
	IplImage		**faceImgArr;			// array of face images
	IplImage		*pAvgTrainImg;			// the average image // U8 8 format
	int				nEigens;				// the number of eigenvalues
	IplImage		**eigenVectArr;			// eigenvectors
	//CvMat			*eigenValMat;			// eigenvalues
	CvMat			*ldaEigenVect;			  // ldaEigenvectors
	CvMat			*ldaEigenValMat;			// ldaEigenvalues
	CvMat			*projectMat;			// projected training faces
	int				nNewPerson;
	newFaceData	*head;
	newFaceData	*last;
	int				dirty;
} CvFaceRecognizer;

/****************************************************************************************\
*                                  face detect                                         *
\****************************************************************************************/
IplImage* cvFaceDetect(IplImage* frameImg);

int cvFaceStatus(CvRect *faceRect, char *faceId); 

void cvFaceInit();

void cvFaceUnInit();

int cvFaceAdd(char *faceID, int face_num, IplImage** faceImgArr);

int cvFaceTrain();

int cvFaceRecognize(IplImage* frameImg);

#ifdef __cplusplus
}
#endif

#endif

/* End of file. */
