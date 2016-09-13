#include "ofApp.h"
#include "ofMain.h"

#ifdef HEADLESS
#include "ofAppNoWindow.h"
#endif  // HEADLESS

void usage() {
// Platform-specific usage
char message[] =
#ifdef __APPLE__
    "open -n ./ESP.app --args <ESP session directory>";
#elif _WIN32
    "No command line argument support for Windows yet";
#else
    "./ESP <ESP session directory>";
#endif
    std::cerr << message << std::endl;
}

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
    } else {
        // wrong number of arguments
        usage();
        return -1;
    }
    ofRunApp(ESP);
}
