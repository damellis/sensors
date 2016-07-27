/**
 * @file ostream.h
 * @brief OStream supports emitting prediction results to some downstream
 * consumers, e.g. Mac OS Keyboard Emulator, OSC Stream, TCP Stream, etc.
 *
 * @verbatim
 * MacOSKeyboardOStream ostream(3, '\0', 'f', 'd');
 * MacOSMouseOStream ostream(3, 0, 0, 240, 240, 400, 400);
 * TcpOStream ostream("localhost", 9999, 3, "", "mouse 300, 300.", "mouse 400, 400.");
 * TcpOStream ostream("localhost", 5204, 3, "l", "r", " ");
 * @endverbatim
 *
 */
#pragma once
#ifndef OSTREAM_H_
#define OSTREAM_H_

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ofMain.h"
#include "stream.h"

const uint64_t kGracePeriod = 500; // 0.5 second

// Forward declaration.
class ofApp;
class ofxTCPClient;

/**
 @brief Base class for output streams that forward ESP prediction results to
 other systems.

 To use an OStream instance in your application, pass it to useOutputStream()
 in your setup() function.

 Note, this is for output streams that accept the output of classifiers
 (label numbers). For output streams that can also accept the multi-dimensional
 output of signal processing pipelines, implement OStreamVector.
 */
class OStream : public virtual Stream {
  public:
    virtual void onReceive(uint32_t label) = 0;
};

/**
 @brief Base class for output streams that forward ESP pipeline output to
 other systems.

 To use an OStreamVector instance in your application, pass it to
 useOutputStream() in your setup() function.

 Note: this is for output streams that can handle both classifier output (a
 single label) and signal processing output (multi-dimensional floating point
 vectors). For output streams that only accept classifier output, implement
 OStream.
 */
class OStreamVector : public OStream {
  public:
    virtual void onReceive(vector<double>) = 0;
};

/**
 @brief Emulate keyboard key presses corresponding to prediction results.

 This class generates a key-down then key-up command for keys corresponding
 to class labels predicted by the current pipeline. The mapping from class
 labels to keys is specified in the constructor. Note that no key press
 will be generated if less than 500 ms have elapsed since the last key press.

 To use an MacOSKeyboardOStream instance in your application, pass it to
 useOutputStream() in your setup() function.
 */
class MacOSKeyboardOStream : public OStream {
  public:
    /**
     @brief Create a MacOSKeyboardOStream instance, specifying the key presses
     to emulate for each predicted class label.

     @param key_mapping: a map from predicted class labels to keys. Note that
     class 0 is the GRT's special null prediction label and is not used to
     generate key presses.
    */
    MacOSKeyboardOStream(std::map<uint32_t, char> key_mapping)
            : key_mapping_(key_mapping) {
    }

    /**
     @brief Create a MacOSKeyboardOStream instance, specifying the key presses
     to emulate for each predicted class label.

     @param count: the number of keys specified
     @param ...: the key to "press" upon prediction of the corresponding class
     label. Each key is a UTF16 character passed as an int. The first key
     specified corresponds to class label 1, the second to class label 2, etc.
    */
    MacOSKeyboardOStream(uint32_t count, ...) {
        va_list args;
        va_start(args, count);
        for (uint32_t i = 1; i <= count; i++) {
            key_mapping_[i] = va_arg(args, int);
        }
        va_end(args);
    }

    virtual void onReceive(uint32_t label) {
        if (has_started_) {
            if (getChar(label) != '\0') {
                sendKey(getChar(label));
            }
        }
    }

  private:
    void sendKey(char c);

    void sendString(const std::string& str);

    char getChar(uint32_t label) {
        return key_mapping_[label];
    }

    uint64_t elapsed_time_ = 0;
    std::map<uint32_t, char> key_mapping_;
};

/**
 @brief Emulate mouse double-clicks at locations corresponding to prediction
 results.

 This class generates a mouse double-click at locations corresponding to class
 labels predicted by the current pipeline. The mapping from class
 labels to locations is specified in the constructor. Note that no double-click
 will be generated if less than 500 ms have elapsed since the last double-click.

 To use an MacOSMouseOStream instance in your application, pass it to
 useOutputStream() in your setup() function.
 */
class MacOSMouseOStream : public OStream {
  public:
    /**
     @brief Create a MacOSMouseOStream instance, specifying the locations at
     which to double-click the mouse for each predicted class label.

     @param mouse_mapping: a map from predicted class labels to screen
     locations (x, y pairs). Note that class 0 is the GRT's special null
     prediction label and is not used to generate mouse clicks.
    */
    MacOSMouseOStream(std::map<uint32_t, pair<uint32_t, uint32_t> > mouse_mapping)
    : mouse_mapping_(mouse_mapping) {
    }

    /**
     @brief Create a MacOSMouseOStream instance, specifying the location at
     which to double-click the mouse for each predicted class label.

     @param count: the number of locations specified
     @param ...: the location at which to "double-click" upon prediction of
     the corresponding class label. Each location is specified by two uint32_t
     parameters: x then y. The first location (first two parameters) specified
     corresponds to class label 1, the second (parameters 3 and 4) to class
     label 2, etc.
    */
    MacOSMouseOStream(uint32_t count, ...) {
        va_list args;
        va_start(args, count);
        for (uint32_t i = 1; i <= count; i++) {
            mouse_mapping_[i] = make_pair(va_arg(args, uint32_t),
                                          va_arg(args, uint32_t));

        }
        va_end(args);
    }

    virtual void onReceive(uint32_t label) {
        if (has_started_) {
            pair<uint32_t, uint32_t> mouse = getMousePosition(label);
            if (mouse.first > 0 & mouse.second > 0) {
                clickMouse(mouse);
            }
        }
    }

private:
    void clickMouse(pair<uint32_t, uint32_t> mouse);
    void doubleClick(pair<uint32_t, uint32_t> mouse, int clickCount = 2);

    pair<uint32_t, uint32_t> getMousePosition(uint32_t label) {
        return mouse_mapping_[label];
    }

    uint64_t elapsed_time_ = 0;
    std::map<uint32_t, pair<uint32_t, uint32_t>> mouse_mapping_;
};

/**
 @brief Send strings over a TCP socket based on pipeline predictions.

 This class connects to a TCP server and sends it strings when predictions are
 made by the current machine learning pipeline.  The TCP connection is only
 made once, when ESP first starts, and is not restored if the other side
 disconnects.

 To use an TcpOStream instance in your application, pass it to
 useOutputStream() in your setup() function.
 */
class TcpOStream : public OStreamVector {
  public:
    /**
     Create a TCPOStream instance. This passes the predicted class labels
     directly to the TCP stream, as ASCII-formatted, newline-terminated
     strings (e.g. class label 1 will be sent as the string "1\n").

     @param server: the hostname or IP address of the TCP server to connect to
     @param port: the port of the TCP server to connect to
     */
    TcpOStream(string server, int port)
            : server_(server), port_(port),
            use_tcp_stream_mapping_(false) {}

    /**
     Create a TCPOStream instance.

     @param server: the hostname or IP address of the TCP server to connect to
     @param port: the port of the TCP server to connect to
     @tcp_stream_mapping: a map from predicted class labels to strings to send
     over the TCP connection. No delimiters or other characters are added to
     the strings specified, so, if they're required, be sure to include them
     in the provided strings. Note that 0 is a special GRT class label
     indicating no prediction, and will not trigger the sending of a string.
     */
    TcpOStream(string server, int port,
               std::map<uint32_t, string> tcp_stream_mapping)
            : server_(server), port_(port),
            use_tcp_stream_mapping_(true),
            tcp_stream_mapping_(tcp_stream_mapping) {
    }

    /**
     Create a TCPOStream instance.

     @param server: the hostname or IP address of the TCP server to connect to
     @param port: the port of the TCP server to connect to
     @param count: the number of strings provided
     @param ...: the strings to send over the TCP connection for each predicted
     class label. The first string provided corresponds to class label 1, the
     second to class label 2, etc. No delimiters or other characters are added
     to the strings specified, so, if they're required, be sure to include them
     in the provided strings.
     */
    TcpOStream(string server, int port, uint32_t count, ...)
            : server_(server), port_(port), use_tcp_stream_mapping_(true) {
        va_list args;
        va_start(args, count);
        for (uint32_t i = 1; i <= count; i++) {
            char* s = va_arg(args, char *);
            tcp_stream_mapping_[i] = std::string(s);
        }
        va_end(args);
    }

    virtual void onReceive(uint32_t label) {
        if (has_started_) {
            string to_send = getStreamString(label);
            if (!to_send.empty()) {
                sendString(to_send);
            }
        }
    }

    virtual void onReceive(vector<double> data) {
        string s;
        for (int i = 0; i < data.size(); i++) {
            if (i > 0) s += "\t";
            s += to_string(data[i]);
        }
        s += "\n";
        if (!s.empty()) sendString(s);
    }

	bool start();

private:
    void sendString(const string& tosend);

    string getStreamString(uint32_t label) {
        if (use_tcp_stream_mapping_) return tcp_stream_mapping_[label];
        else return std::to_string(label) + "\n";
    }

    string server_;
    int port_;
    ofxTCPClient *client_;

    uint64_t elapsed_time_ = 0;
    std::map<uint32_t, string> tcp_stream_mapping_;
    bool use_tcp_stream_mapping_;
};

/**
 @brief Specify an OStream to which to stream predictions made by the active
 ESP pipeline. Multiple output streams are supported.

 See also: useInputStream() to specify the input stream (from which to read
 sensor data into the ESP pipeline); and useStream() to specify a single
 stream for both input and output.

 @param stream: the OStream to use
 */
void useOutputStream(OStream &stream);
void useOutputStream(OStreamVector &stream);

#endif