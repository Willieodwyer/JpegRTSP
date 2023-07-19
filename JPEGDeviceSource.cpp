#include "JPEGDeviceSource.hh"
#include <sys/time.h>
#include <sys/mman.h>

#include <algorithm>

JPEGDeviceSource*
JPEGDeviceSource::createNew(UsageEnvironment& env,
                            unsigned timePerFrame) {
    int fd = -1;
    try {
        return new JPEGDeviceSource(env, fd, timePerFrame);
    } catch (DeviceException) {
        return NULL;
    }
}

JPEGDeviceSource
::JPEGDeviceSource(UsageEnvironment& env, int fd, unsigned timePerFrame)
  : JPEGVideoSource(env), fFd(fd), fTimePerFrame(timePerFrame)
{
    jpeg_dat = new unsigned char [MAX_JPEG_FILE_SZ];
    FILE *fp = fopen("test.jpg", "rb");
    if(fp==NULL) {
        env.setResultErrMsg("could not open test.jpg.\n");
        throw DeviceException();
    }
    jpeg_datlen = fread(jpeg_dat, 1, MAX_JPEG_FILE_SZ, fp);
    fclose(fp);
}

JPEGDeviceSource::~JPEGDeviceSource()
{
    delete [] jpeg_dat;
}

static int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
    if(x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if(x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;
    return x->tv_sec < y->tv_sec;
}

static float timeval_diff(struct timeval *x, struct timeval *y)
{
    struct timeval result;
    timeval_subtract(&result, x, y);
    return result.tv_sec + result.tv_usec/1000000.0;
}

static struct timezone Idunno;

void JPEGDeviceSource::doGetNextFrame()
{
    static unsigned long framecount = 0;
    static struct timeval starttime;
    
    fFrameSize = jpeg_to_rtp(fTo, jpeg_dat, jpeg_datlen);
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

static unsigned char calcQ(unsigned char const *qt);

static unsigned char calcQ(unsigned char const *qt)
{
    unsigned int q;
    q = (qt[0]*100-50)/16;
    //q = (qt[64]*100-50)/17;
    if(q>5000)
        q = 5000;
    if(q<2)
        q = 2;
    if(q>100)
        q = 5000/q;
    else
        q = (200-q)/2;
    return (unsigned char) q;
}

size_t JPEGDeviceSource::jpeg_to_rtp(void *pto, void *pfrom, size_t len)
{
    unsigned char *to=(unsigned char*)pto, *from=(unsigned char*)pfrom;
    unsigned int datlen;
    unsigned char const * dat;
    if(parser.parse(from, len) == 0) { // successful parsing
        dat = parser.scandata(datlen);
        memcpy(to, dat, datlen);
        to += datlen;
        return datlen;
    }
    return 0;
}

u_int8_t const * JPEGDeviceSource::quantizationTables(u_int8_t & precision, u_int16_t & length)
{
    precision = parser.precision();
    return parser.quantizationTables(length);
}

u_int8_t JPEGDeviceSource::type()
{
    return parser.type();
}

u_int8_t JPEGDeviceSource::qFactor()
{
    return 128;
}

u_int8_t JPEGDeviceSource::width()
{
    return parser.width();
}

u_int8_t JPEGDeviceSource::height()
{
    return parser.height();
}
