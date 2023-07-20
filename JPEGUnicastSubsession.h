#pragma once

#include <FileServerMediaSubsession.hh>

class JPEGServerMediaSubsession : public FileServerMediaSubsession
{
public:
  static JPEGServerMediaSubsession* createNew(UsageEnvironment& env, char const* fileName);

private:
  JPEGServerMediaSubsession(UsageEnvironment& env, const char* fileName);


private: // redefined virtual functions
  virtual FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate);
  virtual RTPSink*      createNewRTPSink(Groupsock*    rtpGroupsock,
                                         unsigned char rtpPayloadTypeIfDynamic,
                                         FramedSource* inputSource);
};
