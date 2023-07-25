#ifndef _WEBCAM_JPEG_DEVICE_SOURCE_HH
#define _WEBCAM_JPEG_DEVICE_SOURCE_HH

#include "JPEGVideoSource.hh"
#include "JpegFrameParser.hh"

#include <JPEGVideoRTPSink.hh>
#include <SimpleRTPSink.hh>
#include <VideoRTPSink.hh>
#include <exception>
#include <vector>

#define MAX_JPEG_FILE_SZ 200000

class DeviceException : public std::exception {
    
};

class JPEGFramedSource : public JPEGVideoSource {
public:
    static JPEGFramedSource * createNew(UsageEnvironment& env,
                                       unsigned timePerFrame);
    // "timePerFrame" is in microseconds

protected:
    JPEGFramedSource(UsageEnvironment& env,
                     int fd, unsigned timePerFrame);
    // called only by createNew()
    virtual ~JPEGFramedSource();

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

    std::vector<uint8_t> quantisation;
    unsigned precision;

private:
    int fFd;
    unsigned fTimePerFrame;
    struct timeval fLastCaptureTime;

    unsigned char *jpeg_dat;
    size_t jpeg_datlen;
};

class JPEGRTPSink : public JPEGVideoRTPSink
{
  public:
    static JPEGRTPSink* createNew(UsageEnvironment& env, Groupsock* RTPgs);

    ~JPEGRTPSink() override;

  protected:
    JPEGRTPSink(UsageEnvironment& env, Groupsock* RTPgs);

  public:
    const char* auxSDPLine() override;
    char* rtpmapLine() const override;
};

#endif // _WEBCAM_JPEG_DEVICE_SOURCE_HH
