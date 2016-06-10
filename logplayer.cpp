/**
 * Implements limited RTSP server(@see https://en.wikipedia.org/wiki/Real_Time_Streaming_Protocol) trying to
 * simulate Panasonic wv-sp105 IP camera behavior.
 * It waits for a 'PLAY' request from a client and starts to playback the given RTP/CAN messages log.
 * Can and networks configuration items(like can device name, network port/address to bind, etc)
 * are read from the cmd line arguments.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dumpplayer.h"

void usage(const char *name)
{
   printf("Usage: %s [-v] [-r] [-d can_device_path] [-t can_frame_type] "
           "[-p bind_port] [-i bind_addr] rtplog_file.bin canlog_file.log\n"
           "  -v increase logging verbosity level\n"
           "  -r rewind log file once end of file is reached\n"
           "  -d can device name to send a CAN message(default: can0)\n"
           "  -t std/ext - Standart/Extended CAN Frame (default: std)\n"
           "  -p port to listen for RTPS connection (default: 554)\n"
           "  -i ip address to bind (default: INADDR_ANY)\n"
           , name);

   printf("Like: %s -r -i 127.0.01 -p 8554 camera.bin CAN.log\n", name);
   exit(EXIT_FAILURE);
}

/**
 * Parses the input RTSP request and extracts the command sequence number (CSeq:) value.
 *
 * @return command sequence number (CSeq:) value or -1 on failure
 */
int getRequestSequenceNumber(const char *data);

/**
 * Parses the input RTSP SETUP request and extracts the client_port value for RTP
 * transmitting.
 *
 * @return 0 on success( port1, port2 is filled with extracted data) or -1 on failure.
 */
int getRequestClientPort(const char *data, uint32_t *port1, uint32_t *port2);

struct playerCfg
{
   const char *bindAddr;
   const char *RTPlogFile;
   const char *CANlogFile;
   int bindPort;
   const char* canDeviceName;
   canFrameType canType;
   int verbosity;
   int rewindLog;
};

struct rtcpSession
{
   uint32_t  sessionID;
   uint32_t  ssrc;
   const char *clientIp;
   uint32_t port1;
   uint32_t port2;

   dumpPlayer player;
};

static rtcpSession session;
static playerCfg   configOptions;

int optionCmdHandler (const char *data, const int size, int fd)
{
   int sequenceNumber = getRequestSequenceNumber(data);
   if(sequenceNumber < 0)
   {
      return EIO;
   }

   char response[512];
   int resSize = snprintf(response, sizeof(response),
         "RTSP/1.0 200 OK\r\n"
         "CSeq: %i\r\n"
         "Connection: Keep-Alive\r\n"
         "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN, SET_PARAMETER\r\n"
         "\r\n"
         ,sequenceNumber
      );

   resSize = write(fd, response, strlen(response));
   if(-1 == resSize)
   {
      fprintf(stderr, "write() failed(%s)\n", strerror(errno));
      return errno;
   }

   return 0;
}

int getParamCmdHandler (const char *data, const int size, int fd)
{
   int sequenceNumber = getRequestSequenceNumber(data);
   if(sequenceNumber < 0)
   {
      return EIO;
   }

   char response[512];
   int resSize = snprintf(response, sizeof(response),
         "RTSP/1.0 200 OK\r\n"
         "CSeq: %i\r\n"
         "Connection: Keep-Alive\r\n"
         "Content-Length: 0\r\n"
         "\r\n"
         ,sequenceNumber
      );

   resSize = write(fd, response, strlen(response));
   if(-1 == resSize)
   {
      fprintf(stderr, "write() failed(%s)\n", strerror(errno));
      return errno;
   }

   return 0;
}

int describeCmdHandler (const char *data, const int size, int fd)
{
   int sequenceNumber = getRequestSequenceNumber(data);
   if(sequenceNumber < 0)
   {
      return EIO;
   }

   const char* SDPMsg =
         "v=0\r\n"
         "o=- 1 1 IN IP4 %s\r\n"
         "c=IN IP4 0.0.0.0\r\n"
         "b=AS:9216\r\n"
         "t=0 0\r\n"
         "a=control:*\r\n"
         "a=range:npt=now-\r\n"
         "m=video 0 RTP/AVP 98\r\n"
         "b=AS:9216\r\n"
         "a=framerate:30.0\r\n"
         "a=control:trackID=1\r\n"
         "a=rtpmap:98 H264/90000\r\n"
         "a=fmtp:98 packetization-mode=1; profile-level-id=640028; sprop-parameter-sets=Z2QAKK3FTYY4jFRWKmwxxGKisVNhjiMVFRBIjEc2SSIJEYjmySRBIjEc2SQtAKAPP+A1SAAAXdgACvyHsQPoAAYahf//HYgfQAAw1C//+FA=,aM44MA==\r\n"
         "a=h264-esid:201\r\n"
         "\r\n"
         ;
   char sdpMsg[256];
   snprintf(sdpMsg, sizeof(sdpMsg), SDPMsg, configOptions.bindAddr);

   char response[1024];
   int resSize = snprintf(response, sizeof(response),
         "RTSP/1.0 200 OK\r\n"
         "CSeq: %i\r\n"
         "Content-Base: rtsp://%s\r\n"
         "Content-type: application/sdp\r\n"
         "Content-length: %lu\r\n"
         "\r\n"
         "%s"
         , sequenceNumber
         , configOptions.bindAddr
         , strlen(sdpMsg)
         , sdpMsg
      );

   resSize = write(fd, response, strlen(response));
   if(-1 == resSize)
   {
      fprintf(stderr, "write() failed(%s)\n", strerror(errno));
      return errno;
   }

   return 0;
}

int playCmdHandler (const char *data, const int size, int fd)
{
   int sequenceNumber = getRequestSequenceNumber(data);
   if(sequenceNumber < 0)
   {
      return EIO;
   }

   char response[512];
   int resSize = snprintf(response, sizeof(response),
         "RTSP/1.0 200 OK\r\n"
         "CSeq: %i\r\n"
         "Session: %u\r\n"
         "RTP-Info: url=trackID=1;seq=57746;rtptime=1212438488\r\n"
         "\r\n"
         ,sequenceNumber
         ,session.sessionID
      );

   resSize = write(fd, response, strlen(response));
   if(-1 == resSize)
   {
      fprintf(stderr, "write() failed(%s)\n", strerror(errno));
      return errno;
   }

   dumpPlayerCfg playerCfg = {configOptions.RTPlogFile, configOptions.CANlogFile, session.clientIp, (int)session.port1, session.ssrc
         , configOptions.canDeviceName, configOptions.canType, configOptions.rewindLog
   };

   int err = dumpPlayerInit(&session.player, &playerCfg);
   if(0 != err)
   {
      fprintf(stderr, "dumpPlayerInit() failed(%s)\n", strerror(err));
      return err;
   }
   err = dumpPlayerStart(&session.player);

   return err;
}

int pauseCmdHandler (const char *data, const int size, int fd)
{
   int sequenceNumber = getRequestSequenceNumber(data);
   if(sequenceNumber < 0)
   {
      return EIO;
   }

   char response[512];
   int resSize = snprintf(response, sizeof(response),
         "RTSP/1.0 200 OK\r\n"
         "CSeq: %i\r\n"
         "Session: %u\r\n"
         "\r\n"
         ,sequenceNumber
         ,session.sessionID
      );

   resSize = write(fd, response, strlen(response));
   if(-1 == resSize)
   {
      fprintf(stderr, "write() failed(%s)\n", strerror(errno));
      return errno;
   }
   return 0;
}


int setupCmdHandler (const char *data, const int size, int fd)
{
   int sequenceNumber = getRequestSequenceNumber(data);
   if(sequenceNumber < 0)
   {
      return EIO;
   }

   int err = getRequestClientPort(data, &session.port1, &session.port2);
   if(0 != err)
   {
      return EIO;
   }

   session.ssrc = rand();
   session.sessionID = rand();

   char response[512];
   int resSize = snprintf(response, sizeof(response),
         "RTSP/1.0 200 OK\r\n"
         "CSeq: %i\r\n"
         "Session: %u;timeout=120\r\n"
         "Transport: RTP/AVP/UDP;unicast;client_port=%u-%u;server_port=%u-%u;ssrc=%x\r\n"
         "\r\n"
         ,sequenceNumber
         ,session.sessionID
         ,session.port1, session.port2
         ,session.port1, session.port2
         ,session.ssrc
      );

   resSize = write(fd, response, strlen(response));
   if(-1 == resSize)
   {
      fprintf(stderr, "write() failed(%s)\n", strerror(errno));
      return errno;
   }
   return 0;
}


/**
 * List of supported RTSP commands and assigned command handlers.
 */
typedef int (*cmdHandler) (const char *data, const int size, int fd);
const char *cmds[] = {"OPTIONS", "DESCRIBE", "SETUP", "PLAY", "PAUSE", "GET_PARAMETER", "TEARDOWN", "SET_PARAMETER"};
cmdHandler handlers[] = { optionCmdHandler, describeCmdHandler, setupCmdHandler, playCmdHandler
                        , pauseCmdHandler, getParamCmdHandler, optionCmdHandler, optionCmdHandler
                        };

/**
 * Go throw the list of supported command, compare to the received one and exec the
 * assigned handler function.
 */
int processRequest(const char *data, const int size, int fd)
{
   int i;
   for(i=0; i<int(sizeof(cmds)/sizeof(cmds[0])); i++)
   {
      if(size < (int)strlen(cmds[i])) continue;

      if(0 == strncmp(data, cmds[i], strlen(cmds[i])))
      {
         if(configOptions.verbosity)
         {
            printf("%s request received\n", cmds[i]);
            printf("%s", data);
         }
         return handlers[i](data, size, fd);
      }
   }

   return 0;
}

int main(int argc, char **argv)
{
   configOptions.bindAddr = NULL;
   configOptions.bindPort = 554;
   configOptions.canDeviceName = "can0";
   configOptions.canType = CAN_FRAME_STD_TYPE;
   configOptions.verbosity = 0;
   configOptions.rewindLog = 0;

   configOptions.RTPlogFile = NULL;
   configOptions.CANlogFile = NULL;

   //if set - don't start rtps server, but just streaming on the given address and port
   bool forcePlayback = false;

   if (argc < 2)
   {
      usage(argv[0]);
      return EXIT_FAILURE;
   }

   int opt;
   while ((opt = getopt(argc, argv, "vrd:b:t:p:i:f")) != -1)
   {
       switch (opt)
       {
          case 'f':
             forcePlayback = true;
          break;
          case 'v':
             configOptions.verbosity++;
          break;
          case 'r':
             configOptions.rewindLog = 1;
          break;
          case 'd':
             configOptions.canDeviceName = optarg;
          break;
          case 't':
          {
             if(0 == strncmp(optarg, "ext", 3));
               configOptions.canType = CAN_FRAME_EXT_TYPE;
          }
          break;
          case 'p':
             configOptions.bindPort = atoi(optarg);
          break;
          case 'i':
             configOptions.bindAddr = optarg;
          break;

          default:
             usage(argv[0]);
             return EXIT_FAILURE;
       }
   }

   if (optind+2 != argc)
   {
      usage(argv[0]);
      return EXIT_FAILURE;
   }
   configOptions.RTPlogFile = argv[optind];
   configOptions.CANlogFile = argv[optind+1];

   //it is debug feature for player performance testing with logdump/logcmp utilities
   if(forcePlayback)
   {
      dumpPlayerCfg playerCfg = {configOptions.RTPlogFile, configOptions.CANlogFile, configOptions.bindAddr, configOptions.bindPort, 11223344
            , configOptions.canDeviceName, configOptions.canType, configOptions.rewindLog
      };

      printf("Stream file %s to %s:%i\n", configOptions.RTPlogFile, configOptions.bindAddr,  configOptions.bindPort);
      int err = dumpPlayerInit(&session.player, &playerCfg);
      if(0 != err)
      {
         fprintf(stderr, "dumpPlayerInit() failed(%s)\n", strerror(err));
         return err;
      }
      err = dumpPlayerStart(&session.player);
      pause();
   }

   int parentfd = socket(AF_INET, SOCK_STREAM, 0);
   if (parentfd < 0)
   {
      fprintf(stderr, "socket() failed(%s)\n", strerror(errno));
      return EXIT_FAILURE;
   }

   struct sockaddr_in serveraddr;
   memset(&serveraddr, 0, sizeof(serveraddr));

   serveraddr.sin_family = AF_INET;

   if(NULL != configOptions.bindAddr)
   {
      if (inet_aton(configOptions.bindAddr, &serveraddr.sin_addr)==0)
      {
         fprintf(stderr, "inet_aton(%s) failed\n", configOptions.bindAddr);
         return EXIT_FAILURE;
      }
   }
   else
   {
      serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
      configOptions.bindAddr = "0.0.0.0";
   }

   serveraddr.sin_port = htons(configOptions.bindPort);

   int optval = 1;
   setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

   if (bind(parentfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
   {
      fprintf(stderr, "bind() failed(%s)\n", strerror(errno));
      return EXIT_FAILURE;
   }
   if (listen(parentfd, 5) < 0)
   {
      fprintf(stderr, "listen() failed(%s)\n", strerror(errno));
      return EXIT_FAILURE;
   }

   struct sockaddr_in clientaddr;
   socklen_t clientlen = sizeof(clientaddr);
   while(1)
   {
      int childfd; /* child socket */

      childfd = accept(parentfd, (struct sockaddr *) &clientaddr, &clientlen);
      if (childfd < 0)
      {
         fprintf(stderr, "accept() failed(%s)\n", strerror(errno));
         exit(EXIT_FAILURE);
      }

      session.clientIp = inet_ntoa(clientaddr.sin_addr);
      if(session.clientIp)
      {
         printf("client %s connected\n", session.clientIp);
      }

      char buf[512];
      memset(&buf, 0, sizeof(buf));
      while(1)
      {
         int n = read(childfd, buf, sizeof(buf));
         if (n < 0)
         {
            fprintf(stderr, "read() failed(%s)\n", strerror(errno));
            break;
         }
         else if(0 == n)
         {
            printf("Connection with %s closed\n", session.clientIp);
            break;
         }
         processRequest(buf, n, childfd);
      }

      dumpPlayerStop(&session.player);
      dumpPlayerDeinit(&session.player);
      close(childfd);
   }
}

/**
 * Parses the input RTSP SETUP request and extracts the client_port value for RTP
 * transmitting.
 *
 * @return 0 on success( port1, port2 is filled with extracted data) or -1 on failure.
 */
int getRequestClientPort(const char *data, uint32_t *port1, uint32_t *port2)
{
   char *tmpData = strdup(data);
   if(NULL == tmpData)
   {
      return -1;
   }

   char* p;

   const char* portPrefix = "client_port=";
   const char* delims = "\r\n";
   p = strtok(tmpData, delims);

   while( p != NULL )
   {
      char* port = strstr(p, portPrefix);
      if(NULL != port)
      {
         port = port + strlen(portPrefix);
         int count = sscanf(port,"%u-%u", port1, port2);
         if(2 == count)
         {
            free(tmpData);
            return 0;
         }
         break;
      }
      p = strtok( NULL, delims );
   }

   free(tmpData);
   return -1;
}


/**
 * Parses the input RTSP request and extracts the command sequence number (CSeq:) value.
 *
 * @return command sequence number (CSeq:) value or -1 on failure
 */
int getRequestSequenceNumber(const char *data)
{
   char *tmpData = strdup(data);
   if(NULL == tmpData)
   {
      return -1;
   }

   int seqNumber = -1;
   char* p;

   const char* seq = "CSeq: ";
   const char* delims = "\r\n";
   p = strtok(tmpData, delims);

   while( p != NULL )
   {
      if(0 == strncmp(p, seq, strlen(seq)))
      {
         seqNumber = atoi(p + strlen(seq));
         break;
      }
      p = strtok( NULL, delims );
   }

   free(tmpData);
   return seqNumber;
}

