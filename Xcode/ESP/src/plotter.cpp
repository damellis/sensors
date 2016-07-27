#include "plotter.h"

using std::string;

Plotter::Plotter() :
        initialized_(false), is_content_modified_(false), is_in_renaming_(false),
        lock_ranges_(false), minY_(0), maxY_(0) {
    // Constructor
}

bool Plotter::setup(uint32_t num_dimensions, string title, string subtitle) {
    num_dimensions_ = num_dimensions;
    title_ = title;
    subtitle_ = subtitle;

    colors_.resize(num_dimensions_);
    // Setup the default colors_
    if (num_dimensions >= 1) colors_[0] = ofColor(255, 0, 0); // red
    if (num_dimensions >= 2) colors_[1] = ofColor(0, 255, 0); // green
    if (num_dimensions >= 3) colors_[2] = ofColor(0, 0, 255); // blue

    // Randomize the remaining colors_
    for (uint32_t n = 3; n < num_dimensions_; n++){
        colors_[n][0] = ofRandom(50, 255);
        colors_[n][1] = ofRandom(50, 255);
        colors_[n][2] = ofRandom(50, 255);
    }

    initialized_ = true;
    return true;
}

bool Plotter::setData(const GRT::MatrixDouble& data) {
    x_start_ = 0;
    x_end_ = 0;
    data_.clear();
    for (int i = 0; i < data.getNumRows(); i++) push_back(data.getRowVector(i));
    is_content_modified_ = true;
    return true;
}

bool Plotter::clearContentModifiedFlag() {
    is_content_modified_ = false;
    return true;
}

bool Plotter::push_back(const vector<double>& data_point) {
    data_.push_back(data_point);
    for (double d : data_point) {
        if (d > maxY_) { maxY_ = d; }
        if (d < minY_) { minY_ = d; }
    }
    is_content_modified_ = true;
    return true;
}

bool Plotter::setRanges(float minY, float maxY, bool lockRanges) {
    if (minY > maxY) { return false; }

    default_minY_ = minY_ = minY;
    default_maxY_ = maxY_ = maxY;
    lock_ranges_ = lockRanges;
    return true;
}

std::pair<float, float> Plotter::getRanges() {
    return lock_ranges_ ?
            std::make_pair(default_minY_, default_maxY_) :
            std::make_pair(minY_, maxY_);
}

bool Plotter::setColorPalette(const vector<ofColor>& colors) {
    if (colors.size() == num_dimensions_) {
        colors_ = colors;
        return true;
    } else {
        return false;
    }
}

bool Plotter::setTitle(const string& title) {
    title_ = title;
    return true;
}

void Plotter::renameTitleStart() {
    is_in_renaming_ = true;
}

void Plotter::renameTitleDone() {
    is_in_renaming_ = false;
}

const string& Plotter::getTitle() const {
    return title_;
}

bool Plotter::draw(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    x_ = x;
    y_ = y;
    w_ = w;
    h_ = h;

    if (!initialized_) return false;

    ofPushMatrix();
    ofPushStyle();

    ofEnableAlphaBlending();
    ofTranslate(x, y);

    // Draw the background
    ofFill();
    ofSetColor(0, 0, 0);
    ofDrawRectangle(0, 0, w, h);

    // Draw the selection.
    ofSetColor(0x1F, 0x1F, 0x1F);
    if (x_start_ > 0 && x_end_ > x_start_) {
        ofDrawRectangle(x_start_, 0, x_end_ - x_start_, h);
    }

    // Draw the axis lines
    ofSetColor(255, 255, 255);
    ofDrawLine(-5, h, w+5, h); // X Axis
    ofDrawLine(0, -5, 0, h+5); // Y Axis

    // Draw the timeseries
    float xPos = 0;
    float x_step = 1.0 * w_ / data_.getNumRows();

    uint32_t index = 0;
    ofNoFill();
    for(uint32_t n = 0; n < num_dimensions_; n++){
        xPos = 0;
        ofSetColor(colors_[n][0], colors_[n][1], colors_[n][2]);
        ofBeginShape();
        for(uint32_t i = 0; i < data_.getNumRows(); i++){
            float min = lock_ranges_ ? default_minY_ : minY_;
            float max = lock_ranges_ ? default_maxY_ : maxY_;
            ofVertex(xPos, ofMap(data_[i][n], min, max, h, 0, true));
            xPos += x_step;
        }
        ofEndShape(false);
    }

    // Draw the title
    int ofBitmapFontHeight = 14;
    int textX = 10;
    int textY = ofBitmapFontHeight + 5;
    if (title_ != ""){
        string display_title = title_;

        // If modified and not in renaming, show a start
        if (is_content_modified_ && !is_in_renaming_) {
            display_title += "*";
        }
        ofSetColor(0xFF, 0xFF, 0xFF);
        ofDrawBitmapString(display_title, textX, textY);
    }

    if (subtitle_ != "") {
        ofSetColor(0x99, 0x99, 0x99);
        ofDrawBitmapString(subtitle_, textX, textY + 15);
    }

    ofPopStyle();
    ofPopMatrix();
    return true;
}

bool Plotter::reset() {
    if (!initialized_) return false;
    x_start_ = 0;
    x_end_ = 0;
    minY_ = 0;
    maxY_ = 0;
    data_.clear();
    return true;
}

bool Plotter::clearData() {
    if (!initialized_) return false;
    data_.clear();
    return true;
}

void Plotter::onRangeSelected(const onRangeSelectedCallback& cb, void* data) {
    range_selected_callback_ = cb;
    callback_data_ = data;
    ofAddListener(ofEvents().mousePressed, this, &Plotter::startSelection);
    ofAddListener(ofEvents().mouseDragged, this, &Plotter::duringSelection);
    ofAddListener(ofEvents().mouseReleased, this, &Plotter::endSelection);
}

std::pair<uint32_t, uint32_t> Plotter::getSelection() {
    return std::make_pair(x_start_ / x_step_, x_end_ / x_step_);
}


bool Plotter::contains(uint32_t x, uint32_t y) {
    if (x_ <= x && x <= x_ + w_ && y_ <= y && y <= y_ + h_) {
        return true;
    } else {
        return false;
    }
}

void Plotter::normalize() {
    pair<uint32_t, uint32_t> sel = std::minmax(x_click_, x_release_);
    x_start_ = sel.first;
    x_end_ = sel.second;
}

void Plotter::startSelection(ofMouseEventArgs& arg) {
    // Only tracks if point is inside and data_ has rows.
    if (contains(arg.x, arg.y) && data_.getNumRows() > 0) {
        x_click_ = arg.x - x_;
        is_tracking_mouse_ = true;
    }
}

void Plotter::duringSelection(ofMouseEventArgs& arg) {
    if (is_tracking_mouse_) {
        if (contains(arg.x, arg.y)) {
            x_release_ = arg.x - x_;
            normalize();
        }
    }
}

void Plotter::endSelection(ofMouseEventArgs& arg) {
    if (is_tracking_mouse_) {
        if (contains(arg.x, arg.y)) {
            x_release_ = arg.x - x_;
            normalize();
        }

        if (range_selected_callback_ != nullptr) {
            CallbackArgs args;
            args.start = x_start_;
            args.end = x_end_;
            args.data = callback_data_;
            range_selected_callback_(args);
        }
    }
    is_tracking_mouse_ = false;
}
