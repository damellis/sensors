#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofSetupOpenGL(1024, 768, OF_WINDOW);

#ifdef __APPLE__
    ofSetDataPathRoot("../Resources/data/");
#elif _WIN32
    ofSetDataPathRoot("data/");
#else
    ofSetDataPathRoot(".");
#endif

    ofxDatGui::setAssetPath("./");

    // this kicks off the running of my app
    // can be OF_WINDOW or OF_FULLSCREEN
    // pass in width and height too:
    ofRunApp(new ofApp());
}
