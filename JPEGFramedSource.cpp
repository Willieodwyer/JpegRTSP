#include "JPEGFramedSource.hh"
#include <sys/mman.h>
#include <sys/time.h>

#include <algorithm>
#include <chrono>
#include <string>

#include "GstJpegParser.h"


#define IMAGE "image.jpg"

JPEGFramedSource *
JPEGFramedSource::createNew(UsageEnvironment& env,
                            unsigned timePerFrame) {
    int fd = -1;
    try {
        return new JPEGFramedSource(env, fd, timePerFrame);
    } catch (DeviceException) {
        return NULL;
    }
}

JPEGFramedSource ::JPEGFramedSource(UsageEnvironment& env, int fd, unsigned timePerFrame)
  : JPEGVideoSource(env), fFd(fd), fTimePerFrame(timePerFrame)
{
    jpeg_dat = new unsigned char [MAX_JPEG_FILE_SZ];
    FILE *fp = fopen(IMAGE, "rb");
    if(fp==NULL) {
        env.setResultErrMsg("could not open " IMAGE "\n");
        throw DeviceException();
    }
    else
    {
        printf("Successfully opened: " IMAGE "\n");
    }
    jpeg_datlen = fread(jpeg_dat, 1, MAX_JPEG_FILE_SZ, fp);
    fclose(fp);
}

JPEGFramedSource::~JPEGFramedSource()
{
    delete [] jpeg_dat;
}

static struct timezone Idunno;

void JPEGFramedSource::doGetNextFrame()
{
    static unsigned long framecount = 0;
    static struct timeval starttime;

    auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

    fTo =         gst_rtp_jpeg_pay_handle_buffer(jpeg_dat, jpeg_datlen, ms.count(), quantisation, precision);
    
    //fFrameSize = jpeg_to_rtp(fTo, jpeg_dat, jpeg_datlen);
    gettimeofday(&fLastCaptureTime, &Idunno);
    if(framecount==0)
        starttime = fLastCaptureTime;
    framecount++;
    fPresentationTime = fLastCaptureTime;
    fDurationInMicroseconds = fTimePerFrame;

    // Switch to another task, and inform the reader that he has data:
    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
                    (TaskFunc*)FramedSource::afterGetting, this);
}

const u_int8_t* JPEGFramedSource::quantizationTables(u_int8_t& precision, u_int16_t& length)
{
    length    = quantisation.size();
    precision = precision;
    return quantisation.data();
}

u_int8_t JPEGFramedSource::type()
{
    return 1;
}

u_int8_t JPEGFramedSource::qFactor()
{
    return 255;
}

u_int8_t JPEGFramedSource::width()
{
    return 67;
}

u_int8_t JPEGFramedSource::height()
{
    return 50;
}

// JPEGRTPSink

JPEGRTPSink* JPEGRTPSink::createNew(UsageEnvironment& env, Groupsock* RTPgs)
{
    return new JPEGRTPSink(env, RTPgs);
}

JPEGRTPSink::~JPEGRTPSink()
{
    printf("~JPEGRTPSink()\n");
};

JPEGRTPSink::JPEGRTPSink(UsageEnvironment& env, Groupsock* RTPgs) : JPEGVideoRTPSink(env, RTPgs) {}

char* JPEGRTPSink::rtpmapLine() const
{
    //return RTPSink::rtpmapLine();
    if (rtpPayloadType() == 26) {
        char* encodingParamsPart;
        if (numChannels() != 1) {
          encodingParamsPart = new char[1 + 20 /* max int len */];
          sprintf(encodingParamsPart, "/%d", numChannels());
        } else {
          encodingParamsPart = strDup("");
        }
        char const* const rtpmapFmt = "a=rtpmap:%d %s/%d%s\r\n";
        unsigned rtpmapLineSize = strlen(rtpmapFmt)
                                  + 3 /* max char len */ + strlen(rtpPayloadFormatName())
                                  + 20 /* max int len */ + strlen(encodingParamsPart);
        char* rtpmapLine = new char[rtpmapLineSize];
        sprintf(rtpmapLine, rtpmapFmt,
                rtpPayloadType(), rtpPayloadFormatName(),
                rtpTimestampFrequency(), encodingParamsPart);
        delete[] encodingParamsPart;

        return rtpmapLine;
    } else {
        // The payload format is static, so there's no "a=rtpmap:" line:
        return strDup("");
    }
}

const char* JPEGRTPSink::auxSDPLine()
{
    std::string aux_line = "a=framesize:26 534-400\r\n" // TODO
                           "a=framerate:25.000000\r\n";

    char* rtpmapLine = new char[aux_line.length() + 1];
    strncpy(rtpmapLine, aux_line.c_str(), aux_line.size() + 1);
    return rtpmapLine;
}
