#include "ofApp.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <math.h>
#include <sstream>
#include <string>

#include "user.h"
#include "ofxParagraph.h"

// If the feature output dimension is larger than 32, making the visualization a
// single output will be more visual.
const uint32_t kTooManyFeaturesThreshold = 32;

// This delay is needed so that UI can update to reflect the training status.
const uint32_t kDelayBeforeTraining = 50;  // milliseconds

// Instructions for each tab.
static const char* kCalibrateInstruction =
    "Press `s` to save session, `a` to save as, `l` to load session. (`S` and `L` to save/load calibration data only.)\n"
    "Use key 1-9 to record calibration samples (required before you can start training).";

static const char* kPipelineInstruction =
    "Press capital C/P/A/T/R to change tabs, `p` to pause or resume.\n"
    "Press `s` to save session, `a` to save as, `l` to load session.";

static const char* kTrainingInstruction =
    "Press capital C/P/A/T/R to change tabs, `p` to pause or resume.\n"
    "Press `s` to save session, `a` to save as, `l` to load session. (`S` and `L` to save/load training data only.)\n"
    "Hold 1-9 to record samples. Press `t` to train model, `f` to show features.";

static const char* kAnalysisInstruction =
    "Press capital C/P/A/T/R to change tabs, `p` to pause or resume.\n"
    "Press `s` to save session, `a` to save as, `l` to load session. (`S` and `L` to save/load test data only.)\n"
    "Hold `r` to record test data.";

static const char* kPredictionInstruction =
    "Press capital C/P/A/T/R to change tabs, `p` to pause or resume.\n"
    "Press `s` to save session, `a` to save as, `l` to load session.";

const double kPipelineHeightWeight = 0.3;
const ofColor kSerialSelectionColor = ofColor::fromHex(0x00FF00);

// Utility functions forward declaration
string encodeName(const string &name);
string decodeName(const string &name);

class Palette {
  public:
    vector<ofColor> generate(uint32_t n) {
        // TODO(benzh) fill instead of re-generate.
        if (n > size) {
            size = n;
            do_generate(size);
        }

        std::vector<ofColor> sliced(colors.begin(), colors.begin() + n);
        return sliced;
    }

    Palette() : size(256) {
        do_generate(size);
    }
  private:
    void do_generate(uint32_t n) {
        uint32_t numDimensions = n;
        // Code snippet from ofxGrtTimeseriesPlot.cpp

        colors.resize(n);
        // Setup the default colors
        if( numDimensions >= 1 ) colors[0] = ofColor(255,0,0); //red
        if( numDimensions >= 2 ) colors[1] = ofColor(0,255,0); //green
        if( numDimensions >= 3 ) colors[2] = ofColor(0,0,255); //blue
        if( numDimensions >= 4 ) colors[3] = ofColor::orange;
        if( numDimensions >= 5 ) colors[4] = ofColor::purple;
        if( numDimensions >= 6 ) colors[5] = ofColor::brown;
        if( numDimensions >= 7 ) colors[6] = ofColor::pink;
        if( numDimensions >= 8 ) colors[7] = ofColor::grey;
        if( numDimensions >= 9 ) colors[8] = ofColor::cyan;

        //Randomize the remaining colors
        for(unsigned int n=9; n<numDimensions; n++){
            colors[n][0] = ofRandom(50,255);
            colors[n][1] = ofRandom(50,255);
            colors[n][2] = ofRandom(50,255);
        }
    }

    uint32_t size;
    std::vector<ofColor> colors;
};

void ofApp::useCalibrator(Calibrator &calibrator) {
    calibrator_ = &calibrator;
}

void ofApp::useIStream(InputStream &stream) {
    if (!setup_finished_) istream_ = &stream;
}

void ofApp::usePipeline(GRT::GestureRecognitionPipeline &pipeline) {
    pipeline_ = &pipeline;
}

void ofApp::useOStream(OStream &stream) {
    if (!setup_finished_) ostreams_.push_back(&stream);
}

void ofApp::useOStream(OStreamVector &stream) {
    if (!setup_finished_) {
        ostreamvectors_.push_back(&stream);
    }
}

void ofApp::useTrainingSampleChecker(TrainingSampleChecker checker) {
    training_sample_checker_ = checker;
}

void ofApp::useTrainingDataAdvice(string advice) {
    training_data_advice_ = advice;
}

// TODO(benzh): initialize other members as well.
ofApp::ofApp() : fragment_(TRAINING),
                 num_pipeline_stages_(0),
                 calibrator_(nullptr),
                 training_data_manager_(kNumMaxLabels_),
                 should_save_calibration_data_(false),
                 should_save_pipeline_(false),
                 should_save_training_data_(false),
                 should_save_test_data_(false),
                 is_training_scheduled_(false) {
}

//--------------------------------------------------------------
void ofApp::setup() {
    is_recording_ = false;

    // setup() is a user-defined function.
    ::setup(); setup_finished_ = true;

    for (OStream *ostream : ostreams_) {
        if (!(ostream->start())) {
            // TODO(benzh) If failed to start, alert in the GUI.
            ofLog(OF_LOG_ERROR) << "failed to connect to ostream";
        }
    }

    for (OStreamVector *ostream : ostreamvectors_) {
        if (!(ostream->start())) {
            // TODO(benzh) If failed to start, alert in the GUI.
            ofLog(OF_LOG_ERROR) << "failed to connect to ostream";
        }
    }

    if (calibrator_ && !(calibrator_->isCalibrated())) {
        fragment_ = CALIBRATION;
    } else {
        fragment_ = PIPELINE;
    }

    if (training_data_advice_ == "")
        training_data_advice_ = getTrainingDataAdvice();

    istream_->onDataReadyEvent(this, &ofApp::onDataIn);

    Palette color_palette;

    predicted_label_buffer_.resize(kBufferSize_);
    predicted_class_labels_buffer_.resize(kBufferSize_);
    predicted_class_distances_buffer_.resize(kBufferSize_);
    predicted_class_likelihoods_buffer_.resize(kBufferSize_);

    const vector<string>& istream_labels = istream_->getLabels();
    plot_raw_.setup(kBufferSize_, istream_->getNumOutputDimensions(), "Raw Data");
    plot_raw_.setDrawGrid(true);
    plot_raw_.setDrawInfoText(true);
    plot_raw_.setChannelNames(istream_labels);
    plot_inputs_.setup(kBufferSize_, istream_->getNumOutputDimensions(), "Input");
    plot_inputs_.setDrawGrid(true);
    plot_inputs_.setDrawInfoText(true);
    plot_inputs_.setChannelNames(istream_labels);
    plot_inputs_.onRangeSelected(this, &ofApp::onInputPlotRangeSelection, NULL);
    plot_inputs_.onValueHighlighted(this, &ofApp::onInputPlotValueSelection, NULL);
    if (istream_->getNumOutputDimensions() >= kTooManyFeaturesThreshold) {
        plot_inputs_snapshot_.setup(istream_->getNumOutputDimensions(), 1, "Snapshot");
        plot_inputs_.setDrawInfoText(false); // this will be too long to show
    }

    plot_testdata_window_.setup(kBufferSize_, istream_->getNumOutputDimensions(), "Test Data");
    plot_testdata_window_.setDrawGrid(true);
    plot_testdata_window_.setDrawInfoText(true);

    plot_testdata_overview_.setup(istream_->getNumOutputDimensions(), "Overview");
    plot_testdata_overview_.onRangeSelected(this, &ofApp::onTestOverviewPlotSelection, NULL);

    plot_class_likelihoods_.setup(kBufferSize_, kNumMaxLabels_, "Class Likelihoods");
    plot_class_likelihoods_.setDrawInfoText(true);
    // plot_class_likelihoods_.setColorPalette(color_palette.generate(kNumMaxLabels_));
    plot_class_likelihoods_.onValueHighlighted(this, &ofApp::onClassLikelihoodsPlotValueHighlight, NULL);

    plot_class_distances_.resize(kNumMaxLabels_);
    for (int i = 0; i < kNumMaxLabels_; i++) {
        InteractiveTimeSeriesPlot &plot = plot_class_distances_[i];
        plot.setup(kBufferSize_, 2, std::to_string(i + 1));
        plot.setChannelNames({ "Threshold", "Actual" });
        plot.onValueHighlighted(this, &ofApp::onClassDistancePlotValueHighlight, NULL);
    }

    // Parse the user supplied pipeline and extract information:
    //  o num_pipeline_stages_

    // 1. Parse pre-processing.
    uint32_t num_pre_processing = pipeline_->getNumPreProcessingModules();
    num_pipeline_stages_ += num_pre_processing;
    for (int i = 0; i < num_pre_processing; i++) {
        PreProcessing* pp = pipeline_->getPreProcessingModule(i);
        uint32_t dim = pp->getNumOutputDimensions();
        ofxGrtTimeseriesPlot plot;
        plot.setup(kBufferSize_, dim, "PreProcessing Stage " + std::to_string(i));
        plot.setDrawGrid(true);
        plot.setDrawInfoText(true);
        // plot.setColorPalette(color_palette.generate(dim));
        plot_pre_processed_.push_back(plot);
    }

    // 2. Parse pre-processing.
    uint32_t num_feature_modules = pipeline_->getNumFeatureExtractionModules();
    uint32_t num_final_features = 0;
    for (int i = 0; i < num_feature_modules; i++) {
        vector<ofxGrtTimeseriesPlot> feature_at_stage_i;

        FeatureExtraction* fe = pipeline_->getFeatureExtractionModule(i);
        uint32_t feature_dim = fe->getNumOutputDimensions();

        if (feature_dim < kTooManyFeaturesThreshold) {
            for (int i = 0; i < feature_dim; i++) {
                ofxGrtTimeseriesPlot plot;
                plot.setup(kBufferSize_, 1, "Feature " + std::to_string(i));
                plot.setDrawInfoText(true);
                // plot.setColorPalette(color_palette.generate(feature_dim));
                feature_at_stage_i.push_back(plot);
            }
            // Each feature will be draw with a height of stage_height *
            // kPipelineHeightWeight, therefore, the stage counts need to be
            // adjusted.
            num_pipeline_stages_ += ceil(feature_dim * kPipelineHeightWeight);
        } else {
            // We will have only one here.
            ofxGrtTimeseriesPlot plot;
            plot.setup(feature_dim, 1, "Feature");
            plot.setDrawGrid(true);
            plot.setDrawInfoText(true);
            // plot.setColorPalette(color_palette.generate(feature_dim));
            feature_at_stage_i.push_back(plot);

            // Since we will be drawing each feature in a separate plot, count them
            // in pipeline stages.
            num_pipeline_stages_ += 1;
        }
        num_final_features = feature_dim;

        plot_features_.push_back(feature_at_stage_i);
    }

    for (uint32_t i = 0; i < num_final_features; i++) {
        sample_feature_ranges_.push_back(make_pair(0, 0));
    }

    if (calibrator_ != nullptr) {
        vector<CalibrateProcess>& calibrators = calibrator_->getCalibrateProcesses();
        for (uint32_t i = 0; i < calibrators.size(); i++) {
            uint32_t label_dim = istream_->getNumOutputDimensions();
            Plotter plot;
            plot.setup(label_dim, calibrators[i].getName(), calibrators[i].getDescription());
            plot.setColorPalette(color_palette.generate(label_dim));
            plot_calibrators_.push_back(plot);
        }
    }

    for (uint32_t i = 0; i < kNumMaxLabels_; i++) {
        uint32_t label_dim = istream_->getNumOutputDimensions();
        Plotter plot;
        plot.setup(label_dim, training_data_manager_.getLabelName(i + 1));
        plot.setColorPalette(color_palette.generate(label_dim));
        plot_samples_.push_back(plot);
        
        if (istream_->getNumOutputDimensions() >= kTooManyFeaturesThreshold) {
            Plotter plot;
            plot.setup(1, "");
            plot_samples_snapshots_.push_back(plot);
        }

        vector<Plotter> feature_plots;
        if (num_final_features < kTooManyFeaturesThreshold) {
            // For this label, `num_final_features` vertically stacked plots
            for (int j = 0; j < num_final_features; j++) {
                Plotter plot;
                plot.setup(1, "Feature " + std::to_string(j + 1));
                plot.setColorPalette(color_palette.generate(label_dim));
                feature_plots.push_back(plot);
            }
        } else {
            is_final_features_too_many_ = true;

            // The case of many features (like FFT), draw a single plot.
            Plotter plot;
            plot.setup(1, "Feature");
            plot.setColorPalette(color_palette.generate(label_dim));
            feature_plots.push_back(plot);
        }
        plot_sample_features_.push_back(feature_plots);

        plot_sample_indices_.push_back(-1);
        plot_sample_button_locations_.push_back(
            pair<ofRectangle, ofRectangle>(ofRectangle(), ofRectangle()));

        // =====================================================
        //  Add controls for each individual training classes
        // =====================================================
        TrainingSampleGuiListener *listener =
                new TrainingSampleGuiListener(this, i);

        ofxDatGui *gui = new ofxDatGui();
        gui->setWidth(80);
        gui->setAutoDraw(false);
        ofxDatGuiButton* rename_button = gui->addButton("rename");
        rename_button->onButtonEvent(
            listener, &TrainingSampleGuiListener::renameButtonPressed);
        rename_button->setStripeVisible(false);

        ofxDatGuiButton* delete_button = gui->addButton("delete");
        delete_button->onButtonEvent(
            listener, &TrainingSampleGuiListener::deleteButtonPressed);
        delete_button->setStripeVisible(false);

        ofxDatGuiButton* trim_button = gui->addButton("trim");
        trim_button->onButtonEvent(
            listener, &TrainingSampleGuiListener::trimButtonPressed);
        trim_button->setStripeVisible(false);

        ofxDatGuiButton* relabel_button = gui->addButton("relabel");
        relabel_button->onButtonEvent(
            listener, &TrainingSampleGuiListener::relabelButtonPressed);
        relabel_button->setStripeVisible(false);

        ofxDatGuiButton* delete_all_button = gui->addButton("delete all");
        delete_all_button->onButtonEvent(
            listener, &TrainingSampleGuiListener::deleteAllButtonPressed);
        delete_all_button->setStripeVisible(false);

        training_sample_guis_.push_back(gui);
    }

    for (uint32_t i = 0; i < plot_samples_.size(); i++) {
        plot_samples_[i].onRangeSelected(this, &ofApp::onPlotRangeSelected,
                                         reinterpret_cast<void*>(i + 1));
        plot_samples_[i].onValueHighlighted(this, &ofApp::onPlotSamplesValueHighlight,
                                         reinterpret_cast<void*>(i + 1));
    }

    training_data_manager_.setNumDimensions(istream_->getNumOutputDimensions());

    gui_.addHeader(":: Configuration ::");
    gui_.setAutoDraw(false);
    gui_.setPosition(ofGetWidth() - 300, 0);
    gui_.setWidth(280, 140);

    bool should_expand_gui = false;
    // Start input streaming.
    // If failed, this could be due to serial stream's port configuration.
    // We prompt to ask for the port.
    if (!istream_->start()) {
        if (ASCIISerialStream* ss = dynamic_cast<ASCIISerialStream*>(istream_)) {
            vector<string> serials = ss->getSerialDeviceList();
            serial_selection_dropdown_ =
                    gui_.addDropdown("Select A Serial Port", serials);
            serial_selection_dropdown_->onDropdownEvent(
                this, &ofApp::onSerialSelectionDropdownEvent);

            // Fine tune the theme (the default has a red color; we use
            // kSerialSelectionColor)
            ofxDatGuiTheme myTheme(true);
            myTheme.stripe.dropdown = kSerialSelectionColor;
            serial_selection_dropdown_->setTheme(&myTheme);

            gui_.addBreak()->setHeight(5.0f);

            status_text_ = "Please select a serial port from the dropdown menu";

            // We will keep the gui open.
            should_expand_gui = true;
        }
    }

    // Add the rest of the tuneables.
    for (Tuneable* t : tuneable_parameters_) {
        t->addToGUI(gui_);
    }

    // Two extra button for saving/loading tuneable parameters.
    gui_.addBreak()->setHeight(30.0f);
    ofxDatGuiButton* save_button = gui_.addButton("Save");
    ofxDatGuiButton* load_button = gui_.addButton("Load");
    save_button->onButtonEvent(this, &ofApp::saveTuneables);
    load_button->onButtonEvent(this, &ofApp::loadTuneables);

    gui_.addFooter();
    gui_.getFooter()->setLabelWhenExpanded("Click to apply and hide");
    gui_.getFooter()->setLabelWhenCollapsed("Click to open configuration");

    if (should_expand_gui) {
        gui_.expand();
    } else {
        gui_.collapse();
    }

    ofBackground(54, 54, 54);

    // Register myself as logging observer but disable first.
    GRT::ErrorLog::enableLogging(false);
    GRT::ErrorLog::registerObserver(*this);
}

void ofApp::onPlotRangeSelected(InteractivePlot::RangeSelectedCallbackArgs arg) {
    if (is_in_feature_view_) {
        uint32_t sample_index = reinterpret_cast<uint64_t>(arg.data) - 1;
        populateSampleFeatures(sample_index);
    }
}

void ofApp::onPlotSamplesValueHighlight(InteractivePlot::ValueHighlightedCallbackArgs arg) {
    uint32_t sample_index = reinterpret_cast<uint64_t>(arg.data) - 1;
    updatePlotSamplesSnapshot(sample_index, arg.index);
}

void ofApp::updatePlotSamplesSnapshot(int num, int row) {
    // Nothing to do if we're not showing the snapshots.
    if (istream_->getNumOutputDimensions() < kTooManyFeaturesThreshold) return;
    
    plot_samples_snapshots_[num].clearData();
    
    if (row == -1) row = plot_samples_[num].getData().getNumRows() - 1;
    for (int i = 0; i < plot_samples_[num].getData().getNumCols(); i++) {
        plot_samples_snapshots_[num].push_back({
            plot_samples_[num].getData().getRowVector(row)[i]
        });
    }
}

void ofApp::populateSampleFeatures(uint32_t sample_index) {
    if (pipeline_->getNumFeatureExtractionModules() == 0) { return; }

    // Clean up historical data/caches.
    pipeline_->reset();

    vector<Plotter>& feature_plots = plot_sample_features_[sample_index];
    for (Plotter& plot : feature_plots) { plot.clearData(); }

    // 1. get samples
    MatrixDouble& sample = plot_samples_[sample_index].getData();
    uint32_t start = 0;
    uint32_t end = sample.getNumRows();
    if (is_final_features_too_many_) {
        pair<uint32_t, uint32_t> sel = plot_samples_[sample_index].getSelection();
        if (sel.second - sel.first > 10) {
            start = sel.first;
            end = sel.second;
        }
    }

    // 2. get features by flowing samples through
    for (uint32_t i = start; i < end; i++) {
        vector<double> data_point = sample.getRowVector(i);
        if (!pipeline_->preProcessData(data_point)) {
            ofLog(OF_LOG_ERROR) << "ERROR: Failed to compute features!";
            continue;
        }

        // Last stage of feature extraction.
        uint32_t j = pipeline_->getNumFeatureExtractionModules();
        vector<double> feature = pipeline_->getFeatureExtractionData(j - 1);

        for (uint32_t k = 0; k < feature_plots.size(); k++) {
            vector<double> feature_point = { feature[k] };
            feature_plots[k].push_back(feature_point);

            // sample_feature_ranges_[k].(first, second) tracks the min and max
            // for feature k so that the plots will be comparable.
            if (sample_feature_ranges_[k].first > feature[k]) {
                sample_feature_ranges_[k].first = feature[k];
            }
            if (sample_feature_ranges_[k].second < feature[k]) {
                sample_feature_ranges_[k].second = feature[k];
            }
        }

        if (is_final_features_too_many_) {
            assert(feature_plots.size() == 1);
            MatrixDouble feature_matrix;
            feature_matrix.resize(feature.size(), 1);
            feature_matrix.setColVector(feature, 0);
            sample_feature_ranges_[0].first = feature_matrix.getMinValue();
            sample_feature_ranges_[0].second = feature_matrix.getMaxValue();
            feature_plots[0].setData(feature_matrix);
        }
    }
}

void ofApp::onInputPlotRangeSelection(InteractiveTimeSeriesPlot::RangeSelectedCallbackArgs arg) {
    if (!enable_history_recording_) {
        plot_inputs_.clearSelection();
        return;
    }

    status_text_ = "Press 1-9 to extract from live data to training data.";
    is_in_history_recording_ = true;
    sample_data_.clear();
    sample_data_ = plot_inputs_.getData(arg.start, arg.end);
}

void ofApp::onInputPlotValueSelection(InteractiveTimeSeriesPlot::ValueHighlightedCallbackArgs arg) {
    if (enable_history_recording_) {
        int i = arg.index;
        predicted_label_ = predicted_label_buffer_[i];
        predicted_class_distances_ = predicted_class_distances_buffer_[i];
        predicted_class_likelihoods_ = predicted_class_likelihoods_buffer_[i];
        predicted_class_labels_ = predicted_class_labels_buffer_[i];
        plot_inputs_snapshot_.setData(plot_inputs_.getData(arg.index));
    }
}

void ofApp::onClassLikelihoodsPlotValueHighlight(InteractiveTimeSeriesPlot::ValueHighlightedCallbackArgs arg) {
    if (enable_history_recording_) {
        class_likelihood_values_ = arg.source->getData(arg.index);
    }
}

void ofApp::onClassDistancePlotValueHighlight(InteractiveTimeSeriesPlot::ValueHighlightedCallbackArgs arg) {
    if (enable_history_recording_) {
        class_distance_values_ = arg.source->getData(arg.index);
    }
}

void ofApp::onTestOverviewPlotSelection(InteractivePlot::RangeSelectedCallbackArgs arg) {
    updateTestWindowPlot();
}

void ofApp::updateTestWindowPlot() {
    std::pair<uint32_t, uint32_t> sel = plot_testdata_overview_.getSelection();
    uint32_t start = 0;
    uint32_t end = test_data_.getNumRows();
    if (sel.second - sel.first > 10) {
        start = sel.first;
        end = sel.second;
    }
    plot_testdata_window_.reset();
    for (int i = start; i < end; i++) {
        plot_testdata_window_.setup(end - start, istream_->getNumInputDimensions(), "Test Data");
        for (int i = start; i < end; i++) {
            if (pipeline_->getTrained()) {
                int predicted_label = test_data_predicted_class_labels_[i];
                std::string title = "";
                if (predicted_label != 0) title = training_data_manager_.getLabelName(predicted_label);
                plot_testdata_window_.update(test_data_.getRowVector(i), predicted_label != 0, title);
            } else {
                plot_testdata_window_.update(test_data_.getRowVector(i));
            }
        }
    }
}

void ofApp::runPredictionOnTestData() {
    test_data_predicted_class_labels_.resize(test_data_.getNumRows());
    for (int i = 0; i < test_data_.getNumRows(); i++) {
        if (pipeline_->getTrained()) {
            pipeline_->predict(test_data_.getRowVector(i));

            int predicted_label = pipeline_->getPredictedClassLabel();

            test_data_predicted_class_labels_[i] = predicted_label;
        } else {
            test_data_predicted_class_labels_[i] = 0;
        }
    }
}

bool ofApp::savePipelineWithPrompt() {
    ofFileDialogResult result = ofSystemSaveDialog(
        kPipelineFilename, "Save pipeline?");
    if (!result.bSuccess) { return false; }
    return savePipeline(result.getPath());
}

bool ofApp::savePipeline(const string& filename) {
    if (pipeline_->save(filename)) {
        setStatus("Pipeline is saved to " + filename);
        should_save_pipeline_ = false;
        return true;
    } else {
        setStatus("Failed to save pipeline to " + filename);
        return false;
    }
}

bool ofApp::loadPipelineWithPrompt() {
    ofFileDialogResult result = ofSystemLoadDialog(
        "Load existing pipeline", false);
    if (!result.bSuccess) { return false; }
    return loadPipeline(result.getPath());
}

bool ofApp::loadPipeline(const string& filename) {
    if (pipeline_->load(filename)) {
        setStatus("Pipeline is loaded from " + filename);
        should_save_pipeline_ = false;
        if (pipeline_->getTrained()) afterTrainModel();
        return true;
    } else {
        setStatus("Failed to load pipeline from " + filename);
        return false;
    }
}

bool ofApp::saveCalibrationDataWithPrompt() {
    if (calibrator_ == NULL) return false;

    ofFileDialogResult result = ofSystemSaveDialog(
        kCalibrationDataFilename, "Save your calibration data?");
    if (!result.bSuccess) { return false; }
    return saveCalibrationData(result.getPath());
}

bool ofApp::saveCalibrationData(const string& filename) {
    if (calibrator_ == NULL) return true;

    // Pack calibration samples into a TimeSeriesClassificationData so they can
    // all be saved in a single file.
    GRT::TimeSeriesClassificationData data(istream_->getNumOutputDimensions(),
                                           "CalibrationData");
    auto calibrators = calibrator_->getCalibrateProcesses();
    for (int i = 0; i < calibrators.size(); i++) {
        data.addSample(i, calibrators[i].getData());
        data.setClassNameForCorrespondingClassLabel(
            encodeName(calibrators[i].getName()), i);
    }

    if (data.save(filename)) {
        setStatus("Calibration data is saved to " + filename);
        should_save_calibration_data_ = false;
        return true;
    } else {
        setStatus("Failed to save calibration data to " + filename);
        return false;
    }
}

bool ofApp::loadCalibrationDataWithPrompt() {
    if (calibrator_ == NULL) return false;

    ofFileDialogResult result = ofSystemLoadDialog(
        "Load existing calibration data", false);
    if (!result.bSuccess) { return false; }
    return loadCalibrationData(result.getPath());
}

bool ofApp::loadCalibrationData(const string& filename) {
    if (calibrator_ == NULL) {
        if (ofFile::doesFileExist(filename)) {
            setStatus("Calibration file exists but there's no calibrator.");
            return false;
        }

        return true; // nothing to do, so declare victory and go home
    }

    vector<CalibrateProcess>& calibrators = calibrator_->getCalibrateProcesses();
    GRT::TimeSeriesClassificationData data;

    if (data.load(filename)) {
        setStatus("Calibration data is loaded from " + filename);
    } else {
        setStatus("Failed to load calibration data from " + filename);
        return false;
    }

    if (data.getNumSamples() != calibrators.size()) {
        setStatus("Number of samples in file differs from the "
                  "number of calibration samples.");
        return false;
    }

    if (data.getNumDimensions() != istream_->getNumOutputDimensions()) {
        setStatus("Number of dimensions of data in file differs "
                  "from the number of dimensions expected.");
        return false;
    }

    for (int i = 0; i < data.getNumSamples(); i++) {
        string name = decodeName(data.getClassNameForCorrespondingClassLabel(i));
        if (name != calibrators[i].getName()) {
            ofLog(OF_LOG_WARNING) << "Name of saved calibration sample " << (i + 1) << " ('"
                                  << data.getClassNameForCorrespondingClassLabel(i)
                                  << "') differs from current calibration sample name ('"
                                  << calibrators[i].getName() << "')";
        }
        calibrators[i].clear();
        plot_calibrators_[i].reset();
        if (calibrators[i].calibrate(data[i].getData()).getResult() ==
            CalibrateResult::FAILURE) {
            ofLog(OF_LOG_WARNING) << "Failed to calibrate saved "
                                  << "calibration sample " << (i + 1) << ": "
                                  << calibrators[i].getName();
        } else {
            plot_calibrators_[i].setData(data[i].getData());
        }
    }

    plot_inputs_.reset();
    should_save_calibration_data_ = false;
    return true;
}

bool ofApp::saveTrainingDataWithPrompt() {
    ofFileDialogResult result = ofSystemSaveDialog(
        kTrainingDataFilename, "Save your training data?");
    if (!result.bSuccess) { return false; }
    return saveTrainingData(result.getPath());
}

bool ofApp::saveTrainingData(const string& filename) {
    if (training_data_manager_.save(filename)) {
        setStatus("Training data is saved to " + filename);
        should_save_training_data_ = false;
        return true;
    } else {
        setStatus("Failed to save training data to " + filename);
        return false;
    }
}

bool ofApp::loadTrainingDataWithPrompt() {
    ofFileDialogResult result = ofSystemLoadDialog(
        "Load existing training data", false);
    if (!result.bSuccess) { return false; }
    return loadTrainingData(result.getPath());
}

bool ofApp::loadTrainingData(const string& filename) {
    GRT::TimeSeriesClassificationData training_data;

    if (training_data_manager_.load(filename)) {
        setStatus("Training data is loaded from " + filename);
        should_save_training_data_ = false;
    } else {
        setStatus("Failed to load training data from " + filename);
        return false;
    }

    // Update the plotting
    for (uint32_t i = 1; i <= kNumMaxLabels_; i++) {
        uint32_t num = training_data_manager_.getNumSampleForLabel(i);
        plot_sample_indices_[i - 1] = num - 1;

        if (num > 0) {
            plot_samples_[i - 1].setData(training_data_manager_.getSample(i, num - 1));
        } else {
            plot_samples_[i - 1].reset();
        }

        std::string title = training_data_manager_.getLabelName(i);
        plot_samples_[i - 1].setTitle(title);
        
        updatePlotSamplesSnapshot(i - 1);
    }

    return true;
}

bool ofApp::saveTestDataWithPrompt() {
    if (test_data_.getNumRows() == 0) return false;

    ofFileDialogResult result = ofSystemSaveDialog(
        kTestDataFilename, "Save your test data?");
    if (!result.bSuccess) { return false; }
    return saveTestData(result.getPath());
}

bool ofApp::saveTestData(const string& filename) {
    // if there's no data, don't write a file. otherwise, we'll get an empty
    // file, which we won't be able to load.
    if (test_data_.getNumRows() == 0) return true;

    if (test_data_.save(filename)) {
        setStatus("Test data is saved to " + filename);
        should_save_test_data_ = false;
        return true;
    } else {
        setStatus("Failed to save test data to " + filename);
        return false;
    }
}

bool ofApp::loadTestDataWithPrompt() {
    ofFileDialogResult result = ofSystemLoadDialog(
        "Load existing test data", false);
    if (!result.bSuccess) { return false; }
    return loadTestData(result.getPath());
}

bool ofApp::loadTestData(const string& filename) {
    GRT::MatrixDouble test_data;

    if (ofFile::doesFileExist(filename)) {
        if (test_data.load(filename) ){
            setStatus("Test data is loaded from " + filename);
            should_save_test_data_ = false;
        } else {
            setStatus("Failed to load test data from " + filename);
            return false;
        }
    } else should_save_test_data_ = false;

    test_data_ = test_data;
    plot_testdata_overview_.setData(test_data_);
    runPredictionOnTestData();
    updateTestWindowPlot();

    return true;
}

bool ofApp::saveTuneablesWithPrompt() {
    ofFileDialogResult result = ofSystemSaveDialog("TuneableParameters.grt",
                                                   "Save your tuneable parameters?");
    if (!result.bSuccess) { return false; }
    return saveTuneables(result.getPath());
}

bool ofApp::saveTuneables(const string& filename) {
    std::ofstream file(filename);
    for (Tuneable* t : tuneable_parameters_) {
        file << t->toString() << std::endl;
    }
    file.close();
    return true; // TODO: check for failure
}

bool ofApp::loadTuneablesWithPrompt() {
    ofFileDialogResult result = ofSystemLoadDialog("Load tuneable parameters", true);
    if (!result.bSuccess) { return false; }
    return loadTuneables(result.getPath());
}

bool ofApp::loadTuneables(const string& filename) {
    std::string line;
    std::ifstream file(filename);
    for (Tuneable* t : tuneable_parameters_) {
        std::getline(file, line);
        t->fromString(line);
    }
    file.close();
    reloadPipelineModules();
    return true; // TODO: check for failure
}

void ofApp::saveTuneables(ofxDatGuiButtonEvent e) { saveTuneablesWithPrompt(); }
void ofApp::loadTuneables(ofxDatGuiButtonEvent e) { loadTuneablesWithPrompt(); }

void ofApp::loadAll() {
    ofFileDialogResult result = ofSystemLoadDialog(
        "Load an exising ESP session", true);
    if (!result.bSuccess) { return; }

    save_path_ = result.getPath();
    const string dir = save_path_ + "/";

    // Need to load tuneable before pipeline because loading the tuneables
    // resets the pipeline. Also, need to load pipeline after training and
    // test data so we can use the loaded pipeline to score training data and
    // evaluate test data.
    if (loadCalibrationData(dir + kCalibrationDataFilename) &&
        loadTuneables(dir + kTuneablesFilename) &&
        loadTrainingData(dir + kTrainingDataFilename) &&
        loadTestData(dir + kTestDataFilename) &&
        loadPipeline(dir + kPipelineFilename)) {

        setStatus("ESP session is loaded from " + dir);
    } else {
        // TODO(benzh) Temporarily disable this message so that each individual
        // load will reveal which one failed.
        // setStatus("Failed to load ESP from " + dir);
    }

}

void ofApp::saveAll(bool saveAs) {
    if (save_path_.empty() || saveAs) {
        ofFileDialogResult result = ofSystemSaveDialog(
            "ESP", "Save this session");
        if (!result.bSuccess) { return; }
        save_path_ = result.getPath();
    }

    // Create a directory with result.path as the absolute path.
    const string dir = save_path_ + "/";
    if (ofDirectory::createDirectory(dir, false, false)
        && saveCalibrationData(dir + kCalibrationDataFilename)
        && savePipeline(dir + kPipelineFilename)
        && saveTrainingData(dir + kTrainingDataFilename)
        && saveTestData(dir + kTestDataFilename)
        && saveTuneables(dir + kTuneablesFilename)) {

        setStatus("ESP session is saved to " + dir);
        should_save_test_data_ = false;
    } else {
        // TODO(benzh) Temporarily disable this message so that each individual
        // save will reveal which one failed.
        // setStatus("Failed to save ESP session to " + dir);
    }

}

void ofApp::onSerialSelectionDropdownEvent(ofxDatGuiDropdownEvent e) {
    if (istream_->hasStarted()) { return; }

    if (ASCIISerialStream* ss = dynamic_cast<ASCIISerialStream*>(istream_)) {
        if (ss->selectSerialDevice(e.child)) {
            serial_selection_dropdown_->collapse();
            serial_selection_dropdown_->setVisible(false);
            gui_.collapse();
            status_text_ = "";
        } else {
            status_text_ = "Please select another serial port!";
        }
    }
}

void ofApp::renameTrainingSample(int num) {
    // If we are already in renaming, finish it by calling rename...Done.
    if (is_in_renaming_) {
        renameTrainingSampleDone();
    }

    int label = num + 1;
    // TODO(benzh) This should be renaming each sample, instead of each label.
    // Currently, we are in the transition from managing everything in ofApp to
    // individual components (such as TrainingDataManager).
    rename_title_ = training_data_manager_.getLabelName(label);

    is_in_renaming_ = true;
    rename_target_ = label;
    display_title_ = rename_title_;
    plot_samples_[rename_target_ - 1].renameTitleStart();
    plot_samples_[rename_target_ - 1].setTitle(display_title_);
    ofAddListener(ofEvents().update, this, &ofApp::updateEventReceived);
}

void ofApp::renameTrainingSampleDone() {
    training_data_manager_.setNameForLabel(rename_title_, rename_target_);

    is_in_renaming_ = false;
    plot_samples_[rename_target_ - 1].setTitle(rename_title_);
    plot_samples_[rename_target_ - 1].renameTitleDone();
    ofRemoveListener(ofEvents().update, this, &ofApp::updateEventReceived);
    should_save_training_data_ = true;
}

void ofApp::updateEventReceived(ofEventArgs& arg) {
    update_counter_++;

    // Assuming 60fps, to update the cursor every 0.1 seconds
    int period = 60 * 0.1;
    if (is_in_renaming_) {
        if (update_counter_ == period) {
            display_title_ = rename_title_ + "_";
        } else if (update_counter_ == period * 2) {
            display_title_ = rename_title_;
            update_counter_ = 0;
        }
        plot_samples_[rename_target_ - 1].setTitle(display_title_);
    }
}

void ofApp::deleteTrainingSample(int num) {
    int label = num + 1;

    if (plot_sample_indices_[num] < 0) { return; }
    training_data_manager_.deleteSample(label, plot_sample_indices_[num]);

    uint32_t num_sample_left = training_data_manager_.getNumSampleForLabel(label);

    // Before, we might be showing the last one; adjust the sample down by one
    if (plot_sample_indices_[num] == num_sample_left) {
        plot_sample_indices_[num]--;
    }
    if (plot_sample_indices_[num] >= 0) {
        plot_samples_[num].setData(
            training_data_manager_.getSample(label, plot_sample_indices_[num]));
    } else {
        plot_samples_[num].reset();
        plot_sample_indices_[num] = -1;
    }

    updatePlotSamplesSnapshot(num);
    populateSampleFeatures(num);
    should_save_training_data_ = true;
}

void ofApp::deleteAllTrainingSamples(int num) {
    int label = num + 1;

    training_data_manager_.deleteAllSamplesWithLabel(label);

    uint32_t num_sample_left = training_data_manager_.getNumSampleForLabel(label);

    plot_samples_[num].reset();
    plot_sample_indices_[num] = -1;

    updatePlotSamplesSnapshot(num);
    populateSampleFeatures(num);
    should_save_training_data_ = true;
}

void ofApp::trimTrainingSample(int num) {
    pair<uint32_t, uint32_t> selection = plot_samples_[num].getSelection();

    // Return if no selection or the range is too small (if user left clicked).
    if (selection.second - selection.first < 10) { return; }

    int label = num + 1;

    training_data_manager_.trimSample(label, plot_sample_indices_[num],
                                      selection.first, selection.second);
    plot_samples_[num].setData(
        training_data_manager_.getSample(label, plot_sample_indices_[num]));

    updatePlotSamplesSnapshot(num);
    populateSampleFeatures(num);
    should_save_training_data_ = true;
}

void ofApp::relabelTrainingSample(int num) {
    // After this button is pressed, we enter relabel_mode
    is_in_relabeling_ = true;
    relabel_source_ = num + 1;
}

void ofApp::doRelabelTrainingSample(uint32_t source, uint32_t target) {
    if (source == target) {
        return;
    }

    // // plot_samples_ (num) is 0-based, labels (source and target) are 1-based.
    uint32_t num = source - 1;
    uint32_t label = source;
    if (plot_sample_indices_[num] < 0) { return; }
    training_data_manager_.relabelSample(source, plot_sample_indices_[num], target);

    // Update the source plot
    uint32_t num_source_sample_left = training_data_manager_.getNumSampleForLabel(source);
    if (plot_sample_indices_[num] == num_source_sample_left) {
        plot_sample_indices_[num]--;
    }
    if (plot_sample_indices_[num] >= 0) {
        plot_samples_[num].setData(
            training_data_manager_.getSample(source, plot_sample_indices_[num]));
    } else {
        plot_samples_[num].reset();
        plot_sample_indices_[num] = -1;
    }
    updatePlotSamplesSnapshot(num);
    populateSampleFeatures(num);

    // Update the target plot
    plot_sample_indices_[target - 1]++;
    plot_samples_[target - 1].setData(
        training_data_manager_.getSample(target, plot_sample_indices_[target - 1]));
    updatePlotSamplesSnapshot(target - 1);
    populateSampleFeatures(target - 1);

    should_save_training_data_ = true;
}

string ofApp::getTrainingDataAdvice() {
    if (!pipeline_->getIsClassifierSet()) return "";
    if (dynamic_cast<DTW *>(pipeline_->getClassifier())) {
        return "This algorithm picks a representative sample for each class "
            "and looks for the class with the closest representative sample. "
            "As a result, you don't need a lot of training data but bad "
            "training samples can cause problems.";
    }
    if (dynamic_cast<ANBC *>(pipeline_->getClassifier())) {
        return "This algorithm uses an average of the training data. "
            "As a result, recording additional training data can help the "
            "performance of the algorithm. For each class, try to record "
            "training data that represents the range of situations you want "
            "to be recognized.";
    }
    if (dynamic_cast<SVM *>(pipeline_->getClassifier())) {
        return "This algorithm looks at the boundaries between the different "
            "classes of training data. As a result, it can help to record "
            "additional data at the boundaries between the different classes "
            "you want to recognize.";
    }
    return "";
}

//--------------------------------------------------------------
void ofApp::update() {
    std::lock_guard<std::mutex> guard(input_data_mutex_);
    for (int i = 0; i < input_data_.getNumRows(); i++){
        vector<double> raw_data = input_data_.getRowVector(i);
        vector<double> data_point;
        plot_raw_.update(raw_data);
        if (calibrator_ == nullptr) {
            data_point = raw_data;
        } else if (calibrator_->isCalibrated()) {
            data_point = calibrator_->calibrate(raw_data);
        } else {
            // Not calibrated! For now, force the tab to be CALIBRATION.
            fragment_ = CALIBRATION;
        }

        std::string title;

        if (pipeline_->getTrained()) {
            pipeline_->predict(data_point);

            predicted_label_ = pipeline_->getPredictedClassLabel();
            predicted_label_buffer_.push_back(predicted_label_);

            if (predicted_label_ != 0) {
                for (OStream *ostream : ostreams_)
                    ostream->onReceive(predicted_label_);
                for (OStream *ostream : ostreamvectors_)
                    ostream->onReceive(predicted_label_);

                title = training_data_manager_.getLabelName(predicted_label_);
            }
            
            predicted_class_labels_ = pipeline_->getClassLabels();
            predicted_class_labels_buffer_.push_back(predicted_class_labels_);

            predicted_class_likelihoods_ = pipeline_->getClassLikelihoods();
            predicted_class_likelihoods_buffer_.push_back(predicted_class_likelihoods_);

            vector<double> likelihoods(kNumMaxLabels_);
            for (int i = 0; i < predicted_class_likelihoods_.size() &&
                            i < predicted_class_labels_.size(); i++)
            {
                likelihoods[predicted_class_labels_[i] - 1] =
                    predicted_class_likelihoods_[i];
            }
            plot_class_likelihoods_.update(likelihoods, predicted_label_ != 0, title);

            predicted_class_distances_ = pipeline_->getClassDistances();
//            // TODO(damellis): this shouldn't be classifier-specific but should
//            // instead be based on a virtual function in Classifier or similar.
//            DTW *dtw = dynamic_cast<DTW *>(pipeline_->getClassifier());
//            if (dtw != NULL) {
//                for (int k = 0; k < predicted_class_distances_.size() &&
//                                k < predicted_class_labels_.size(); k++) {
//                    predicted_class_distances_[k] =
//                        dtw->classDistanceToNullRejectionCoefficient(
//                            predicted_class_labels_[k],
//                            predicted_class_distances_[k]);
//                }
//            }
            predicted_class_distances_buffer_.push_back(predicted_class_distances_);

            for (int i = 0; i < predicted_class_distances_.size() &&
                            i < predicted_class_labels_.size(); i++) {
                vector<double> thresholds = pipeline_->getClassifier()->getNullRejectionThresholds();
                plot_class_distances_[predicted_class_labels_[i] - 1].update(
                    vector<double>{
                        (thresholds.size() > i ? thresholds[i] : 0.0),
                        predicted_class_distances_[i]
                    }, thresholds.size() > i && predicted_class_distances_[i] < thresholds[i],
                    "");
            }
        } else predicted_label_ = 0;

        plot_inputs_.update(data_point, predicted_label_ != 0, title);
        if (istream_->getNumOutputDimensions() >= kTooManyFeaturesThreshold)
            plot_inputs_snapshot_.setData(data_point);

        if (istream_->hasStarted() &&
            (calibrator_ == NULL || calibrator_->isCalibrated())) {
            if (!pipeline_->preProcessData(data_point)) {
                ofLog(OF_LOG_ERROR) << "ERROR: Failed to compute features!";
            }

            vector<double> data = data_point;

            for (int j = 0; j < pipeline_->getNumPreProcessingModules(); j++) {
                data = pipeline_->getPreProcessedData(j);
                plot_pre_processed_[j].update(data);
            }

            for (int j = 0; j < pipeline_->getNumFeatureExtractionModules(); j++) {
                // Working on j-th stage.
                data = pipeline_->getFeatureExtractionData(j);
                if (data.size() < kTooManyFeaturesThreshold) {
                    for (int k = 0; k < data.size(); k++) {
                        vector<double> v = { data[k] };
                        plot_features_[j][k].update(v);
                    }
                } else {
                    assert(plot_features_[j].size() == 1);
                    plot_features_[j][0].setData(data);
                }
            }

            // If there's no classifier set, we've got a signal processing
            // pipeline and we should send the results of the pipeline to
            // any OStreamVector instances that are listening for it.
            // TODO(damellis): this logic will need updating when / if we
            // support regression and clustering pipelines.
            if (!pipeline_->getIsClassifierSet()) {
                for (OStreamVector *stream : ostreamvectors_) {
                    stream->onReceive(data);
                }
            }
        }

        if (is_recording_) {
            if (fragment_ == CALIBRATION) {
                sample_data_.push_back(raw_data);
            } else {
                sample_data_.push_back(data_point);
            }
        }
    }

    if (is_training_scheduled_ == true &&
        (ofGetElapsedTimeMillis() - schedule_time_ > kDelayBeforeTraining)) {
        trainModel();
    }
}

void ofDrawColoredBitmapString(ofColor color,
                               const string& text,
                               float x, float y) {
    ofPushStyle();
    ofSetColor(color);
    ofDrawBitmapString(text, x, y);
    ofPopStyle();
}

//--------------------------------------------------------------
void ofApp::draw() {
    // Hacky panel on the top.
    const uint32_t left_margin = 10;
    const uint32_t top_margin = 20;
    const uint32_t margin = 20;

    if (pipeline_->getClassifier() != nullptr) {
        ofDrawBitmapString("[C]alibration\t[P]ipeline\t[A]nalysis\t[T]raining\tP[R]ediction",
                           left_margin, top_margin);
    } else {
        ofDrawBitmapString("[C]alibration\t[P]ipeline\t[A]nalysis",
                           left_margin, top_margin);
    }

    ofColor red = ofColor(0xFF, 0, 0);
    uint32_t tab_start = 0;
    uint32_t kTabWidth = 120;

    switch (fragment_) {
        case CALIBRATION:
            ofDrawColoredBitmapString(red, "[C]alibration\t",
                                      left_margin, top_margin);
            ofDrawBitmapString(kCalibrateInstruction,
                               left_margin, top_margin + margin);
            drawCalibration();
            break;
        case PIPELINE:
            ofDrawColoredBitmapString(red, "\t\t[P]ipeline\t",
                                      left_margin, top_margin);
            ofDrawBitmapString(kPipelineInstruction,
                               left_margin, top_margin + margin);
            drawLivePipeline();
            tab_start += kTabWidth;
            break;
        case ANALYSIS:
            ofDrawColoredBitmapString(red, "\t\t\t\t[A]nalysis",
                                      left_margin, top_margin);
            ofDrawBitmapString(kAnalysisInstruction,
                               left_margin, top_margin + margin);
            drawAnalysis();
            tab_start += 2 * kTabWidth;
            break;
        case TRAINING:
            if (pipeline_->getClassifier() == nullptr) { break; }
            ofDrawColoredBitmapString(red, "\t\t\t\t\t\t[T]raining",
                                      left_margin, top_margin);
            ofDrawBitmapString(kTrainingInstruction,
                               left_margin, top_margin + margin);
            drawTrainingInfo();
            tab_start += 3 * kTabWidth;
            break;
        case PREDICTION:
            if (pipeline_->getClassifier() == nullptr) { break; }
            ofDrawColoredBitmapString(red, "\t\t\t\t\t\t\t\tP[R]ediction",
                                      left_margin, top_margin);
            ofDrawBitmapString(kPredictionInstruction,
                               left_margin, top_margin + margin);
            drawPrediction();
            tab_start += 4 * kTabWidth;
            break;

        default:
            ofLog(OF_LOG_ERROR) << "Unknown tag!";
            break;
    }

    // Draw a shape like the following to indicate a tab.
    //          ______
    // ________|     |____________
    uint32_t bottom = top_margin + 5;
    uint32_t ceiling = 5;
    ofDrawLine(0, bottom, tab_start, bottom);
    ofDrawLine(tab_start, bottom, tab_start, ceiling);
    ofDrawLine(tab_start, ceiling, tab_start + kTabWidth, ceiling);
    ofDrawLine(tab_start + kTabWidth, ceiling, tab_start + kTabWidth, bottom);
    ofDrawLine(tab_start + kTabWidth, bottom, ofGetWidth(), bottom);

    // Status text at the bottom
    ofDrawBitmapString(status_text_, left_margin, ofGetHeight() - 20);

    gui_.draw();
}

void ofApp::drawInputs(uint32_t stage_left, uint32_t stage_top,
                      uint32_t stage_width, uint32_t stage_height) {
    if (istream_->getNumOutputDimensions() >= kTooManyFeaturesThreshold) {
        plot_inputs_snapshot_.draw(stage_left, stage_top, stage_width, stage_height * 0.75);
        plot_inputs_.draw(stage_left, stage_top + stage_height * 0.75, stage_width, stage_height * 0.25);
    } else {
        plot_inputs_.draw(stage_left, stage_top, stage_width, stage_height);
    }
}

void ofApp::drawCalibration() {
    uint32_t margin = 30;
    uint32_t stage_left = 10;
    uint32_t stage_top = 70;
    uint32_t stage_height = (ofGetHeight() - stage_top - margin * 3) / 2;
    uint32_t stage_width = ofGetWidth() - margin;

    // 1. Draw Input.
    ofPushStyle();
    plot_raw_.draw(stage_left, stage_top, stage_width, stage_height);
    ofPopStyle();
    stage_top += stage_height + margin;

    if (plot_calibrators_.size() == 0) return;

    float minY = plot_raw_.getRanges().first;
    float maxY = plot_raw_.getRanges().second;

    // 2. Draw Calibrators.
    int width = stage_width / plot_calibrators_.size();
    for (int i = 0; i < plot_calibrators_.size(); i++) {
        int x = stage_left + width * i;
        ofPushStyle();
        plot_calibrators_[i].setRanges(minY, maxY);
        plot_calibrators_[i].draw(x, stage_top, width, stage_height);
        ofPopStyle();
    }
}

void ofApp::drawLivePipeline() {
    // A Pipeline was parsed in the ofApp::setup function and here we simple
    // draw the pipeline information.
    uint32_t margin = 30;
    uint32_t stage_left = 10;
    uint32_t stage_top = 70;
    uint32_t stage_height = // Hacky math for dimensions.
            (ofGetHeight() - margin - stage_top) / (num_pipeline_stages_ + 1) - margin;
    uint32_t stage_width = ofGetWidth() - margin;

    // 1. Draw Input.
    ofPushStyle();
    drawInputs(stage_left, stage_top, stage_width, stage_height);
    ofPopStyle();
    stage_top += stage_height + margin;

    // 2. Draw pre-processing: iterate all stages.
    for (int i = 0; i < pipeline_->getNumPreProcessingModules(); i++) {
        // working on pre-processing stage i.
        ofPushStyle();
        plot_pre_processed_[i].
                draw(stage_left, stage_top, stage_width, stage_height);
        ofPopStyle();
        stage_top += stage_height + margin;
    }

    // 3. Draw features.
    for (int i = 0; i < pipeline_->getNumFeatureExtractionModules(); i++) {
        // working on feature extraction stage i.
        ofPushStyle();
        uint32_t height = plot_features_[i].size() == 1 ?
                stage_height : stage_height * kPipelineHeightWeight;
        for (int j = 0; j < plot_features_[i].size(); j++) {
            plot_features_[i][j].draw(stage_left, stage_top, stage_width, height);
            stage_top += height;
        }
        ofPopStyle();
        stage_top += margin;
    }
}

void ofApp::drawTrainingInfo() {
    uint32_t margin_left = 10;
    uint32_t margin_top = 70;
    uint32_t margin = 30;
    uint32_t stage_left = margin_left;
    uint32_t stage_top = margin_top;
    uint32_t stage_width = ofGetWidth() - margin;
    uint32_t stage_height =
        (ofGetHeight() - margin_top
         - margin // between the two plots
         - 72 // indices (1 / 3) and scores for training samples
         - 35 // bottom margin and status message
         - training_sample_guis_[0]->getHeight()) / 2;

    // need to create the paragraph and calculate its height to determine the
    // height of the stages (input and training sample plots).
    ofxParagraph paragraph(training_data_advice_, stage_width);
    paragraph.setFont("ofxbraitsch/fonts/Verdana.ttf", 11);
    paragraph.setColor(0xffffff);
    paragraph.setIndent(0);
    paragraph.setLeading(0);

    if (training_data_advice_ != "") {
        stage_height -= paragraph.getHeight() / 2;
    }

    // 1. Draw Input
    if (!is_in_feature_view_) {
        ofPushStyle();
        drawInputs(stage_left, stage_top, stage_width, stage_height);
        ofPopStyle();
        stage_top += stage_height + margin;
    }

    // 2. Draw advice for training data (if any)
    if (training_data_advice_ != "") {
        paragraph.draw(stage_left, stage_top);
        stage_top += paragraph.getHeight();
    }

    // 3. Draw samples
    // Currently we support kNumMaxLabels_ labels
    uint32_t width = stage_width / kNumMaxLabels_;
    float minY = plot_inputs_.getRanges().first;
    float maxY = plot_inputs_.getRanges().second;

    for (int i = 0; i < predicted_class_distances_.size() &&
                    i < predicted_class_likelihoods_.size(); i++) {
        ofColor backgroundColor, textColor;
        UINT label = predicted_class_labels_[i];
        uint32_t x = stage_left + (label - 1) * width;
        if (predicted_label_ == label) {
            backgroundColor = ofColor(255);
            textColor = ofColor(0);
        } else {
            backgroundColor = ofGetBackgroundColor();
            textColor = ofColor(255);
        }
        double likelihood = predicted_class_likelihoods_[i];
        double distance = predicted_class_distances_[i];
        ofDrawBitmapString(
            std::to_string((int) (likelihood * 100)) + "% (" +
            std::to_string(distance).substr(0,4) + ")",
            x, stage_top);
    }

    stage_top += 12;

    for (uint32_t i = 0; i < kNumMaxLabels_; i++) {
        uint32_t label = i + 1;
        uint32_t x = stage_left + i * width;
        plot_samples_[i].setRanges(minY, maxY, true);
        
        if (istream_->getNumOutputDimensions() >= kTooManyFeaturesThreshold) {
            plot_samples_snapshots_[i].draw(x, stage_top, width, 2 * stage_height / 3);
            plot_samples_[i].draw(x, stage_top + 2 * stage_height / 3, width, stage_height / 3);
        } else {
            plot_samples_[i].draw(x, stage_top, width, stage_height);
        }

        uint32_t num_samples = training_data_manager_.getNumSampleForLabel(label);
        ofDrawBitmapString(
            std::to_string(plot_sample_indices_[i] + 1) + " / " +
            std::to_string(training_data_manager_.getNumSampleForLabel(label)),
            x + width / 2 - 20,
            stage_top + stage_height + 20);
        if (plot_sample_indices_[i] > 0) {
            ofDrawBitmapString("<-", x, stage_top + stage_height + 20);
        }
        if (plot_sample_indices_[i] + 1 < num_samples) {
            ofDrawBitmapString("->", x + width - 20, stage_top + stage_height + 20);
        }
        plot_sample_button_locations_[i].first.set(x, stage_top + stage_height, 20, 20);
        plot_sample_button_locations_[i].second.set(x + width - 20, stage_top + stage_height, 20, 20);

        ofDrawBitmapString("Closeness To:", x, stage_top + stage_height + 32);

        for (int j = 0; j < kNumMaxLabels_; j++) {
            ofDrawBitmapString(std::to_string(j + 1), x + j * width / kNumMaxLabels_, stage_top + stage_height + 44);
            if (training_data_manager_.hasSampleClassLikelihoods(label, plot_sample_indices_[i])) {
                double score = training_data_manager_.getSampleClassLikelihoods(
                    label, plot_sample_indices_[i])[j + 1];
                if (i == j)
                    // the lower the score => the less likely it is to be
                    // classified correctly => the more red we want to draw =>
                    // the less green and blue it should have
                    ofSetColor(255, 255 * score, 255 * score);
                else
                    // the higher the score => the more confused it is with
                    // another class => the more red we want to draw => the
                    // less green and blue it should have
                    ofSetColor(255, 255 * (1.0 - score), 255 * (1.0 - score));
                ofDrawBitmapString(std::to_string((int) (score * 100)),
                    x + j * width / kNumMaxLabels_,
                    stage_top + stage_height + 56);
                ofSetColor(255);
            } else {
                ofDrawBitmapString("-", x + j * width / kNumMaxLabels_,
                    stage_top + stage_height + 56);
            }
        }

        // TODO(dmellis): only update these values when the screen size changes.
        training_sample_guis_[i]->setPosition(x + margin / 8,
                                              stage_top + stage_height + 60);
        training_sample_guis_[i]->setWidth(width - margin / 4);
        training_sample_guis_[i]->draw();
    }

    stage_top += stage_height + 60 + training_sample_guis_[0]->getHeight();

    if (!is_in_feature_view_) { return; }
    if (pipeline_->getNumFeatureExtractionModules() == 0) { return; }
    // 3. Features
    stage_top += margin * 2;
    for (uint32_t i = 0; i < kNumMaxLabels_; i++) {
        uint32_t x = stage_left + i * width;
        uint32_t y = stage_top;
        vector<Plotter> feature_plots = plot_sample_features_[i];
        uint32_t margin = 5;
        uint32_t height = stage_height / feature_plots.size() - margin;

        for (uint32_t j = 0; j < feature_plots.size(); j++) {
            pair<double, double> range = sample_feature_ranges_[j];

            feature_plots[j].setRanges(range.first, range.second);
            feature_plots[j].draw(x, y, width, height);
            y += height + margin;
        }
    }
}

void ofApp::drawAnalysis() {
    uint32_t margin_left = 10;
    uint32_t margin_top = 70;
    uint32_t margin = 30;
    uint32_t stage_left = margin_left;
    uint32_t stage_top = margin_top;
    uint32_t stage_width = ofGetWidth() - margin;
    uint32_t stage_height = (ofGetHeight() - 4 * margin - margin_top) / 2.25;

    // 1. Draw Input
    ofPushStyle();
    drawInputs(stage_left, stage_top, stage_width, stage_height);
    ofPopStyle();
    stage_top += stage_height + margin;

    ofPushStyle();
    plot_testdata_window_.draw(stage_left, stage_top, stage_width, stage_height);
    ofPopStyle();
    stage_top += stage_height + margin;

    ofPushStyle();
    plot_testdata_overview_.draw(stage_left, stage_top, stage_width, stage_height / 4);
    ofPopStyle();
}

void ofApp::drawPrediction() {
    uint32_t margin_left = 10;
    uint32_t margin_top = 70;
    uint32_t margin = 30;
    uint32_t stage_left = margin_left;
    uint32_t stage_top = margin_top;
    uint32_t stage_width = ofGetWidth() - margin;
    uint32_t stage_height = (ofGetHeight() - 3 * margin - margin_top) / 3;

    // 1. Draw Input
    ofPushStyle();
    drawInputs(stage_left, stage_top, stage_width, stage_height);
    ofPopStyle();
    stage_top += stage_height + margin;

    // 2. Draw Class Likelihoods
    if (class_likelihood_values_.size() > 0) {
        string s = "Class Likelihoods: ";
        for (int i = 0; i < class_likelihood_values_.size(); i++) {
            s += "[" + std::to_string(i + 1) + "]: " +
                 std::to_string((int) (class_likelihood_values_[i] * 100)) + "% ";
        }
        ofDrawBitmapString(s, stage_left, stage_top - margin / 3);
    }
    
    ofPushStyle();
    plot_class_likelihoods_.draw(stage_left, stage_top, stage_width, stage_height);
    ofPopStyle();
    stage_top += stage_height + margin;

    // 3. Draw Class Distances
    if (class_distance_values_.size() == 2) {
        ofDrawBitmapString(
            "Class Distance: " + std::to_string(class_distance_values_[1]) +
            " vs. Threshold: " + std::to_string(class_distance_values_[0]),
            stage_left, stage_top - margin / 3);
    }
    
    uint32_t height = stage_height / kNumMaxLabels_;
    double minDistance = 0.0, maxDistance = 1.0;
    for (int i = 0; i < kNumMaxLabels_; i++) {
        auto range = plot_class_distances_[i].getRanges();
        if (range.first < minDistance) minDistance = range.first;
        if (range.second > maxDistance) maxDistance = range.second;
    }

    for (int i = 0; i < kNumMaxLabels_; i++) {
        plot_class_distances_[i].setRanges(minDistance, maxDistance, true);
        plot_class_distances_[i].draw(stage_left, stage_top, stage_width, height);
        stage_top += height;
    }
}

void ofApp::exit() {
    if (training_thread_.joinable()) {
        training_thread_.join();
    }
    istream_->stop();

    // Save data here!
    if (should_save_calibration_data_ || should_save_training_data_ ||
        should_save_pipeline_ || should_save_test_data_) saveAll();
}

void ofApp::onDataIn(GRT::MatrixDouble input) {
    std::lock_guard<std::mutex> guard(input_data_mutex_);
    input_data_ = input;
}

//--------------------------------------------------------------
void ofApp::toggleFeatureView() {
    if (fragment_ != TRAINING) { return; }

    if (is_in_feature_view_) {
        is_in_feature_view_ = false;
    } else {
        is_in_feature_view_ = true;
        for (uint32_t i = 0; i < kNumMaxLabels_; i++) {
            populateSampleFeatures(i);
        }
    }
}

void ofApp::beginTrainModel() {
    // Update UI to reflect training starts.
    status_text_ = "Training the model . . .";
    is_training_scheduled_ = true;
    schedule_time_ = ofGetElapsedTimeMillis();
}

void ofApp::trainModel() {
    is_training_scheduled_ = false;

   // If prior training has not finished, we wait.
   if (training_thread_.joinable()) {
       training_thread_.join();
   }

   auto training_func = [this]() -> bool {
       ofLog() << "Training started";
       bool training_status = false;

       // Enable logging. GRT error logs will call ofApp::notify().
       GRT::ErrorLog::enableLogging(true);

       if (pipeline_->train(training_data_manager_.getAllData())) {
           ofLog() << "Training is successful";

           for (Plotter& plot : plot_samples_) {
               assert(true == plot.clearContentModifiedFlag());
           }

           should_save_pipeline_ = true;
           training_status = true;
       } else {
           ofLog(OF_LOG_ERROR) << "Failed to train the model";
       }

       // Stop logging.
       GRT::ErrorLog::enableLogging(false);
       return training_status;
   };

   // TODO(benzh) Fix data race issue later.
   if (training_func()) {
       afterTrainModel();
       status_text_ = "Training was successful";
   }
}

void ofApp::afterTrainModel() {
    scoreTrainingData(use_leave_one_out_scoring_);
    fragment_ = TRAINING;
    runPredictionOnTestData();
    updateTestWindowPlot();
    pipeline_->reset();
    for (int i = 0; i < plot_class_distances_.size(); i++)
        plot_class_distances_[i].reset();
}

void ofApp::scoreTrainingData(bool leaveOneOut) {
    for (int label = 1; label <= training_data_manager_.getNumLabels(); label++) {
        // No point in doing leave-one-out scoring for labels w/ one sample.
        if (leaveOneOut && training_data_manager_.getNumSampleForLabel(label) == 1)
            continue;

        for (int i = 0; i < training_data_manager_.getNumSampleForLabel(label); i++) {
            GRT::MatrixDouble sample =
                training_data_manager_.getSample(label, (leaveOneOut ? 0 : i));

            if (leaveOneOut) {
                training_data_manager_.deleteSample(label, 0);
                pipeline_->train(training_data_manager_.getAllData());
            }

            //ofLog(OF_LOG_NOTICE) << "sample " << i << " (class " << label << "):";
            vector<double> likelihoods(training_data_manager_.getNumLabels() + 1, 0.0);
            for (int j = 0; j < sample.getNumRows(); j++) {
                pipeline_->predict(sample.getRowVector(j));
                auto l = pipeline_->getClassLikelihoods();
                for (int k = 0; k < l.size(); k++) {
                    likelihoods[pipeline_->getClassLabels()[k]] += l[k];
                }
            }
            double sum = 0.0;
            for (int j = 0; j < likelihoods.size(); j++) {
                //ofLog(OF_LOG_NOTICE) << "\t" << (j + 1) << ": " << likelihoods[j] << "%";
                sum += likelihoods[j];
            }
            for (int j = 0; j < likelihoods.size(); j++) {
                likelihoods[j] /= (sum == 0.0 ? 1e-9 : sum);
                //ofLog(OF_LOG_NOTICE) << "\t" << (j + 1) << ": " << likelihoods[j] << "%";
            }

            if (leaveOneOut) training_data_manager_.addSample(label, sample);

            training_data_manager_.setSampleClassLikelihoods(label,
                (leaveOneOut ? training_data_manager_.getNumSampleForLabel(label) - 1 : i),
                likelihoods);

            pipeline_->reset();
        }
    }

    if (leaveOneOut) pipeline_->train(training_data_manager_.getAllData());
}

void ofApp::scoreImpactOfTrainingSample(int label, const MatrixDouble &sample) {
    if (!pipeline_->getTrained()) return; // can't calculate a score

    GestureRecognitionPipeline p(*pipeline_);

    p.reset();
    //std::cout << "Scoring sample impact: " << std::endl;
    double score = 0.0;
    int num_non_zero = 0;
    for (int j = 0; j < sample.getNumRows(); j++) {
        pipeline_->predict(sample.getRowVector(j));
        auto l = pipeline_->getClassLikelihoods();
        bool non_zero = false;
        for (int k = 0; k < l.size(); k++) {
            if (l[k] > 1e-9) non_zero = true;
            if (pipeline_->getClassLabels()[k] == label) {
                //std::cout << l[k] << " ";
                score += l[k];
            }
        }
        if (non_zero) num_non_zero++;
        //std::cout << non_zero << std::endl;
    }
    //std::cout << std::to_string(score / num_non_zero);
    status_text_ = "Information gain of sample: " +
        std::to_string((int) (100 * -log(score / num_non_zero))) + "%";
}

void ofApp::reloadPipelineModules() {
    pipeline_->clearAll();
    ::setup();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if (is_in_renaming_) {
        // Add normal characters.
        if (key >= 32 && key <= 126) {
            // key code 32 is for space, we remap it to '_'.
            key = (key == 32) ? '_' : key;
            rename_title_ += key;
            return;
        }

        switch (key) {
          case OF_KEY_BACKSPACE:
            rename_title_ = rename_title_.substr(0, rename_title_.size() - 1);
            break;
          case OF_KEY_RETURN:
            renameTrainingSampleDone();
            return;
          default:
            break;
        }

        plot_samples_[rename_target_ - 1].setTitle(display_title_);
        return;
    }

    if (is_in_history_recording_) { return; }

    // If in relabeling, take action at key release stage.
    if (is_in_relabeling_) { return; }

    if (key >= '1' && key <= '9') {
        if (!is_recording_) {
            is_recording_ = true;
            label_ = key - '0';
            sample_data_.clear();
        }
    }

    switch (key) {
        case 'r':
            if (!is_recording_) {
                is_recording_ = true;
                label_ = 255;
                sample_data_.clear();
                test_data_.clear();
                plot_testdata_window_.reset();
            }
            break;
        case 'f': toggleFeatureView(); break;
        case 'l': loadAll(); break;
        case 'L':
            if (fragment_ == CALIBRATION) loadCalibrationDataWithPrompt();
            else if (fragment_ == TRAINING) loadTrainingDataWithPrompt();
            else if (fragment_ == ANALYSIS) loadTestDataWithPrompt();
            break;
        case 'p': {
            istream_->toggle();
            enable_history_recording_ = !enable_history_recording_;
            if (!enable_history_recording_) {
                class_likelihood_values_.resize(0);
                class_distance_values_.resize(0);
            }
            input_data_.clear();
            break;
        }
        case 'a': saveAll(true); break;
        case 's': saveAll(); break;
        case 'S':
            if (fragment_ == CALIBRATION) saveCalibrationDataWithPrompt();
            else if (fragment_ == TRAINING) saveTrainingDataWithPrompt();
            else if (fragment_ == ANALYSIS) saveTestDataWithPrompt();
            break;
        case 't': beginTrainModel(); break;

        // Tab related
        case 'C': fragment_ = CALIBRATION; break;
        case 'P': fragment_ = PIPELINE; break;
        case 'T': {
            if (pipeline_->getClassifier() != nullptr) {
                fragment_ = TRAINING;
            }
            break;
        }
        case 'R': {
            if (pipeline_->getClassifier() != nullptr) {
                fragment_ = PREDICTION;
            }
            break;
        }
        case 'A': fragment_ = ANALYSIS; break;
    }
}

void ofApp::keyReleased(int key) {
    if (is_in_renaming_) { return; }
    if (is_in_history_recording_) {
        // Pressing 1-9 will turn the samples into training data
        if (key >= '1' && key <= '9') {
            label_ = key - '0';
            training_data_manager_.addSample(key - '0', sample_data_);
            int num_samples = training_data_manager_.getNumSampleForLabel(label_);

            plot_samples_[label_ - 1].setData(sample_data_);
            plot_sample_indices_[label_ - 1] = num_samples - 1;
            
            updatePlotSamplesSnapshot(label_ - 1);

            should_save_training_data_ = true;
        }
        // Reset the status of the GUI
        is_in_history_recording_ = false;
        status_text_ = "";
        plot_inputs_.clearSelection();
        return;
    }

    if (is_in_relabeling_ && key >= '1' && key <= '9') {
        doRelabelTrainingSample(relabel_source_, key - '0');
        is_in_relabeling_ = false;
        return;
    }

    is_recording_ = false;
    if (key >= '1' && key <= '9') {
        if (fragment_ == CALIBRATION) {
            if (calibrator_ == nullptr) { return; }

            vector<CalibrateProcess>& calibrators = calibrator_->getCalibrateProcesses();
            if (label_ - 1 < calibrators.size()) {
                CalibrateResult result =
                    calibrators[label_ - 1].calibrate(sample_data_);

                if (result.getResult() != CalibrateResult::FAILURE) {
                    plot_calibrators_[label_ - 1].setData(sample_data_);
                    plot_inputs_.reset();
                    should_save_calibration_data_ = true;
                }

                status_text_ = calibrators[label_ - 1].getName() +
                        " calibration: " + result.getResultString() + ": " +
                        result.getMessage();
            }
        } else if (fragment_ == TRAINING) {
            if (training_sample_checker_) {
                TrainingSampleCheckerResult result =
                    training_sample_checker_(sample_data_);
                status_text_ = plot_samples_[label_ - 1].getTitle() +
                    " check: " + result.getMessage();

                // Don't save sample if the checker returns failure.
                if (result.getResult() == TrainingSampleCheckerResult::FAILURE)
                    return;
            }

            scoreImpactOfTrainingSample(label_, sample_data_);

            training_data_manager_.addSample(label_, sample_data_);
            int num_samples = training_data_manager_.getNumSampleForLabel(label_);

            plot_samples_[label_ - 1].setData(sample_data_);
            plot_sample_indices_[label_ - 1] = num_samples - 1;
            
            updatePlotSamplesSnapshot(label_ - 1);

            should_save_training_data_ = true;
        }
    }

    if (key == 'r') {
        test_data_ = sample_data_;
        plot_testdata_overview_.setData(test_data_);
        runPredictionOnTestData();
        updateTestWindowPlot();
        should_save_test_data_ = true;
    }
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ) {

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {
    // Navigating between samples (samples themselves are not changed).
    for (int i = 0; i < kNumMaxLabels_; i++) {
        int label = i + 1;
        if (plot_sample_button_locations_[i].first.inside(x, y)) {
            if (plot_sample_indices_[i] > 0) {
                plot_sample_indices_[i]--;
                plot_samples_[i].setData(
                    training_data_manager_.getSample(label, plot_sample_indices_[i]));
                assert(true == plot_samples_[i].clearContentModifiedFlag());
                updatePlotSamplesSnapshot(i);
                populateSampleFeatures(i);
            }
        }
        if (plot_sample_button_locations_[i].second.inside(x, y)) {
            if (plot_sample_indices_[i] + 1 < training_data_manager_.getNumSampleForLabel(label)) {
                plot_sample_indices_[i]++;
                plot_samples_[i].setData(
                    training_data_manager_.getSample(label, plot_sample_indices_[i]));
                assert(true == plot_samples_[i].clearContentModifiedFlag());
                updatePlotSamplesSnapshot(i);
                populateSampleFeatures(i);
            }
        }
    }

    // Tab click detection
    const uint32_t left_margin = 10;
    const uint32_t top_margin = 20;
    const uint32_t tab_width = 120;
    if (x > left_margin && y < top_margin + 5) {
        if (x < left_margin + tab_width) {
            fragment_ = CALIBRATION;
        } else if (x < left_margin + 2 * tab_width) {
            fragment_ = PIPELINE;
        } else if (x < left_margin + 3 * tab_width) {
            fragment_ = ANALYSIS;
        } else if (x < left_margin + 4 * tab_width
                   && pipeline_->getClassifier() != nullptr) {
            fragment_ = TRAINING;
        } else if (x < left_margin + 5 * tab_width
                   && pipeline_->getClassifier() != nullptr) {
            fragment_ = PREDICTION;
        }
    }
}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {

}

string encodeName(const string &name) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (auto i = name.begin(); i != name.end(); ++i) {
        string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << setw(2) << int((unsigned char) c);
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

string decodeName(const string &from) {
    ostringstream escaped;
    escaped.fill('0');

    // Convert from hex to decimal
    auto from_hex = [](char ch) {
        return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
    };

    for (auto i = from.begin(), n = from.end(); i != n; ++i) {
        string::value_type c = (*i);
        if (c == '%') {
            if (i[1] && i[2]) {
                char h = from_hex(i[1]) << 4 | from_hex(i[2]);
                escaped << h;
                i += 2;
            }
        } else if (c == '+') {
            escaped << ' ';
        } else {
            escaped << c;
        }
    }

    return escaped.str();
}
