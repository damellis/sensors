#include "IStream.h"
#include "ofApp.h"

#include <chrono>         // std::chrono::milliseconds
#include <thread>         // std::this_thread::sleep_for

void useInputStream(InputStream &stream) {
    ((ofApp *) ofGetAppPtr())->useIStream(stream);
}

void usePipeline(GRT::GestureRecognitionPipeline &pipeline) {
    ((ofApp *) ofGetAppPtr())->usePipeline(pipeline);
}

InputStream::InputStream() : data_ready_callback_(nullptr) {}

vector<double> InputStream::normalize(vector<double> input) {
    if (vectorNormalizer_ != nullptr) {
        return vectorNormalizer_(input);
    } else if (normalizer_ != nullptr) {
        vector<double> output;
        std::transform(input.begin(), input.end(), back_inserter(output), normalizer_);
        return output;
    } else {
        return input;
    }
}

void InputStream::setLabelsForAllDimensions(const vector<string> labels) {
    InputStream_labels_ = labels;
}

void InputStream::setLabelsForAllDimensions(std::initializer_list<string> list) {
    if (list.size() != getNumOutputDimensions()) { return; }
    vector<string> labels(list);
    InputStream_labels_ = labels;
}

const vector<string>& InputStream::getLabels() const {
    return InputStream_labels_;
}

AudioStream::AudioStream(uint32_t downsample_rate)
        : downsample_rate_(downsample_rate),
          sound_stream_(new ofSoundStream()) {
    setup_successful_ = sound_stream_->setup(this, 0, 2,
                                             kOfSoundStream_SamplingRate,
                                             kOfSoundStream_BufferSize,
                                             kOfSoundStream_nBuffers);
    sound_stream_->stop();
}

bool AudioStream::start() {
    if (!setup_successful_) return false;
    if (!has_started_) {
        sound_stream_->start();
        has_started_ = true;
    }
    return true;
}

void AudioStream::stop() {
    if (has_started_) {
        sound_stream_->stop();
        has_started_ = false;
    }
}

int AudioStream::getNumInputDimensions() {
    return 1; // set by the call to sound_stream->setup() above
}

void AudioStream::audioIn(float* input, int buffer_size, int nChannel) {
    // set nChannelOut as 1 to load only a single channel (left).
    int nChannelOut = 1;
    GRT::MatrixDouble data(buffer_size / nChannel / downsample_rate_, nChannelOut);

    for (int i = 0; i < buffer_size / nChannel / downsample_rate_; i++)
        for (int j = 0; j < nChannelOut; j++)
            data[i][j] = input[i * nChannel * downsample_rate_ + j];

    if (data_ready_callback_ != nullptr) {
        data_ready_callback_(data);
    }
}

AudioFileStream::AudioFileStream(char *file, bool loop) {
    player_.load(file);
    player_.setLoop(loop);
}

bool AudioFileStream::start() {
    if (!has_started_) {
        player_.play();
        update_thread_.reset(new std::thread(&AudioFileStream::readSpectrum, this));
        has_started_ = true;
    }
    
    return true;
}

void AudioFileStream::stop() {
    player_.stop();
    has_started_ = false;
    if (update_thread_ != nullptr && update_thread_->joinable()) {
        update_thread_->join();
    }
}

void AudioFileStream::readSpectrum() {
    while (has_started_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (44100 / 1024)));
        float *spectrum = ofSoundGetSpectrum(512);
        VectorDouble data(spectrum, spectrum + 512);
        MatrixDouble out; out.push_back(data);
        if (data_ready_callback_ != nullptr) data_ready_callback_(out);
    }
}

int AudioFileStream::getNumInputDimensions() {
    return 512;
}

BaseSerialStream::BaseSerialStream(uint32_t port, uint32_t baud, int dimensions)
        : port_(port), baud_(baud), dimensions_(dimensions), serial_(new ofSerial()) {
    // Print all devices for convenience.
    // serial_->listDevices();
}

bool BaseSerialStream::start() {
    if (port_ == -1) {
        ofLog(OF_LOG_ERROR) << "USB Port has not been properly set";
        return false;
    }

    if (!has_started_) {
        if (!serial_->setup(port_, baud_)) return false;
        reading_thread_.reset(new std::thread(&BaseSerialStream::readSerial, this));
        has_started_ = true;
    }

    return true;
}

void BaseSerialStream::stop() {
    has_started_ = false;
    if (reading_thread_ != nullptr && reading_thread_->joinable()) {
        reading_thread_->join();
    }
}

int BaseSerialStream::getNumInputDimensions() {
    return dimensions_;
}

void BaseSerialStream::readSerial() {
    // TODO(benzh) This readSerial is running in a different thread
    // and performing a busy polling (100% CPU usage). Should be
    // optimized.
    int sleep_time = 1000 / (baud_ / 10); // sleep about long enough to receive a byte
    ofLog() << "Serial port will be read every " << sleep_time << " ms";
    while (has_started_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        const int kBufSize = 32;
        unsigned char buf[kBufSize];
        while (serial_->available() > 0) {
            int result = serial_->readBytes(buf, kBufSize);
            
            if ( result == OF_SERIAL_ERROR ) {
                ofLog( OF_LOG_ERROR, "unrecoverable error reading from serial" );
                break;
            } else if ( result != OF_SERIAL_NO_DATA ) {
                buffer_.insert(buffer_.end(), buf, buf + result);
            }
        }
        
        parseSerial(buffer_);
    }
}

void BinaryIntArraySerialStream::parseSerial(vector<unsigned char> &buffer) {
    auto start = std::find(buffer.begin(), buffer.end(), 0); // look for packet start
    if (start != buffer.end()) {
        buffer.erase(buffer.begin(), start); // advance to start of packet
        if (buffer.size() >= 3) { // 0, LSB(n), MSB(n)
            unsigned char checksum = 0;
            int LSB = buffer[1]; checksum += LSB;
            int MSB = buffer[2]; checksum += MSB;
            int n = ((MSB & 0x7F) << 7) | (LSB & 0x7F);
            if (buffer.size() >= 4 + 2 * n) {
                vector<double> vals;
                //std::cout << "Got array of " << n << " bytes: " << buffer.size();
                for (int j = 3; j < (3 + 2 * n);) {
                    LSB = buffer[j++]; checksum += LSB;
                    MSB = buffer[j++]; checksum += MSB;
                    int val = ((MSB & 0x7F) << 7) | (LSB & 0x7F);
                    //std::cout << val << " ";
                    vals.push_back(val);
                }
                //std::cout << "(" << (checksum | 0x80) << " <> " << buffer[3 + 2 * n] << ")" << std::endl;
                if ((checksum | 0x80) != buffer[3 + 2 * n]) {
                    ofLog(OF_LOG_WARNING) << "Invalid checksum, discarding serial packet.";
                } else if (n != getNumInputDimensions()) {
                    ofLog(OF_LOG_WARNING) << "Serial packet contains " << n <<
                        " dimensions. Expected " << getNumInputDimensions();
                } else {
                    GRT::MatrixDouble data(1, n);
                    for (int i = 0; i < getNumInputDimensions(); i++) {
                        int b = vals[i];
                        data[0][i] = (normalizer_ != nullptr) ? normalizer_(b) : b;
                    }
                    if (data_ready_callback_ != nullptr) {
                        data_ready_callback_(data);
                    }
                    
                }
                
                buffer.erase(buffer.begin(), buffer.begin() + (4 + 2 * n));
            }
        }
    }
}

SerialStream::SerialStream(uint32_t port, uint32_t baud = 115200)
        : port_(port), baud_(baud), serial_(new ofSerial()) {
    // Print all devices for convenience.
    // serial_->listDevices();
}

bool SerialStream::start() {
    if (port_ == -1) {
        ofLog(OF_LOG_ERROR) << "USB Port has not been properly set";
        return false;
    }

    if (!has_started_) {
        if (!serial_->setup(port_, baud_)) return false;
        reading_thread_.reset(new std::thread(&SerialStream::readSerial, this));
        has_started_ = true;
    }

    return true;
}

void SerialStream::stop() {
    has_started_ = false;
    if (reading_thread_ != nullptr && reading_thread_->joinable()) {
        reading_thread_->join();
    }
}

int SerialStream::getNumInputDimensions() {
    return 1;
}

void SerialStream::readSerial() {
    // TODO(benzh) This readSerial is running in a different thread
    // and performing a busy polling (100% CPU usage). Should be
    // optimized.
    int sleep_time = kBufferSize_ * 1000 / (baud_ / 10);
    ofLog() << "Serial port will be read every " << sleep_time << " ms";
    while (has_started_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));

        int local_buffer_size = kBufferSize_;
        int bytesRequired = kBufferSize_;
        uint8_t *bytes = new uint8_t[bytesRequired];
        int bytesRemaining = bytesRequired;
        while (bytesRemaining > 0) {
            // check for data
            if (serial_->available() > 0) {
                // try to read - note offset into the bytes[] array, this is so
                // that we don't overwrite the bytes we already have
                int bytesArrayOffset = bytesRequired - bytesRemaining;
                int result = serial_->readBytes(&bytes[bytesArrayOffset],
                    bytesRemaining);

                // check for error code
                if (result == OF_SERIAL_ERROR) {
                    // something bad happened
                    ofLog(OF_LOG_ERROR) << "Error reading from serial";
                    break;
                }
                else if (result == OF_SERIAL_NO_DATA) {
                    // nothing was read, try again
                }
                else {
                    // we read some data!
                    bytesRemaining -= result;
                }
            }
        }
        delete[] bytes;
        GRT::MatrixDouble data(local_buffer_size, 1);
        for (int i = 0; i < local_buffer_size; i++) {
            int b = bytes[i];
            data[i][0] = (normalizer_ != nullptr) ? normalizer_(b) : b;
        }
        if (data_ready_callback_ != nullptr) {
            data_ready_callback_(data);
        }
    }
}

FirmataStream::FirmataStream(uint32_t port) : port_(port) {
    ofSerial serial;
    serial.listDevices();
}

void FirmataStream::useAnalogPin(int i) {
    pins_.push_back(i);
};

bool FirmataStream::start() {
    if (port_ == -1) {
        ofLog(OF_LOG_ERROR) << "USB Port has not been properly set";
        return false;
    }

    if (pins_.size() == 0) {
        ofLog(OF_LOG_ERROR) << "Pin has not been properly set";
        return false;
    }


    if (!has_started_) {
        ofSerial serial;
        configured_arduino_ = false;
        if (!arduino_.connect(serial.getDeviceList()[port_].getDevicePath()))
            return false;
        update_thread_.reset(new std::thread(&FirmataStream::update, this));
        has_started_ = true;
    }

    return true;
}

void FirmataStream::stop() {
    has_started_ = false;
    if (update_thread_ != nullptr && update_thread_->joinable()) {
        update_thread_->join();
    }
}

int FirmataStream::getNumInputDimensions() {
    return pins_.size();
}

void FirmataStream::update() {
    int sleep_time = 10;
    ofLog() << "Serial port will be read every " << sleep_time << " ms";
    while (has_started_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        arduino_.update();

        if (configured_arduino_) {
            vector<double> data(pins_.size());
            for (int i = 0; pins_.size(); i++)
                data[i] = arduino_.getAnalog(pins_[i]);
            data = normalize(data);
            GRT::MatrixDouble matrix;
            matrix.push_back(data);
            if (data_ready_callback_ != nullptr) data_ready_callback_(matrix);
        } else if (arduino_.isInitialized()) {
            ofLog() << "Configuring Arduino.";
            for (int i = 0; i < pins_.size(); i++)
                arduino_.sendAnalogPinReporting(pins_[i], ARD_ON);
            configured_arduino_ = true;
        }
    }
}
