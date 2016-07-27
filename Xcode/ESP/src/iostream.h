#pragma once

#include "istream.h"
#include "ostream.h"

class IOStream : public InputStream, public OStream {};
class IOStreamVector : public InputStream, public OStreamVector {};

/**
 @brief Input stream for reading ASCII data from a (USB) serial port.

 Data should be formatted as ASCII text, in newline-terminated lines. Each line
 consists of whitespace-separated numbers, e.g.
 @verbatim 123 45 678 90 @endverbatim
 The numbers in each line of text are turned into a single data sample.

 To use an ASCIISerialStream in your application, pass it to useStream() in
 your setup() function.
 */
class ASCIISerialStream : public IOStreamVector {
  public:
    /**
     Create an ASCIISerialStream instance.

     @param port: the index of the (USB) serial port to use.
     @param baud: the baud rate at which to communicate with the serial port
     @param numDimensions: the number of dimensions in the data that will come
     from the serial port (i.e. the number of numbers in each line of data).
     */
    ASCIISerialStream(uint32_t port, uint32_t baud, uint32_t numDimensions);

    /**
     Create an ASCIISerialStream instance.

     This constructor doesn't require USB port so users will be asked to select
     them at runtime.

     @param baud: the baud rate at which to communicate with the serial port
     @param numDimensions: the number of dimensions in the data that will come
     from the serial port (i.e. the number of numbers in each line of data).
     */
    ASCIISerialStream(uint32_t baud, uint32_t numDimensions);

    virtual bool start() final;
    virtual void stop() final;
    virtual int getNumInputDimensions() final;
  
    virtual void onReceive(uint32_t label);
    virtual void onReceive(vector<double> data);

    vector<string> getSerialDeviceList() {
        serial_->listDevices();
        vector<string> retval;
        vector<ofSerialDeviceInfo> device_list = serial_->getDeviceList();
        retval.reserve(device_list.size());
        for (auto& d : device_list) {
            retval.push_back(d.getDevicePath());
        }
        return retval;
    }

    bool selectSerialDevice(uint32_t port) {
        assert(has_started_ == false
               && "Should only reach here if ASCIISerialStream hasn't started");

        port_ = port;
        if (!serial_->setup(port_, baud_)) {
            return false;
        }

        reading_thread_.reset(new std::thread(&ASCIISerialStream::readSerial, this));
        has_started_ = true;
        return true;
    }

  private:
    unique_ptr<ofSerial> serial_;
    uint32_t port_;
    uint32_t baud_;
    uint32_t numDimensions_;

    // A separate reading thread to read data from Serial.
    unique_ptr<std::thread> reading_thread_;
    void readSerial();
};

/**
 Tells the ESP system which stream to use, for both input and output. Call
 from your setup() function. The specified stream will be automatically
 started by the ESP system. Note that only one input stream is supported
 at a time; subsequent calls to useStream() will cause the new stream to be
 used for input, replacing input from streams passed to any previous calls to
 useStream() or useInputStream(). Multiple simultaneous output streams are
 supported, however, so calling useStream() will cause output to be sent to
 the specified stream in addition to streams previously specified with
 useStream() or useOutputStream().

 @param stream: the stream to use for input and output
 */
void useStream(IOStream &stream);
void useStream(IOStreamVector &stream);