#ifndef _WEBCAM_JPEG_DEVICE_SOURCE_HH
#define _WEBCAM_JPEG_DEVICE_SOURCE_HH

#include "JPEGVideoSource.hh"
#include "JpegFrameParser.hh"

#include <exception>

#define MAX_JPEG_FILE_SZ 200000

class DeviceException : public std::exception {
    
};

class JPEGDeviceSource: public JPEGVideoSource {
public:
    static JPEGDeviceSource* createNew(UsageEnvironment& env,
                                       unsigned timePerFrame);
    // "timePerFrame" is in microseconds

protected:
    JPEGDeviceSource(UsageEnvironment& env,
                     int fd, unsigned timePerFrame);
    // called only by createNew()
    virtual ~JPEGDeviceSource();

private:
    // redefined virtual functions:
    virtual void doGetNextFrame();
    virtual u_int8_t type();
    virtual u_int8_t qFactor();
    virtual u_int8_t width();
    virtual u_int8_t height();
    virtual u_int8_t const * quantizationTables(u_int8_t & precision, u_int16_t & length);

private:
    struct buffer {
        void   *start;
        size_t  length;
    };

    size_t jpeg_to_rtp(void *to, void *from, size_t len);
    
private:
    int fFd;
    unsigned fTimePerFrame;
    struct timeval fLastCaptureTime;

    JpegFrameParser parser;
    
    unsigned char *jpeg_dat;
    size_t jpeg_datlen;

};

#endif // _WEBCAM_JPEG_DEVICE_SOURCE_HH
