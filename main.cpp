// A test program that reads JPEG frames from a webcam
// and streams them via RTP/RTCP

#include "GroupsockHelper.hh"
#include "liveMedia.hh"
#include <iostream>

#include "BasicUsageEnvironment.hh"
#include "JPEGFramedSource.hh"
#include "JPEGUnicastSubsession.h"

UsageEnvironment* env;
char*             progName;
int               fps;

void play(); // forward

void usage()
{
  std::cerr << "Usage: " << progName << " <frames-per-second>\n";
  exit(1);
}

static void announceStream(RTSPServer*         rtspServer,
                           ServerMediaSession* sms,
                           char const*         streamName,
                           char const*         inputFileName)
{
  if (rtspServer == NULL || sms == NULL)
    return; // sanity check

  UsageEnvironment& env = rtspServer->envir();

  env << "Play this stream using the URL ";
  char* url = rtspServer->rtspURL(sms);
  env << "\"" << url << "\"";
  delete[] url;

  env << "\n";
}

int main(int argc, char** argv)
{
  progName = argv[0];
  if (argc != 2)
    usage();

  if (sscanf(argv[1], "%d", &fps) != 1 || fps <= 0)
  {
    usage();
  }

  play();

  return 0;
}

void afterPlaying(void* clientData); // forward

// A structure to hold the state of the current session.
// It is used in the "afterPlaying()" function to clean up the session.
struct sessionState_t
{
  FramedSource* source;
  RTPSink*      sink;
  RTCPInstance* rtcpInstance;
  Groupsock*    rtpGroupsock;
  Groupsock*    rtcpGroupsock;
  RTSPServer*   rtspServer;
} sessionState;

void play()
{
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env                      = BasicUsageEnvironment::createNew(*scheduler);

  sessionState.source = JPEGFramedSource::createNew(*env, fps);
  if (sessionState.source == NULL)
  {
    *env << "Unable to open webcam: " << env->getResultMsg() << "\n";
    exit(1);
  }

  // Create and start a RTSP server to serve this stream:
  sessionState.rtspServer = RTSPServer::createNew(*env, 7070);
  if (sessionState.rtspServer == NULL)
  {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }

  ServerMediaSession* sms = ServerMediaSession::createNew(*env, "JPEG", progName, "JPEG Stream", False);
  sms->addSubsession(JPEGServerMediaSubsession::createNew(*env, "test.jpg"));
  sessionState.rtspServer->addServerMediaSession(sms);

  announceStream(sessionState.rtspServer, sms, "StreamName", "InputFileName");

  env->taskScheduler().doEventLoop();
}

void afterPlaying(void* /*clientData*/)
{
  *env << "...done streaming\n";

  // End by closing the media:
  Medium::close(sessionState.rtspServer);
  Medium::close(sessionState.sink);
  delete sessionState.rtpGroupsock;
  Medium::close(sessionState.source);
  Medium::close(sessionState.rtcpInstance);
  delete sessionState.rtcpGroupsock;

  // We're done:
  exit(0);
}
