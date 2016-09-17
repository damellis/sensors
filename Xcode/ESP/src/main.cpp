#include "ofApp.h"
#include "ofMain.h"
#include "ESP.h"

#ifdef HEADLESS
#include "ofAppNoWindow.h"
#endif  // HEADLESS

// Instructions to run the program
//   Linux  : ./ESP <ESP session directory>

int main(int argc, char* argv[]) {

#ifdef HEADLESS
    ofAppNoWindow w;
    ofSetupOpenGL(&w, 1024, 768, OF_WINDOW);
#else
    ofSetupOpenGL(1024, 768, OF_WINDOW);
#endif  // HEADLESS

// Platform-specific data loading
#ifdef __APPLE__
    ofSetDataPathRoot("../Resources/data/");
#elif _WIN32
    ofSetDataPathRoot("data/");
#else
    ofSetDataPathRoot(".");
#endif

    ofxDatGui::setAssetPath("./");

    ofApp* ESP = new ofApp();
    if (argc == 1) {
        // normal launch
    } else if (argc == 2) {
        ESP->handleArgs(argc, argv);
    }

    ofRunApp(ESP);
}
