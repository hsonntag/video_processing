
/**
 * UTF-16 character type
 */
//#ifndef __STDC_UTF_16__
//#    ifndef CHAR16_T
//#        define CHAR16_T unsigned short
//typedef CHAR16_T char16_t;
//#    endif
//#endif

#include "cv.h"
#include "highgui.h"
#ifdef MATLAB_MEX_FILE
#include "mex.h"
#else
#include <stdbool.h>
#include <stdio.h>
#endif

static const double pi = 3.14159265358979323846;



inline static double square(int a)

{

    return a * a;

}

inline static void allocateOnDemand( IplImage **img, CvSize size, int depth, int channels )

{

    if ( *img != NULL ) return;



    *img = cvCreateImage( size, depth, channels );

    if ( *img == NULL )

    {

        fprintf(stderr, "Error: Couldn't allocate image.  Out of memory?\n");

        exit(-1);

    }

}


//######################################################
//##############--main mex function--###################
//######################################################
#ifdef MATLAB_MEX_FILE
void mexFunction(int nlhs, mxArray *plhs[],
        int nrhs, const mxArray *prhs[]) 
{
    int num;
    if (nrhs<1)
    {
        mexErrMsgTxt("not enough input arguments");
        return;
    }
    num = mxGetScalar(prhs[0]);

#else
    int main()
    {
#endif
        static CvCapture *capture = NULL;
        if ( !capture )
        {
            /* initialize camera */
            capture = cvCaptureFromCAM( 0 );
        }

        /* always check */
        if ( !capture ) {
            fprintf( stderr, "Cannot open initialize webcam!\n" );
            return;
        }
        /* This is a hack.  If we don't call this first then getting capture
         * *
         * *  * properties (below) won't work right.  This is an OpenCV bug.  We 
         * *
         * *  * ignore the return value here.  But it's actually a video frame.
         * *
         * *  */

        cvQueryFrame( capture );

        /* Read the video's frame size out of the AVI. */

        CvSize frame_size;

        frame_size.height =

            (int) cvGetCaptureProperty( capture, CV_CAP_PROP_FRAME_HEIGHT );

        frame_size.width =

            (int) cvGetCaptureProperty( capture, CV_CAP_PROP_FRAME_WIDTH );
        long current_frame = 0;

        static IplImage *frame = NULL, *frame1 = NULL, *frame1_1C = NULL, *frame2_1C = NULL, *eig_image = NULL, *temp_image = NULL, *pyramid1 = NULL, *pyramid2 = NULL;

    // Initialise the GUI
    const char *window = "Optical Flow";
    cvNamedWindow(window, CV_WINDOW_AUTOSIZE);

#ifndef MATLAB_MEX_FILE
    // The main loop
    while (cvWaitKey(4) == -1)
    {
#endif
        frame = cvQueryFrame( capture );

        if (frame == NULL)

        {

            /* Why did we get a NULL frame?  We shouldn't be at the end. */

            fprintf(stderr, "Error: Hmm. The end came sooner than we thought.\n");

            return;

        }
        allocateOnDemand( &frame1_1C, frame_size, IPL_DEPTH_8U, 1 );
        cvConvertImage(frame, frame1_1C, 0/*CV_CVTIMG_FLIP*/);

        /* We'll make a full color backup of this frame so that we can draw on it.
         * *
         * *  * (It's not the best idea to draw on the static memory space of cvQueryFrame().)
         * *
         * *  */

        allocateOnDemand( &frame1, frame_size, IPL_DEPTH_8U, 3 );

        cvConvertImage(frame, frame1, 0/*CV_CVTIMG_FLIP*/);

        /* Get the second frame of video.  Sample principles as the first. */

        frame = cvQueryFrame( capture );

        if (frame == NULL)

        {

            fprintf(stderr, "Error: Hmm. The end came sooner than we thought.\n");

            return;

        }

        allocateOnDemand( &frame2_1C, frame_size, IPL_DEPTH_8U, 1 );

        cvConvertImage(frame, frame2_1C, 0/*CV_CVTIMG_FLIP*/);

        /* Shi and Tomasi Feature Tracking! */

        /* Preparation: Allocate the necessary storage. */

        allocateOnDemand( &eig_image, frame_size, IPL_DEPTH_32F, 1 );

        allocateOnDemand( &temp_image, frame_size, IPL_DEPTH_32F, 1 );



        /* Preparation: This array will contain the features found in frame 1. */

        CvPoint2D32f frame1_features[400];

        int number_of_features;

        number_of_features = 400;

        cvGoodFeaturesToTrack(frame1_1C, eig_image, temp_image, frame1_features, &number_of_features, .01, .01, NULL, 3, false, 0.0);

        /* This array will contain the locations of the points from frame 1 in frame 2. */

        CvPoint2D32f frame2_features[400];



        /* The i-th element of this array will be non-zero if and only if the i-th feature of
         * *
         * *  * frame 1 was found in frame 2.
         * *
         * *  */

        char optical_flow_found_feature[400];



        /* The i-th element of this array is the error in the optical flow for the i-th feature
         * *
         * *  * of frame1 as found in frame 2.  If the i-th feature was not found (see the array above)
         * *
         * *  * I think the i-th entry in this array is undefined.
         * *
         * *  */

        float optical_flow_feature_error[400];



        /* This is the window size to use to avoid the aperture problem (see slide "Optical Flow: Overview"). */

        CvSize optical_flow_window = cvSize(3,3);



        /* This termination criteria tells the algorithm to stop when it has either done 20 iterations or when
         * *
         * *  * epsilon is better than .3.  You can play with these parameters for speed vs. accuracy but these values
         * *
         * *  * work pretty well in many situations.
         * *
         * *  */

        CvTermCriteria optical_flow_termination_criteria

            = cvTermCriteria( CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 20, .3 );



        /* This is some workspace for the algorithm.
         * *
         * *  * (The algorithm actually carves the image into pyramids of different resolutions.)
         * *
         * *  */

        allocateOnDemand( &pyramid1, frame_size, IPL_DEPTH_8U, 1 );

        allocateOnDemand( &pyramid2, frame_size, IPL_DEPTH_8U, 1 );


        cvCalcOpticalFlowPyrLK(frame1_1C, frame2_1C, pyramid1, pyramid2, frame1_features, frame2_features, number_of_features, optical_flow_window, 5, optical_flow_found_feature, optical_flow_feature_error, optical_flow_termination_criteria, 0 );



        /* For fun (and debugging :)), let's draw the flow field. */

        for(int i = 0; i < number_of_features; i++)

        {

            /* If Pyramidal Lucas Kanade didn't really find the feature, skip it. */

            if ( optical_flow_found_feature[i] == 0 )   continue;



            int line_thickness;             line_thickness = 1;

            /* CV_RGB(red, green, blue) is the red, green, and blue components
             * *
             * *  * of the color you want, each out of 255.
             * *
             * *  */ 

            CvScalar line_color;            line_color = CV_RGB(255,0,0);

            CvPoint p,q;

            p.x = (int) frame1_features[i].x;

            p.y = (int) frame1_features[i].y;

            q.x = (int) frame2_features[i].x;

            q.y = (int) frame2_features[i].y;

            double angle;       angle = atan2( (double) p.y - q.y, (double) p.x - q.x );

            double hypotenuse;  hypotenuse = sqrt( square(p.y - q.y) + square(p.x - q.x) );

            /* Here we lengthen the arrow by a factor of three. */

            q.x = (int) (p.x - 3 * hypotenuse * cos(angle));

            q.y = (int) (p.y - 3 * hypotenuse * sin(angle));

            cvLine( frame1, p, q, line_color, line_thickness, CV_AA, 0 );

            /* Now draw the tips of the arrow.  I do some scaling so that the
             * *
             * *  * tips look proportional to the main line of the arrow.
             * *
             * *  */         

            p.x = (int) (q.x + 9 * cos(angle + pi / 4));

            p.y = (int) (q.y + 9 * sin(angle + pi / 4));

            cvLine( frame1, p, q, line_color, line_thickness, CV_AA, 0 );

            p.x = (int) (q.x + 9 * cos(angle - pi / 4));

            p.y = (int) (q.y + 9 * sin(angle - pi / 4));

            cvLine( frame1, p, q, line_color, line_thickness, CV_AA, 0 );

        }

#ifdef MATLAB_MEX_FILE
        /* always check */
        if( frame1 )
        {
            /* get a frame */
            int height = frame1->height;
            int width  = frame1->width;
            int dims[] = {height, width, 3};
            //create 3 or 4 dimensional storage and assign it to output arguments
            plhs[0] = mxCreateNumericArray(3,dims , mxUINT8_CLASS, mxREAL);
            //get pointer to data (cast necessary since data is not double, mxGetData returns a void*)
            unsigned char * image = (unsigned char*)mxGetData(plhs[0]);
            for( int px = 0; px < width; px++) {
                for( int py = 0; py < height; py++) {
                    image[py + px*height + 2*width*height] =((uchar*)(frame1->imageData + frame1->widthStep*py))[px*3];
                    image[py + px*height + width*height] =((uchar*)(frame1->imageData + frame1->widthStep*py))[px*3+1];
                    image[py + px*height] =((uchar*)(frame1->imageData + frame1->widthStep*py))[px*3+2];
                }
            }
        }
        if ( num == -1 )
        {
            cvReleaseCapture( &capture );
            capture = 0;
        }
#else
        cvShowImage(window, frame1);
    }
    // Clean up
    cvDestroyAllWindows();
    cvReleaseCapture( &capture );
#endif
    }

