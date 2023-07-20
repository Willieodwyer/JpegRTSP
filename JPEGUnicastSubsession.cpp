//
// Created by will on 7/20/23.
//

#include "JPEGUnicastSubsession.h"
#include "JPEGFramedSource.hh"
JPEGServerMediaSubsession* JPEGServerMediaSubsession::createNew(UsageEnvironment& env, const char* fileName)
{
  try
  {
    return new JPEGServerMediaSubsession(env, fileName);
  }
  catch (...)
  {}
  return nullptr;
}

JPEGServerMediaSubsession::JPEGServerMediaSubsession(UsageEnvironment& env, const char* fileName)
    : FileServerMediaSubsession(env, fileName, False)
{}

FramedSource* JPEGServerMediaSubsession::createNewStreamSource(unsigned int clientSessionId, unsigned int& estBitrate)
{
  return JPEGFramedSource::createNew(envir(), estBitrate);
}

RTPSink* JPEGServerMediaSubsession::createNewRTPSink(Groupsock*    rtpGroupsock,
                                                     unsigned char rtpPayloadTypeIfDynamic,
                                                     FramedSource* inputSource)
{
  return JPEGRTPSink::createNew(envir(), rtpGroupsock);
}
