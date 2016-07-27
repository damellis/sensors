#pragma once

#include "ofMain.h"
#include "ofxGrt.h"

using std::string;

class InteractivePlot {
  public:
    struct RangeSelectedCallbackArgs {
        InteractivePlot *source;
        uint32_t start;
        uint32_t end;
        void* data;
    };

    struct ValueHighlightedCallbackArgs {
        InteractivePlot *source;
        uint32_t index;
        void* data;
    };

    struct NoValueHighlightedCallbackArgs {
        InteractivePlot *source;
        void* data;
    };

    InteractivePlot() :
            x_click_(0), x_release_(0),
            x_start_(0), x_end_(0),
            is_tracking_mouse_(false) {
    }

    typedef std::function<void(RangeSelectedCallbackArgs)> onRangeSelectedCallback;
    typedef std::function<void(ValueHighlightedCallbackArgs)> onValueHighlightedCallback;
    typedef std::function<void(NoValueHighlightedCallbackArgs)> onNoValueHighlightedCallback;

    void onRangeSelected(const onRangeSelectedCallback& cb, void* data) {
        range_selected_callback_ = cb;
        range_selected_callback_data_ = data;
        ofAddListener(ofEvents().mousePressed,
                      this, &InteractivePlot::startSelection);
        ofAddListener(ofEvents().mouseDragged,
                      this, &InteractivePlot::duringSelection);
        ofAddListener(ofEvents().mouseReleased,
                      this, &InteractivePlot::endSelection);
    }

    template<typename T1, typename arg, class T>
    void onRangeSelected(T1* owner, void (T::*listenerMethod)(arg), void* data) {
        using namespace std::placeholders;
        onRangeSelected(std::bind(listenerMethod, owner, _1), data);
    }

    void onValueHighlighted(const onValueHighlightedCallback& cb, void* data) {
        value_highlighted_callback_ = cb;
        value_highlighted_callback_data_ = data;
        ofAddListener(ofEvents().mouseDragged,
                      this, &InteractivePlot::mouseMoved);
        ofAddListener(ofEvents().mouseMoved,
                      this, &InteractivePlot::mouseMoved);
    }

    template<typename T1, typename arg, class T>
    void onValueHighlighted(T1* owner, void (T::*listenerMethod)(arg), void* data) {
        using namespace std::placeholders;
        onValueHighlighted(std::bind(listenerMethod, owner, _1), data);
    }

    void onNoValueHighlighted(const onNoValueHighlightedCallback& cb, void* data) {
        no_value_highlighted_callback_ = cb;
        no_value_highlighted_callback_data_ = data;
        ofAddListener(ofEvents().mouseMoved,
                      this, &InteractivePlot::mouseMoved);
    }

    template<typename T1, typename arg, class T>
    void onNoValueHighlighted(T1* owner, void (T::*listenerMethod)(arg), void* data) {
        using namespace std::placeholders;
        onNoValueHighlighted(std::bind(listenerMethod, owner, _1), data);
    }

    virtual MatrixDouble getData(uint32_t x_start_idx, uint32_t x_end_idx) = 0;
    virtual vector<double> getData(uint32_t x_idx) = 0;
    
     std::pair<uint32_t, uint32_t> getSelection() {
        return std::make_pair(mouseCoordinateToIndex(x_start_),
                              mouseCoordinateToIndex(x_end_));
    }
    
    void clearSelection() {
        x_start_ = 0;
        x_end_ = 0;
    }

  protected:
    virtual uint32_t mouseCoordinateToIndex(uint32_t x) = 0;
  
    bool contains(uint32_t x, uint32_t y) {
        return (x_ <= x && x <= x_ + w_ && y_ <= y && y <= y_ + h_) ?
                true : false;
    }
    
    void mouseMoved(ofMouseEventArgs& arg) {
        // Only tracks if point is inside
        if (contains(arg.x, arg.y)) {
            x_move_ = arg.x - x_;
            if (value_highlighted_callback_ != nullptr) {
                ValueHighlightedCallbackArgs args;
                args.source = this;
                args.index = mouseCoordinateToIndex(x_move_);
                args.data = value_highlighted_callback_data_;
                value_highlighted_callback_(args);
            }
        } else {
            if (no_value_highlighted_callback_ != nullptr) {
                NoValueHighlightedCallbackArgs args;
                args.source = this;
                args.data = no_value_highlighted_callback_data_;
                no_value_highlighted_callback_(args);
            }
        }
    }

    void startSelection(ofMouseEventArgs& arg) {
        // Only tracks if point is inside
        if (contains(arg.x, arg.y)) {
            x_click_ = arg.x - x_;
            is_tracking_mouse_ = true;
        }
    }

    void duringSelection(ofMouseEventArgs& arg) {
        if (is_tracking_mouse_) {
            if (contains(arg.x, arg.y)) {
                x_release_ = arg.x - x_;
                normalize();
            }
        }
    }

    void normalize() {
        pair<uint32_t, uint32_t> sel = std::minmax(x_click_, x_release_);
        x_start_ = sel.first;
        x_end_ = sel.second;
    }

    void endSelection(ofMouseEventArgs& arg) {
        if (is_tracking_mouse_) {
            if (contains(arg.x, arg.y)) {
                x_release_ = arg.x - x_;
                normalize();
            }

            if (range_selected_callback_ != nullptr) {
                RangeSelectedCallbackArgs args;
                args.source = this;
                args.start = mouseCoordinateToIndex(x_start_);
                args.end = mouseCoordinateToIndex(x_end_);
                args.data = range_selected_callback_data_;
                range_selected_callback_(args);
            }
        }
        is_tracking_mouse_ = false;
    }

    // These need to be saved by the sub-class's draw() function so that the
    // plot knows where it is (and can convert mouse coordinates to plot
    // coordinates).
    uint32_t x_;
    uint32_t y_;
    uint32_t w_;
    uint32_t h_;
    
    uint32_t x_move_;

    // x_click_ and x_release_ keeps track of where the mouse is clicked and
    // releasesd. Their values are normalized by subtracting x_.
    uint32_t x_click_;
    uint32_t x_release_;

    // x_start_ and x_end_ are values that are reported to users of this class
    // for selections. When selection happens, x_start_ is always smaller than
    // x_end_.
    uint32_t x_start_;
    uint32_t x_end_;

    // To avoid accidental tracking (such as out of range or when no data).
    bool is_tracking_mouse_;

    onRangeSelectedCallback range_selected_callback_;
    void* range_selected_callback_data_;

    onValueHighlightedCallback value_highlighted_callback_;
    void* value_highlighted_callback_data_;
    
    onNoValueHighlightedCallback no_value_highlighted_callback_;
    void *no_value_highlighted_callback_data_;
};

// The Plotter class plots fixed length samples and allows for interaction
// using the InteractivePlot API.
class Plotter : public InteractivePlot {
  public:
    Plotter();

    bool setup(uint32_t num_dimensions, string title, string subtitle = "");
    bool setData(const GRT::MatrixDouble& data);

    bool clearContentModifiedFlag();

    bool push_back(const vector<double>& data_point);

    virtual GRT::MatrixDouble& getData() { return data_; }
    
    virtual MatrixDouble getData(uint32_t x_start_idx, uint32_t x_end_idx) {
        // dataBuffer is a "CircularBuffer< vector<float> >" inside
        // ofxGrtTimeseriesPlot.
        MatrixDouble selected_data;
        for (uint32_t i = x_start_idx; i < x_end_idx && i < data_.getSize(); i++) {
            vector<double> v_double(data_.getRowVector(i).begin(),
                                    data_.getRowVector(i).end());
            selected_data.push_back(v_double);
        }
        return selected_data;
    }
    
    virtual vector<double> getData(uint32_t x_idx) {
        return vector<double>(data_.getRowVector(x_idx).begin(),
                              data_.getRowVector(x_idx).end());
    }
    
    bool setRanges(float minY, float maxY, bool lockRanges = false);
    std::pair<float, float> getRanges();
    
    bool setColorPalette(const vector<ofColor>& colors);

    bool setTitle(const string& title);
    void renameTitleStart();
    void renameTitleDone();

    const string& getTitle() const;

    bool draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

    bool reset();

    bool clearData();

  protected:
    virtual uint32_t mouseCoordinateToIndex(uint32_t x) {
        float x_step = w_ * 1.0 / data_.getNumRows();
        return std::min(std::max((uint32_t) 0, (uint32_t) (x / x_step)), data_.getNumRows() - 1);
    }
  
  private:
    bool initialized_;
    bool is_content_modified_;
    bool is_in_renaming_;
    uint32_t num_dimensions_;
    vector<ofColor> colors_;
    string title_;
    string subtitle_;
    bool lock_ranges_;
    float minY_, default_minY_;
    float maxY_, default_maxY_;
    GRT::MatrixDouble data_;
};

class InteractiveTimeSeriesPlot : public ofxGrtTimeseriesPlot, public InteractivePlot {
  public:
    bool draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
        x_ = x;
        y_ = y;
        w_ = w;
        h_ = h;
        ofxGrtTimeseriesPlot::draw(x, y, w, h);

        // Draw the selection.
        if (x_start_ > 0 && x_end_ > x_start_) {
            ofEnableAlphaBlending();
            // Draw border
            ofSetColor(0xFF, 0xFF, 0xFF, 0x7F);
            ofDrawRectangle(x_ + x_start_, y_, x_end_ - x_start_, h);

            // Fill the rectangle
            ofFill();
            ofSetColor(0xFF, 0xFF, 0xFF, 0x2F);
            ofDrawRectangle(x_ + x_start_, y_, x_end_ - x_start_, h);
        }
        return true;
    }

    virtual MatrixDouble getData(uint32_t x_start_idx, uint32_t x_end_idx) {
        // dataBuffer is a "CircularBuffer< vector<float> >" inside
        // ofxGrtTimeseriesPlot.
        MatrixDouble selected_data;
        for (uint32_t i = x_start_idx; i < x_end_idx && i < dataBuffer.getSize(); i++) {
            vector<double> v_double(dataBuffer[i].begin(), dataBuffer[i].end());
            selected_data.push_back(v_double);
        }
        return selected_data;
    }
    
    virtual vector<double> getData(uint32_t x_idx) {
        return vector<double>(dataBuffer[x_idx].begin(), dataBuffer[x_idx].end());
    }
    
  protected:
    virtual uint32_t mouseCoordinateToIndex(uint32_t x) {
        float x_step = w_ * 1.0 / timeseriesLength;
        return std::min(std::max((uint32_t) 0, (uint32_t) (x / x_step)), timeseriesLength - 1);
    }
};
