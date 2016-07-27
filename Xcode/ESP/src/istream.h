/**
 @file InputStream.h
 @brief Definition of input streams: serial, audio, etc.

 These input streams provide sensor data to the machine learning pipeline.
 Each stream provides a sequence data samples, each of which is a
 multi-dimensional vector representing a single reading from a sensor (or
 group of sensors).
 */
#pragma once

#include "GRT/GRT.h"
#include "ofMain.h"
#include "stream.h"

#include <cstdint>

// See more documentation:
// http://openframeworks.cc/documentation/sound/ofSoundStream/#show_setup
const uint32_t kOfSoundStream_SamplingRate = 44100;
const uint32_t kOfSoundStream_BufferSize = 256;
const uint32_t kOfSoundStream_nBuffers = 4;

/**
 @brief Base class for input streams that provide live sensor data to the ESP
 system.

 To use an InputStream instance in your application, pass it to useInputStream()
 in your setup() function.
 */
class InputStream : public virtual Stream {
  public:
    InputStream();
    virtual ~InputStream() = default;

    /**
     Get the number of dimensions of the data that's provided by the
     input stream (before it's run through the normalizer, if any).
     */
    virtual int getNumInputDimensions() = 0;
    virtual int getNumOutputDimensions() {
        vector<double> input(getNumInputDimensions(), 1.0);
        vector<double> output = normalize(input);
        return output.size();
    }

    using normalizeFunc = std::function<double(double)>;
    using vectorNormalizeFunc = std::function<vector<double>(vector<double>)>;

    // Supply a normalization function: double -> double.
    // Applied to each dimension of each vector of incoming data.
    void useNormalizer(normalizeFunc f) {
        normalizer_ = f;
        vectorNormalizer_ = nullptr;
    }

    // Supply a normalization function: vector<double> -> vector<double>
    // Applied to each vector of incoming data.
    void useNormalizer(vectorNormalizeFunc f) {
        normalizer_ = nullptr;
        vectorNormalizer_ = f;
    }

    typedef std::function<void(GRT::MatrixDouble)> onDataReadyCallback;

    void onDataReadyEvent(onDataReadyCallback callback) {
        data_ready_callback_ = callback;
    }

    template<typename T1, typename arg, class T>
    void onDataReadyEvent(T1* owner, void (T::*listenerMethod)(arg)) {
        using namespace std::placeholders;
        data_ready_callback_ = std::bind(listenerMethod, owner, _1);
    }

    // Set labels on all input dimension. This function takes either a vector of
    // strings, or an initialization list (such as {"left", "right"}).
    void setLabelsForAllDimensions(const vector<string> labels);
    void setLabelsForAllDimensions(std::initializer_list<string> list);

    const vector<string>& getLabels() const;

  protected:
    vector<string> InputStream_labels_;
    onDataReadyCallback data_ready_callback_;
    normalizeFunc normalizer_;
    vectorNormalizeFunc vectorNormalizer_;

    vector<double> normalize(vector<double>);
};

/**
 @brief Input stream for reading audio from the computer's microphone.
 */
class AudioStream : public ofBaseApp, public InputStream {
  public:
    AudioStream(uint32_t downsample_rate = 1);
    void audioIn(float *input, int buffer_size, int nChannel);
    virtual bool start() final;
    virtual void stop() final;
    virtual int getNumInputDimensions() final;
  private:
    uint32_t downsample_rate_;
    unique_ptr<ofSoundStream> sound_stream_;
    bool setup_successful_;
};

/**
 @brief Input stream for getting the FFT spectrum of an audio file as it plays.
 
 This class plays an audio file and supplies its 1024-sample / 512-bin FFT
 spectrum as input to the current pipeline. The spectrum is supplied every
 ~23 milliseconds (i.e. 441000 KHz / 1024 = ~43 Hz).
 */
class AudioFileStream : public InputStream {
  public:
    AudioFileStream(char *file, bool loop = false);
    virtual bool start() final;
    virtual void stop() final;
    virtual int getNumInputDimensions() final;
  private:
    void readSpectrum();
  
    ofSoundPlayer player_;
    unique_ptr<std::thread> update_thread_;
};

class BaseSerialStream : public InputStream {
  public:
    BaseSerialStream(uint32_t usb_port_num, uint32_t baud, int numDimensions);
    virtual bool start() final;
    virtual void stop() final;
    virtual int getNumInputDimensions() final;
  protected:
    virtual void parseSerial(vector<unsigned char> &buffer) = 0;
  private:
    uint32_t port_ = -1;
    uint32_t baud_;
    int dimensions_;

    vector<unsigned char> buffer_;

    unique_ptr<ofSerial> serial_;

    // A separate reading thread to read data from Serial.
    unique_ptr<std::thread> reading_thread_;
    void readSerial();
};

class SerialStream : public InputStream {
  public:
    SerialStream(uint32_t usb_port_num, uint32_t baud);
    virtual bool start() final;
    virtual void stop() final;
    virtual int getNumInputDimensions() final;
  private:
    uint32_t port_ = -1;
    uint32_t baud_;
    // Serial buffer size
    uint32_t kBufferSize_ = 64;

    unique_ptr<ofSerial> serial_;

    // A separate reading thread to read data from Serial.
    unique_ptr<std::thread> reading_thread_;
    void readSerial();
};

class BinaryIntArraySerialStream : public BaseSerialStream {
  public:
    using BaseSerialStream::BaseSerialStream; // inherit constructors
  private:
    virtual void parseSerial(vector<unsigned char> &buffer);
};

/**
 @brief Input stream for reading analog data from an Arduino running Firmata.

 To use an FirmataStream in your application, pass it to useInputStream() in
 your setup() function.
 */
class FirmataStream : public InputStream {
  public:
    /**
     Create a FirmataStream instance. Assumes the Arduino is communicating at
     57600 baud.

     @param port: the index of the (USB) serial port to use.
     */
    FirmataStream(uint32_t port);
    virtual bool start() final;
    virtual void stop() final;
    virtual int getNumInputDimensions() final;

    /**
     Include readings from the specified analog pin in the data reported by
     this FirmataStream. Data will be ordered according to the sequence of
     calls to this function (i.e. readings from the pin passed to the first
     call to useAnalogPin() will appear first in the data provided by the
     FirmataStream).

     @param i: an analog pin to read from
     */
    void useAnalogPin(int i);
  private:
    uint32_t port_;

    vector<int> pins_;

    bool configured_arduino_;

    ofArduino arduino_;
    unique_ptr<std::thread> update_thread_;
    void update();
};

/**
 Tells the ESP system from which input stream to read sensor data for
 processing by the active machine learning pipeline. Call from your setup()
 function. The specified stream will be automatically started by the ESP
 system. Note that only one input stream is supported; subsequent calls to
 useInputStream() will replace the previously-specified stream.

 See also: useOutputStream() for specifying an output stream (to which to
 stream the predictions made by the ESP pipeline) and useStream() to specify
 a stream to use for both input and output.

 @param stream: the input stream to use. May be an IOStream instance, in which
 case the stream will only be used for input.
 */
void useInputStream(InputStream &stream);

/**
 Tells the ESP system which machine learning pipeline to use. Call from your
 setup() function. Note that only one pipeline is supported; subsequent calls
 to usePipeline() will replace the previously-specified pipeline.

 The pipeline will be fed with data from the input stream specified using
 useStream() or useInputStream(), as modified by the calibrators specified
 using useCalibrator().
 */
void usePipeline(GRT::GestureRecognitionPipeline &pipeline);
