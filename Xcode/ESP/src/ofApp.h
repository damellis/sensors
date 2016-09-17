#pragma once

#include <cstdint>
#include <thread>

// of System
#include "ofMain.h"
#include "ofxDatGui.h"
#include "ofxGrt.h"
#include "ofConsoleFileLoggerChannel.h"

// custom
#include "calibrator.h"
#include "iostream.h"
#include "plotter.h"
#include "training.h"
#include "training-data-manager.h"
#include "tuneable.h"

#define ESP_EVENT(s)                                                \
    ofLogVerbose() << "[" << ofGetTimestampString() << "] " << (s)

class ofApp : public ofBaseApp, public GRT::Observer<GRT::ErrorLogMessage> {
  public:
    ofApp();
    void setup() final;
    void update() final;
    void draw() final;
    void exit() final;

    void keyPressed(int key) final;
    void keyReleased(int key) final;
    void mouseMoved(int x, int y ) final;
    void mouseDragged(int x, int y, int button) final;
    void mousePressed(int x, int y, int button) final;
    void mouseReleased(int x, int y, int button) final;
    void mouseEntered(int x, int y) final;
    void mouseExited(int x, int y) final;
    void windowResized(int w, int h) final;
    void dragEvent(ofDragInfo dragInfo) final;
    void gotMessage(ofMessage msg) final;

    void registerTuneable(Tuneable* t) {
        tuneable_parameters_.push_back(t);
    }

    void reloadPipelineModules();

    // GRT error log observer callback: we simply display it as status text.
    virtual void notify(const ErrorLogMessage& data) final {
        status_text_ = data.getMessage();
    }

    void setBufferSize(uint32_t buffer_size) {
        buffer_size_ = buffer_size;
    }

    void handleArgs(int argc, char* argv[]);
  private:
    enum class AppState {
        kCalibration,
        kPipeline,
        kTraining,
        kTrainingRenaming,
        kTrainingHistoryRecording,
        kTrainingRelabelling,
        kAnalysis,
        kPrediction,
        kConfiguration,
    };
    AppState state_;
    AppState last_state_;

    string getAppStateInstruction() const;

    enum Fragment {
        CALIBRATION,
        PIPELINE,
        TRAINING,
        ANALYSIS,
        PREDICTION,
        CONFIGURATION
    };
    Fragment fragment_;

    inline Fragment getAppView(AppState state) {
        switch (state) {
            case AppState::kCalibration:
                return CALIBRATION;
            case AppState::kPipeline:
                return PIPELINE;
            case AppState::kTraining:
            case AppState::kTrainingRenaming:
            case AppState::kTrainingHistoryRecording:
            case AppState::kTrainingRelabelling:
                return TRAINING;
            case AppState::kAnalysis:
                return ANALYSIS;
            case AppState::kPrediction:
                return PREDICTION;
            case AppState::kConfiguration:
                return CONFIGURATION;
            default:
                // Should never be here.
                assert(false);
        }
    }

    void drawInputs(uint32_t, uint32_t, uint32_t, uint32_t);
    void drawLiveFeatures(uint32_t, uint32_t, uint32_t, uint32_t);

    void drawCalibration();
    void drawLivePipeline();
    void drawTrainingInfo();
    void enableTrainingSampleGUI(bool);
    void drawAnalysis();
    void drawPrediction();

    void useCalibrator(Calibrator& calibrator);
    void usePipeline(GRT::GestureRecognitionPipeline& pipeline);
    void useIStream(InputStream& stream);
    void useOStream(OStream& stream);
    void useOStream(OStreamVector& stream);
    void useTrainingSampleChecker(TrainingSampleChecker checker);
    void useTrainingDataAdvice(string advice);
    void useLeaveOneOutScoring(bool enable) {
        use_leave_one_out_scoring_ = enable;}

    friend void useCalibrator(Calibrator &calibrator);
    friend void usePipeline(GRT::GestureRecognitionPipeline &pipeline);
    friend void useInputStream(InputStream &stream);
    friend void useOutputStream(OStream &stream);
    friend void useOutputStream(OStreamVector &stream);
    friend void useStream(IOStream &stream);
    friend void useStream(IOStreamVector &stream);
    friend void useTrainingSampleChecker(TrainingSampleChecker checker);
    friend void useTrainingDataAdvice(string advice);
    friend void useLeaveOneOutScoring(bool enable);
    friend void setTruePositiveWarningThreshold(double threshold);
    friend void setFalseNegativeWarningThreshold(double threshold);

    // This variable is a guard so that code can check its status and made to be
    // executed only once. Because we are loading the user code ::setup()
    // function multiple times during the execution, this may cause input- or
    // output-streams initialized multiple times. This boolean variable is
    // useful there.
    bool setup_finished_ = false;

    // Currently, we support labels (stored in label_) from 1 to 9.
    const uint32_t kNumMaxLabels_ = 9;
    uint8_t label_;

    // kBufferSize_ controls the number of points in the plot. Note: This is not
    // the buffer size used for training/prediction.
    uint32_t buffer_size_ = 256;

    //========================================================================
    // Pipeline, tuneables and all data
    //========================================================================
    GRT::GestureRecognitionPipeline *pipeline_;

    // The number of pipeline stages will control the UI layout. The number is
    // obtained during setup() and used in draw().
    uint32_t num_pipeline_stages_;

    // These two variables just track the number of pre-processing modules and
    // feature extraction modules so that we can control what data to read for
    // getLastStageProcessedData.
    uint32_t num_preprocessing_modules_;
    uint32_t num_feature_modules_;
    vector<double> getLastStageProcessedData() const;

    vector<Tuneable*> tuneable_parameters_;
    Calibrator* calibrator_;
    TrainingDataManager training_data_manager_;
    TrainingSampleChecker training_sample_checker_ = 0;

    GRT::MatrixDouble sample_data_;
    GRT::MatrixDouble input_data_;
    std::mutex input_data_mutex_;  // input_data_ is written by istream_ thread
                                   // and read by GUI thread.
    GRT::MatrixDouble test_data_;

    //========================================================================
    // Analysis
    //========================================================================
    float training_accuracy_;
    int predicted_label_; CircularBuffer<int> predicted_label_buffer_;
    vector<double> predicted_class_distances_;
    CircularBuffer<vector<double>> predicted_class_distances_buffer_;

    vector<double> predicted_class_likelihoods_;
    CircularBuffer<vector<double>> predicted_class_likelihoods_buffer_;

    vector<UINT> predicted_class_labels_;
    CircularBuffer<vector<UINT>> predicted_class_labels_buffer_;

    vector<UINT> test_data_predicted_class_labels_;

    vector<double> class_distance_values_;
    vector<double> class_likelihood_values_;

    //========================================================================
    // Input/Output streams
    //========================================================================
    InputStream *istream_;
    void onDataIn(GRT::MatrixDouble in);
    vector<OStream *> ostreams_;
    vector<OStreamVector *> ostreamvectors_;

    //========================================================================
    // Application states
    //========================================================================
    bool is_recording_;  // When button 1-9 is pressed, is_recording_ will be
                         // set and data will be added to sample_data_.
    bool enable_history_recording_ = false;
    bool is_in_feature_view_ = false;

    //========================================================================
    // rename
    //========================================================================
    int rename_target_ = -1;
    string rename_title_;
    string display_title_;  // Display title is rename_title_ plus a blinking
                            // underscore.

    //========================================================================
    // relabel
    //========================================================================
    uint32_t relabel_source_;
    string relabel_source_title_;

    //========================================================================
    // visual: status message
    //========================================================================
    string status_text_;
    void setStatus(const string& msg) {
        ESP_EVENT(msg);
        status_text_ = msg;
    }

    ofxDatGui gui_;

    //========================================================================
    // visual: input stream
    //========================================================================
    ofxDatGuiDropdown *serial_selection_dropdown_;
    void onSerialSelectionDropdownEvent(ofxDatGuiDropdownEvent e);

    //========================================================================
    // visual: live plots are across all tabs
    //========================================================================
    InteractiveTimeSeriesPlot plot_inputs_;
    vector<ofxGrtTimeseriesPlot> plot_live_features_;  // live features
    ofxGrtTimeseriesPlot plot_inputs_snapshot_;  // a spectrum of the most
                                                 // recent input vector, shown
                                                 // only if the number of input
                                                 // dimensions is greater than
                                                 // kTooManyFeaturesThreshold
    void onInputPlotRangeSelection(InteractivePlot::RangeSelectedCallbackArgs);
    void onInputPlotValueSelection(
        InteractivePlot::ValueHighlightedCallbackArgs arg);

    //========================================================================
    // visual: calibration
    //
    // Raw input + plotter for each calibrators
    //========================================================================
    ofxGrtTimeseriesPlot plot_raw_;
    vector<Plotter> plot_calibrators_;

    //========================================================================
    // visual: pipeline
    //
    // live data (above) + pre_processed + features
    //========================================================================
    vector<ofxGrtTimeseriesPlot> plot_pre_processed_;
    vector<vector<ofxGrtTimeseriesPlot>> plot_features_;

    //========================================================================
    // visual: test
    //
    // live (above) + window + overview
    //========================================================================
    ofxGrtTimeseriesPlot plot_testdata_window_;
    Plotter plot_testdata_overview_;
    void onTestOverviewPlotSelection(InteractivePlot::RangeSelectedCallbackArgs);
    void updateTestWindowPlot();
    void runPredictionOnTestData();

    //========================================================================
    // visual: training
    //
    // live + samples + features
    //========================================================================
    vector<Plotter> plot_samples_;
    vector<Plotter> plot_samples_snapshots_;
    vector<std::string> plot_samples_info_;
    void onPlotRangeSelected(InteractivePlot::RangeSelectedCallbackArgs arg);
    void updatePlotSamplesSnapshot(int num, int row = -1);
    void onPlotSamplesValueHighlight(InteractivePlot::ValueHighlightedCallbackArgs arg);

    bool is_final_features_too_many_ = false;
    vector<vector<Plotter>> plot_sample_features_;
    void toggleFeatureView();
    void populateSampleFeatures(uint32_t sample_index);
    vector<pair<double, double>> sample_feature_ranges_;

    vector<int> plot_sample_indices_; // the index of the currently plotted
                                      // sample for each class label
    vector<pair<ofRectangle, ofRectangle>> plot_sample_button_locations_;

    vector<ofxDatGui *> training_sample_guis_;
    void renameTrainingSample(int num);
    void renameTrainingSampleDone();
    void deleteTrainingSample(int num);
    void trimTrainingSample(int num);
    void relabelTrainingSample(int num);
    void deleteAllTrainingSamples(int num);
    void doRelabelTrainingSample(uint32_t from, uint32_t to);
    friend class TrainingSampleGuiListener;

    //========================================================================
    // visual: prediction
    //
    // live + likelihood + class_distances
    //========================================================================
    InteractiveTimeSeriesPlot plot_class_likelihoods_;
    vector<InteractiveTimeSeriesPlot> plot_class_distances_;
    void onClassLikelihoodsPlotValueHighlight(
        InteractiveTimeSeriesPlot::ValueHighlightedCallbackArgs arg);
    void onClassDistancePlotValueHighlight(
        InteractiveTimeSeriesPlot::ValueHighlightedCallbackArgs arg);

    //==========================================================================
    // Save and load functionalities
    //==========================================================================
    // Tuneable parameters
    void saveTuneables(ofxDatGuiButtonEvent e);
    void loadTuneables(ofxDatGuiButtonEvent e);
    bool saveTuneablesWithPrompt();
    bool saveTuneables(const string& filename);
    bool loadTuneablesWithPrompt();
    bool loadTuneables(const string& filename);
    bool should_save_tuneables_;

    // Pipeline (including trained model)
    bool savePipelineWithPrompt();
    bool savePipeline(const string& filename);
    bool loadPipelineWithPrompt();
    bool loadPipeline(const string& filename);
    bool should_save_pipeline_;

    // Calibration data
    bool saveCalibrationDataWithPrompt();
    bool saveCalibrationData(const string& filename);
    bool loadCalibrationDataWithPrompt();
    bool loadCalibrationData(const string& filename);
    // Prompts to ask the user to save the calibration data if changed.
    bool should_save_calibration_data_;

    bool saveTrainingDataWithPrompt();
    bool saveTrainingData(const string& filename);
    bool loadTrainingDataWithPrompt();
    bool loadTrainingData(const string& filename);
    // Prompts to ask the user to save the training data if changed.
    bool should_save_training_data_;

    bool saveTestDataWithPrompt();
    bool saveTestData(const string& filename);
    bool loadTestDataWithPrompt();
    bool loadTestData(const string& filename);
    // Prompts to ask the user to save the test data if changed.
    bool should_save_test_data_;

    // Convenient functions that save and load everything from a directory. This
    // assumes the structure of the directory follows our naming convention (see
    // the following few const string definitions). If the naming convention is
    // not satisfied, the load will fail.
    const string kPipelineFilename        = "Pipeline.grt";
    const string kCalibrationDataFilename = "CalibrationData.grt";
    const string kTrainingDataFilename    = "TrainingData.grt";
    const string kTestDataFilename        = "TestData.grt";
    const string kTuneablesFilename       = "TuneableParameters.grt";
    string save_path_ = "";

    // Load all and save all
    void loadAllWithPrompt();
    void loadAll(const string& path);
    void saveAll(bool saveAs = false);

    //========================================================================
    // Training
    //========================================================================
    std::thread training_thread_;
    bool is_training_scheduled_;
    std::uint64_t schedule_time_;

    void beginTrainModel();
    void trainModel();
    void afterTrainModel();

    //========================================================================
    // Scoring
    //========================================================================
    void scoreTrainingData(bool leaveOneOut);
    void scoreImpactOfTrainingSample(int label, const MatrixDouble &sample);
    bool use_leave_one_out_scoring_ = true;

    double true_positive_threshold_;
    double false_negative_threshold_;

    //========================================================================
    // Utils
    //========================================================================
    string getTrainingDataAdvice();
    string training_data_advice_ = "";

    void updateEventReceived(ofEventArgs& arg);  // For renaming
    uint32_t update_counter_ = 0;
    std::shared_ptr<ofConsoleFileLoggerChannel> logger_;
};

class TrainingSampleGuiListener {
  public:
    TrainingSampleGuiListener(ofApp *app, int num) : app(app), num(num) {}
    void renameButtonPressed(ofxDatGuiButtonEvent e) {
        app->renameTrainingSample(num);
    }
    void deleteButtonPressed(ofxDatGuiButtonEvent e) {
        app->deleteTrainingSample(num);
    }
    void trimButtonPressed(ofxDatGuiButtonEvent e) {
        app->trimTrainingSample(num);
    }
    void relabelButtonPressed(ofxDatGuiButtonEvent e) {
        app->relabelTrainingSample(num);
    }
    void deleteAllButtonPressed(ofxDatGuiButtonEvent e) {
        app->deleteAllTrainingSamples(num);
    }
  private:
    ofApp *app;
    int num;
};
