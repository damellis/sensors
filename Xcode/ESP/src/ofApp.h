#pragma once

// C System
#include <stdint.h>

// C++ System
#include <thread>

// of System
#include "ofMain.h"
#include "ofxDatGui.h"
#include "ofxGrt.h"

// custom
#include "calibrator.h"
#include "iostream.h"
#include "plotter.h"
#include "training.h"
#include "training-data-manager.h"
#include "tuneable.h"

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

  private:
    enum Fragment { CALIBRATION, PIPELINE, TRAINING, ANALYSIS, PREDICTION };
    Fragment fragment_;

    void drawInputs(uint32_t, uint32_t, uint32_t, uint32_t);
    void drawCalibration();
    void drawLivePipeline();
    void drawTrainingInfo();
    void drawAnalysis();
    void drawPrediction();

    void useCalibrator(Calibrator &calibrator);
    void usePipeline(GRT::GestureRecognitionPipeline &pipeline);
    void useIStream(InputStream &stream);
    void useOStream(OStream &stream);
    void useOStream(OStreamVector &stream);
    void useTrainingSampleChecker(TrainingSampleChecker checker);
    void useTrainingDataAdvice(string advice);
    void useLeaveOneOutScoring(bool enable) { use_leave_one_out_scoring_ = enable; }

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

    bool setup_finished_ = false;

    uint32_t num_pipeline_stages_;

    // Currently, we support labels (stored in label_) from 1 to 9.
    const uint32_t kNumMaxLabels_ = 9;
    uint8_t label_;

    // kBufferSize_ controls the number of points in the plot. Note: This is not
    // the buffer size used for training/prediction.
    const uint32_t kBufferSize_ = 256;

    // The calibrator that is in use.
    Calibrator *calibrator_;

    TrainingSampleChecker training_sample_checker_ = 0;

    // Input stream, a callback should be registered upon data arrival
    InputStream *istream_;
    // Callback used for input data stream (istream_)
    void onDataIn(GRT::MatrixDouble in);

    // Output streams to which to write the results of the pipeline
    vector<OStream *> ostreams_;
    vector<OStreamVector *> ostreamvectors_;

    // When button 1-9 is pressed, is_recording_ will be set and data will be
    // added to sample_data_.
    bool is_recording_;
    GRT::MatrixDouble sample_data_;

    // input_data_ is written by istream_ thread and read by GUI thread.
    std::mutex input_data_mutex_;
    GRT::MatrixDouble input_data_;

    // Pipeline
    GRT::GestureRecognitionPipeline *pipeline_;

    TrainingDataManager training_data_manager_;

    GRT::MatrixDouble test_data_;
    float training_accuracy_;
    int predicted_label_; CircularBuffer<int> predicted_label_buffer_;
    vector<double> predicted_class_distances_; CircularBuffer<vector<double>> predicted_class_distances_buffer_;
    vector<double> predicted_class_likelihoods_; CircularBuffer<vector<double>> predicted_class_likelihoods_buffer_;
    vector<UINT> predicted_class_labels_; CircularBuffer<vector<UINT>> predicted_class_labels_buffer_;
    vector<UINT> test_data_predicted_class_labels_;

    vector<double> class_distance_values_;
    vector<double> class_likelihood_values_;

    /// ====================================================
    ///  Visuals
    /// ====================================================
    ofxGrtTimeseriesPlot plot_raw_;
    InteractiveTimeSeriesPlot plot_inputs_;
    ofxGrtTimeseriesPlot plot_inputs_snapshot_; // a spectrum of the most
                                                // recent input vector, shown
                                                // only if the number of input
                                                // dimensions is greater than
                                                // kTooManyFeaturesThreshold
    void onInputPlotRangeSelection(InteractivePlot::RangeSelectedCallbackArgs arg);
    void onInputPlotValueSelection(InteractivePlot::ValueHighlightedCallbackArgs arg);
    bool enable_history_recording_ = false;
    bool is_in_history_recording_ = false;

    vector<Plotter> plot_calibrators_;

    vector<ofxGrtTimeseriesPlot> plot_pre_processed_;
    vector<vector<ofxGrtTimeseriesPlot>> plot_features_;
    vector<Plotter> plot_samples_;
    vector<Plotter> plot_samples_snapshots_;
    vector<std::string> plot_samples_info_;
    void updatePlotSamplesSnapshot(int num, int row = -1);
    void onPlotSamplesValueHighlight(InteractivePlot::ValueHighlightedCallbackArgs arg);
    // Features associated with each sample.
    vector<vector<Plotter>> plot_sample_features_;
    void toggleFeatureView();
    bool is_in_feature_view_ = false;
    void populateSampleFeatures(uint32_t sample_index);
    vector<pair<double, double>> sample_feature_ranges_;

    vector<int> plot_sample_indices_; // the index of the currently plotted
                                      // sample for each class label
    vector<pair<ofRectangle, ofRectangle>> plot_sample_button_locations_;
    void onPlotRangeSelected(InteractivePlot::RangeSelectedCallbackArgs arg);
    bool is_final_features_too_many_ = false;

    Plotter plot_testdata_overview_;
    ofxGrtTimeseriesPlot plot_testdata_window_;
    void onTestOverviewPlotSelection(InteractivePlot::RangeSelectedCallbackArgs arg);
    void updateTestWindowPlot();
    void runPredictionOnTestData();

    InteractiveTimeSeriesPlot plot_class_likelihoods_;
    vector<InteractiveTimeSeriesPlot> plot_class_distances_;

    void onClassLikelihoodsPlotValueHighlight(InteractiveTimeSeriesPlot::ValueHighlightedCallbackArgs arg);
    void onClassDistancePlotValueHighlight(InteractiveTimeSeriesPlot::ValueHighlightedCallbackArgs arg);

    // Panel for storing and loading pipeline.
    ofxDatGui gui_;

    /// ====================================================
    ///  Save and load functionalities
    /// ====================================================
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
    void loadAll();
    void saveAll(bool saveAs = false);

    ofxDatGuiDropdown *serial_selection_dropdown_;
    void onSerialSelectionDropdownEvent(ofxDatGuiDropdownEvent e);

    void beginTrainModel();
    void drawEventReceived(ofEventArgs& arg);
    void trainModel();
    void afterTrainModel();

    void scoreTrainingData(bool leaveOneOut);
    void scoreImpactOfTrainingSample(int label, const MatrixDouble &sample);

    bool use_leave_one_out_scoring_ = true;

    vector<ofxDatGui *> training_sample_guis_;
    void renameTrainingSample(int num);
    void renameTrainingSampleDone();
    void deleteTrainingSample(int num);
    void trimTrainingSample(int num);
    void relabelTrainingSample(int num);
    void deleteAllTrainingSamples(int num);
    void doRelabelTrainingSample(uint32_t from, uint32_t to);

    string getTrainingDataAdvice();
    string training_data_advice_ = "";

    // Rename
    bool is_in_renaming_ = false;
    int rename_target_ = -1;
    string rename_title_;
    // Display title is rename_title_ plus a blinking underscore.
    string display_title_;

    // Multithreading to avoid GUI blocked.
    std::thread training_thread_;

    friend class TrainingSampleGuiListener;

    void updateEventReceived(ofEventArgs& arg);
    uint32_t update_counter_ = 0;

    // Relabel
    bool is_in_relabeling_ = false;
    uint32_t relabel_source_;

    // tuneable parameters
    vector<Tuneable*> tuneable_parameters_;

    // Status for user notification
    string status_text_;
    void setStatus(const string& msg) {
        status_text_ = msg;
    }

    bool is_training_scheduled_;
    uint64_t schedule_time_;
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
