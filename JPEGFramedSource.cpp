#include "JPEGFramedSource.hh"
#include <sys/mman.h>
#include <sys/time.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

#include "JPEGParser.h"

#define IMAGE "test.jpg"

JPEGFramedSource* JPEGFramedSource::createNew(UsageEnvironment& env, unsigned framerate)
{
  try
  {
    return new JPEGFramedSource(env, framerate);
  }
  catch (...)
  {
    return nullptr;
  }
}

JPEGFramedSource ::JPEGFramedSource(UsageEnvironment& env, unsigned int framerate)
    : JPEGVideoSource(env), m_framerate(framerate)
{
  jpeg_dat = new unsigned char[MAX_JPEG_FILE_SZ];
  FILE* fp = fopen(IMAGE, "rb");
  if (fp == nullptr)
  {
    env.setResultErrMsg("could not open " IMAGE "\n");
    throw DeviceException();
  }
  else
  {
    printf("Successfully opened: " IMAGE "\n");
  }
  jpeg_datlen = fread(jpeg_dat, 1, MAX_JPEG_FILE_SZ, fp);
  fclose(fp);

  // We need to parse first, to ensure all quants are correct
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
  m_quantisation.clear();
  m_payload = JpegParser::handle_buffer(jpeg_dat, jpeg_datlen, ms.count(), m_quantisation, m_precision);
}

JPEGFramedSource::~JPEGFramedSource()
{
  delete[] jpeg_dat;
}

static struct timezone Idunno;

void JPEGFramedSource::doGetNextFrame()
{
  std::this_thread::sleep_for(std::chrono::milliseconds(1000 / m_framerate));

  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

  m_quantisation.clear();
  m_payload = JpegParser::handle_buffer(jpeg_dat, jpeg_datlen, ms.count(), m_quantisation, m_precision);

  if (m_payload.size <= fMaxSize)
  {
    fNumTruncatedBytes = 0;
    fFrameSize         = m_payload.size;

    memcpy(fTo, m_payload.payload, m_payload.size);
    // fTo = m_payload.payload;

    uint64_t ts = m_payload.timestamp;
    if (m_last_pts == 0)
      m_last_pts = ts;

    fPresentationTime.tv_sec = (long)ts / 1000;
    ts -= fPresentationTime.tv_sec * 1000;
    fPresentationTime.tv_usec = (long)ts * 1000;
    fDurationInMicroseconds   = (unsigned int)(m_payload.timestamp - m_last_pts) * 1000;

    m_last_pts = m_payload.timestamp;
  }
  else
  {
    fprintf(stderr, "fMaxSize is too small!");
    fFrameSize = 0;
  }

  // Switch to another task, and inform the reader that he has data:
  nextTask() = envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)FramedSource::afterGetting, this);
}

const u_int8_t* JPEGFramedSource::quantizationTables(u_int8_t& precision, u_int16_t& length)
{
  length    = m_quantisation.size();
  precision = m_precision;
  return m_quantisation.data();
}

u_int8_t JPEGFramedSource::type()
{
  return m_payload.type;
}

u_int8_t JPEGFramedSource::qFactor()
{
  return m_payload.quality;
}

u_int8_t JPEGFramedSource::width()
{
  return m_payload.width;
}

u_int8_t JPEGFramedSource::height()
{
  return m_payload.height;
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
  // return RTPSink::rtpmapLine();
  if (rtpPayloadType() == 26)
  {
    char* encodingParamsPart;
    if (numChannels() != 1)
    {
      encodingParamsPart = new char[1 + 20 /* max int len */];
      sprintf(encodingParamsPart, "/%d", numChannels());
    }
    else
    {
      encodingParamsPart = strDup("");
    }
    char const* const rtpmapFmt      = "a=rtpmap:%d %s/%d%s\r\n";
    unsigned          rtpmapLineSize = strlen(rtpmapFmt) + 3 /* max char len */ + strlen(rtpPayloadFormatName()) +
                              20 /* max int len */ + strlen(encodingParamsPart);
    char* rtpmapLine = new char[rtpmapLineSize];
    sprintf(
        rtpmapLine, rtpmapFmt, rtpPayloadType(), rtpPayloadFormatName(), rtpTimestampFrequency(), encodingParamsPart);
    delete[] encodingParamsPart;

    return rtpmapLine;
  }
  else
  {
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
