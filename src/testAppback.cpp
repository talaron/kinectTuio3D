/*
 A bunch of demos for using Kinect with openFrameworks.
 Copyright (c) 2010 Memo Akten http://www.memo.tv
 
 Using ofxKinect by Theo Watson, Dan Wilcox and co.
 github.com/​ofTheo/​ofxKinect
 
 based on libfreenect by Hector Martin "marcan" 
 git.marcansoft.com/​?p=libfreenect.git
 
 
 This code is licensed to you under the terms of the GNU GPL, version 2 or version 3;
 see:
 gnu.org/​licenses/​old-licenses/​gpl-2.0.txt
 gnu.org/​licenses/​gpl-3.0.txt
 
 More info on the opensource kinect drivers at github.com/​OpenKinect/​openkinect
 */



#include "testApp.h"
#include "ofxSimpleGuiToo.h"
#include "ofxOpenCv.h"
#include "ofxVectorMath.h"
#include "ofxCvKalman.h"


// depth image stuff
ofxCvGrayscaleImage		depthOrig;
ofxCvGrayscaleImage		depthProcessed;
//ofxCvGrayscaleImage		depthAdaptive;
ofxCvContourFinder		depthContours;
//ofxCvGrayscaleImage		depthAccum;


// RGB image stuff
ofxCvColorImage			colorImageRGB;



// depth
bool	invert;
bool	mirror;
int		preBlur;
float	bottomThreshold;
float	topThreshold;
//	int		adaptiveBlockSize;
//	float	adaptiveThreshold;
int		erodeAmount;
int		dilateAmount;
bool	dilateBeforeErode;

// accumulate
//float	accumOldWeight;
//float	accumNewWeight;
//int		accumBlur;
//float	accumThreshold;

// contours
bool	findHoles;
bool	useApproximation;
float	minBlobSize;
float	maxBlobSize;
int		maxNumBlobs;

// transform
float	depthScale;
float	depthOffset;

// tracking
bool doKalman;
float lerpSpeed = 0.1;

ofxVec2f viewRot;


#define kNumPoints		500
#define maxHands        2
ofxVec3f points[maxHands][kNumPoints];
ofxVec3f curPoint[maxHands];
int pointHead = 0;

ofxCvKalman *curPointSmoothed[maxHands][3];


float updateKalman(int i, float v) {
	if(curPointSmoothed[i] == NULL) {
		curPointSmoothed[i] = new ofxCvKalman(v);
		return v;
	} else {
		return curPointSmoothed[0]->correct(v);
	}
}

void clearKalman(int i) {
	if(curPointSmoothed[i]) {
		delete curPointSmoothed[i];
		curPointSmoothed[i] = NULL;
	}
}



//--------------------------------------------------------------
void testApp::setup() {
	ofSetVerticalSync(true);
	
	kinect.init();
	kinect.setVerbose(false);
	kinect.enableDepthNearValueWhite(true);
	kinect.open();
    
	depthOrig.allocate(kinect.getWidth(), kinect.getHeight());
	depthProcessed.allocate(kinect.getWidth(), kinect.getHeight());
	//	depthAdaptive.allocate(kinect.getWidth(), kinect.getHeight());
//	depthAccum.allocate(kinect.getWidth(), kinect.getHeight());
	
	
	colorImageRGB.allocate(kinect.getWidth(), kinect.getHeight());
	
	
	gui.setup();
	gui.config->gridSize.x = 250;
	gui.addTitle("DEPTH PRE PROCESSING");
	gui.addToggle("invert", invert);
	gui.addToggle("mirror", mirror);
	gui.addSlider("preBlur", preBlur, 0, 20);
	gui.addSlider("bottomThreshold", bottomThreshold, 0, 1);
	gui.addSlider("topThreshold", topThreshold, 0, 1);
	//	gui.addSlider("adaptiveBlockSize", adaptiveBlockSize, 0, 5);
	//	gui.addSlider("adaptiveThreshold", adaptiveThreshold, 0, 5);
	gui.addSlider("erodeAmount", erodeAmount, 0, 10);
	gui.addSlider("dilateAmount", dilateAmount, 0, 10);
	gui.addToggle("dilateBeforeErode", dilateBeforeErode);
	
//	gui.addTitle("ACCUMULATE");
//	gui.addSlider("accumOldWeight", accumOldWeight, 0, 1);
//	gui.addSlider("accumNewWeight", accumNewWeight, 0, 1);
//	gui.addSlider("accumBlur", accumBlur, 0, 20);
//	gui.addSlider("accumThreshold", accumThreshold, 0, 1);
	
	
	gui.addTitle("CONTOURS");
	gui.addToggle("findHoles", findHoles);
	gui.addToggle("useApproximation", useApproximation);
	gui.addSlider("minBlobSize", minBlobSize, 0, 1);
	gui.addSlider("maxBlobSize", maxBlobSize, 0, 1);
	gui.addSlider("maxNumBlobs", maxNumBlobs, 0, 2);
	
	gui.addTitle("DEPTH TRANSFORM");
	gui.addSlider("depthScaling", depthScale, -100, 100);
	gui.addSlider("depthOffset", depthOffset, -128, 128);
	
	gui.addTitle("TRACKING");
	gui.addToggle("doKalman", doKalman);
	gui.addSlider("lerpSpeed", lerpSpeed, 0, 1);
	
	
	gui.addContent("depthOrig", depthOrig).setNewColumn(true);
	gui.addContent("depthProcessed", depthProcessed);
	//	gui.addContent("depthAdaptive", depthAdaptive);
//	gui.addContent("depthAccum", depthAccum);
	gui.addContent("depthContours", depthContours);
	
	gui.loadFromXML();
	gui.setDefaultKeys(true);
	gui.show();
	

	GLfloat position[] = {ofGetWidth() * 0.6, ofGetHeight() * 0.3, -2000, 1.0 };
	GLfloat ambientLight[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	GLfloat diffuseLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat specularLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
	
	GLfloat matShininess[] = { 50.0 };
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, matShininess);

	glEnable(GL_LIGHT0);
	glShadeModel (GL_SMOOTH);
	glColorMaterial ( GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE ) ;
	glEnable ( GL_COLOR_MATERIAL ) ;	
	
	// init kalman
	curPointSmoothed[0] = NULL;
	curPointSmoothed[1] = NULL;
	curPointSmoothed[2] = NULL;
    
    blobTracker.setListener( this );
    
    activeAreaVertices[0].x = 10;
    activeAreaVertices[0].y = 10;
    activeAreaVertices[1].x = 200;
    activeAreaVertices[1].y = 200;

	for (int i = 0; i < 2; i++){
		activeAreaVertices[i].bOver 			= false;
		activeAreaVertices[i].bBeingDragged 	= false;
		activeAreaVertices[i].radius = 8;
    }

}


//--------------------------------------------------------------
void testApp::update() {
	
	// rock and roll!
	kinect.update();
	
	// get color pixels
	colorImageRGB			= kinect.getPixels();
	
	// get depth pixels
	depthOrig = kinect.getDepthPixels();
	
	
	// save original depth, and do some preprocessing
	depthProcessed = depthOrig;
	if(invert) depthProcessed.invert();
	if(mirror) {
		depthOrig.mirror(false, true);
		depthProcessed.mirror(false, true);
		colorImageRGB.mirror(false, true);
	}
	if(preBlur) cvSmooth(depthProcessed.getCvImage(), depthProcessed.getCvImage(), CV_BLUR , preBlur*2+1);
	if(topThreshold) cvThreshold(depthProcessed.getCvImage(), depthProcessed.getCvImage(), topThreshold * 255, 255, CV_THRESH_TRUNC);
	if(bottomThreshold) cvThreshold(depthProcessed.getCvImage(), depthProcessed.getCvImage(), bottomThreshold * 255, 255, CV_THRESH_TOZERO);
	if(dilateBeforeErode) {
		if(dilateAmount) cvDilate(depthProcessed.getCvImage(), depthProcessed.getCvImage(), 0, dilateAmount);
		if(erodeAmount) cvErode(depthProcessed.getCvImage(), depthProcessed.getCvImage(), 0, erodeAmount);
	} else {
		if(erodeAmount) cvErode(depthProcessed.getCvImage(), depthProcessed.getCvImage(), 0, erodeAmount);
		if(dilateAmount) cvDilate(depthProcessed.getCvImage(), depthProcessed.getCvImage(), 0, dilateAmount);
	}
	depthProcessed.flagImageChanged();
	
	
	// do adaptive threshold
	//	if(adaptiveThreshold) {
	//		cvAdaptiveThreshold(depthProcessed.getCvImage(), depthAdaptive.getCvImage(), 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 2*adaptiveBlockSize+1, adaptiveThreshold);
	//		depthAdaptive.invert();
	//		depthAdaptive.flagImageChanged();
	//	}
	
	// accumulate older depths to smooth out
//	if(accumOldWeight) {
//		cvAddWeighted(depthAccum.getCvImage(), accumOldWeight, depthProcessed.getCvImage(), accumNewWeight, 0, depthAccum.getCvImage());
//		if(accumBlur) cvSmooth(depthAccum.getCvImage(), depthAccum.getCvImage(), CV_BLUR , accumBlur * 2 + 1);
//		if(accumThreshold) cvThreshold(depthAccum.getCvImage(), depthAccum.getCvImage(), accumThreshold * 255, 255, CV_THRESH_TOZERO);
//		
//		depthAccum.flagImageChanged();
//		
//	} else {
//		depthAccum = depthProcessed;
//	}
//	
	
	// find contours
	depthContours.findContours(depthProcessed,
							   minBlobSize * minBlobSize * depthProcessed.getWidth() * depthProcessed.getHeight(),
							   maxBlobSize * maxBlobSize * depthProcessed.getWidth() * depthProcessed.getHeight(),
							   maxNumBlobs, findHoles, useApproximation);
	
	
	// if one blob found, find nearest point in blob area
	static ofxVec3f newPoint;
	if(depthContours.blobs.size() == 1) {
		ofxCvBlob &blob = depthContours.blobs[0];
		
		depthOrig.setROI(blob.boundingRect);
		
		double minValue, maxValue;
		CvPoint minLoc, maxLoc;
		cvMinMaxLoc(depthOrig.getCvImage(), &minValue, &maxValue, &minLoc, &maxLoc, NULL);

		depthOrig.resetROI();
		
		newPoint.x = maxLoc.x + blob.boundingRect.x;
		newPoint.y = maxLoc.y + blob.boundingRect.y;
//		newPoint.z = (maxValue + offset) * depthScale;	// read from depth map
		
		// read directly from distance (in cm)
		// this doesn't seem to work, need to look into it
		newPoint.z = (kinect.getDistanceAt(newPoint) + depthOffset) * depthScale;
		
		
		// apply kalman filtering
		if(doKalman) {
			newPoint.x = updateKalman(0, newPoint.x);
			newPoint.y = updateKalman(1, newPoint.y);
			newPoint.z = updateKalman(2, newPoint.z);
		}
		
		
	} else {
		clearKalman(0);
		clearKalman(1);
		clearKalman(2);
	}
	
	pointHead = (pointHead + 1) % kNumPoints;
	curPoint += (newPoint - curPoint) * lerpSpeed;
	points[pointHead] = curPoint;
	
	
	
}

//--------------------------------------------------------------
void testApp::draw()
{
	//	kinect.drawDepth(10, 10, 400, 300);
	glColor3f(1, 1, 1);	
	
	colorImageRGB.draw(0, 0, ofGetWidth(), ofGetHeight());
	//	kinect.draw(0, 0, ofGetWidth(), ofGetHeight());
	//	depthContours.draw(0, 0, ofGetWidth(), ofGetHeight());
	
	
	
	if(depthContours.blobs.size() == 2) {
		ofxVec2f center = depthContours.blobs[0].centroid + depthContours.blobs[1].centroid;
        
        ofDrawBitmapString("0", depthContours.blobs[0].centroid.x, depthContours.blobs[0].centroid.y);


		ofxVec2f targetViewRot;
		targetViewRot.x = ofMap(center.x, 0, ofGetWidth(), -360, 360);
		targetViewRot.y = ofMap(center.y, 0, ofGetHeight(), 360, -360);
		
		viewRot += (targetViewRot - viewRot) * 0.05;
	} else {
		viewRot -= viewRot * 0.05;
	}
	
	ofxVec3f camToWorld(ofGetWidth()/depthOrig.getWidth(), ofGetHeight()/depthOrig.getHeight(), 1);
	ofxVec3f up(0, 1, 0);
	
	glEnable(GL_LIGHTING);
	glEnable(GL_DEPTH_TEST);
	
	
	glPushMatrix();
	glTranslatef(ofGetWidth()/2, ofGetHeight()/2, 0);
	glRotatef(viewRot.x, 0, 1, 0);
	glRotatef(viewRot.y, 1, 0, 0);
	
	float thickness = 50;
	glBegin(GL_QUAD_STRIP);
	ofxVec3f c, d;
	for(int i=pointHead + 1; i<pointHead + kNumPoints; i++) {
		ofxVec3f p1 = points[ i % kNumPoints] * camToWorld;		// current point
		ofxVec3f p2 = points[ (i-1 + kNumPoints) % kNumPoints] * camToWorld; // previous point
		
		ofxVec3f dir = (p1-p2).getNormalized();
		ofxVec3f right = (dir.cross(up)).getNormalized();
		ofxVec3f a = p1 - right * thickness/2;
		ofxVec3f b = p1 + right * thickness/2;
		
		ofxVec3f na = right.cross(c-a).getNormalized();
		ofxVec3f nb = (d-b).cross(right).getNormalized();
		
		c = a;
		d = b;

		glColor3f(0, 1, 0.5);
		glNormal3f(na.x, na.y, na.z);
		glVertex3f(a.x-ofGetWidth()/2, a.y-ofGetHeight()/2, a.z);
		
		glNormal3f(nb.x, nb.y, nb.z);
		glVertex3f(b.x-ofGetWidth()/2, b.y-ofGetHeight()/2, b.z);
	}
	glEnd();
	
	glPopMatrix();

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	
	if(depthContours.blobs.size() == 1) {
		glColor3f(1, 0, 0);	
		glLineWidth(3);
		glPushMatrix();
		glTranslatef(curPoint.x * camToWorld.x, curPoint.y * camToWorld.y, curPoint.z);
		glutWireCube(ofGetHeight()/10);
//		glutSolidSphere(50, 10, 10);
		glPopMatrix();
		
		glColor3f(100, 100, 100);	
		string s = ofToString(curPoint.x) + ", " + ofToString(curPoint.y) + ", " + ofToString(curPoint.z);
		ofDrawBitmapString(s, 20, ofGetHeight()-20);
		
		// if 2 blobs found, steer view		
	}
	
    // draw active area rectangle
	ofEnableAlphaBlending();
    ofFill();
    ofSetColor(30,30,200,80);
    
    activeArea = ofRectangle(min(activeAreaVertices[0].x, activeAreaVertices[1].x),
                             min(activeAreaVertices[0].y, activeAreaVertices[1].y),
                             abs(activeAreaVertices[0].x - activeAreaVertices[1].x),
                             abs(activeAreaVertices[0].y -activeAreaVertices[1].y));
    ofRect(activeArea.x, activeArea.y, activeArea.width,  activeArea.height);
    
    ofSetColor(0,0,0,120);
    for (int i = 0; i < 2; i++){
        if (activeAreaVertices[i].bOver == true) ofFill();
        else ofNoFill();
        ofCircle(activeAreaVertices[i].x, activeAreaVertices[i].y,activeAreaVertices[i].radius);
    }
	ofDisableAlphaBlending();

    
	gui.draw();
}

//--------------------------------------------------------------
// Blob listener implementation
void testApp::blobOn( int x, int y, int id, int order ) {}

void testApp::blobMoved( int x, int y, int id, int order) {
	ofxCvTrackedBlob b = blobTracker.getById(id);
	//handForce.set(0.5 * b.deltaLoc.x, 0.5 * b.deltaLoc.y, 0.0);
	// note that we need to position of the blobs to the screen
	//blobCenter.x = ( app_width / vid_width ) * b.centroid.x;
	//blobCenter.y = ( app_height / vid_height) * b.centroid.y;
    
}

void testApp::blobOff( int x, int y, int id, int order ) {}


//--------------------------------------------------------------
void testApp::exit() {
	kinect.close();
}

//--------------------------------------------------------------
void testApp::keyPressed (int key) {
	switch(key) {
		case 'f':
			ofToggleFullscreen();
			break;
	}
}

//------------- -------------------------------------------------
void testApp::mouseMoved(int x, int y ){
	for (int i = 0; i < 2; i++){
		float diffx = x - activeAreaVertices[i].x;
		float diffy = y - activeAreaVertices[i].y;
		float dist = sqrt(diffx*diffx + diffy*diffy);
		if (dist < activeAreaVertices[i].radius){
			activeAreaVertices[i].bOver = true;
		} else {
			activeAreaVertices[i].bOver = false;
		}	
	}
}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){
	for (int i = 0; i < 2; i++){
		if (activeAreaVertices[i].bBeingDragged == true){
            activeAreaVertices[i].x = x;
            activeAreaVertices[i].y = y;
		}
	}
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
	for (int i = 0; i < 2; i++){
		float diffx = x - activeAreaVertices[i].x;
		float diffy = y - activeAreaVertices[i].y;
		float dist = sqrt(diffx*diffx + diffy*diffy);
		if (dist < activeAreaVertices[i].radius){
			activeAreaVertices[i].bBeingDragged = true;
		} else {
			activeAreaVertices[i].bBeingDragged = false;
		}	
	}
    
    
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
    
	for (int i = 0; i < 2; i++){
		activeAreaVertices[i].bBeingDragged = false;	
	}
}

