#pragma once

#include "JPEGParser.h"
#include "JPEGVideoSource.hh"

#include <JPEGVideoRTPSink.hh>
#include <SimpleRTPSink.hh>
#include <VideoRTPSink.hh>
#include <exception>
#include <vector>

#define MAX_JPEG_FILE_SZ 200000

class DeviceException : public std::exception
{};

class JPEGFramedSource : public JPEGVideoSource
{
public:
  static JPEGFramedSource* createNew(UsageEnvironment& env, unsigned timePerFrame);

protected:
  explicit JPEGFramedSource(UsageEnvironment& env, unsigned int framerate);
  // called only by createNew()
  virtual ~JPEGFramedSource();

private:
  // redefined virtual functions:
  virtual void            doGetNextFrame() override;
  virtual u_int8_t        type() override;
  virtual u_int8_t        qFactor() override;
  virtual u_int8_t        width() override;
  virtual u_int8_t        height() override;
  virtual u_int8_t const* quantizationTables(u_int8_t& precision, u_int16_t& length) override;

private:
  JpegParser::RtpJPEGPayload m_payload;

  std::vector<uint8_t> m_quantisation;
  unsigned             m_precision;

private:
  unsigned char* jpeg_dat;
  size_t         jpeg_datlen;
  uint64_t       m_last_pts = 0;
  unsigned int   m_framerate;
};

class JPEGRTPSink : public JPEGVideoRTPSink
{
public:
  static JPEGRTPSink* createNew(UsageEnvironment& env, Groupsock* RTPgs);

  ~JPEGRTPSink() override;

protected:
  JPEGRTPSink(UsageEnvironment& env, Groupsock* RTPgs);

};
