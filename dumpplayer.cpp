#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <string.h>

#include <sys/types.h>
#include <sys/time.h>

#include <errno.h>
#include <time.h>

#include "dumpplayer.h"
#include "eventlog.h"
#include "multiLogReader.h"


/**
 * Calculate time difference in microseconds
 */
static uint32_t timeDiffUsec(struct timeval *a, struct timeval *b)
{
   struct timeval res;
   timersub(a, b, &res);

   return  res.tv_sec * 1e6 + res.tv_usec;
}

/**
 * Calculate time difference in microseconds
 */
static uint32_t timeDiffUsec(struct timespec *a, struct timespec *b)
{
   struct timeval aUsec = {a->tv_sec, a->tv_nsec/1000};
   struct timeval bUsec = {b->tv_sec, b->tv_nsec/1000};

   return timeDiffUsec(&aUsec, &bUsec);
}

//helper variable to control log evens playback loop
static bool playbackActive = false;

/**
 * Reads events from the event log and sends it with appropriate
 * player(CAN or RTP). Calculates time difference to the next event and
 * sleep for this timeout.
 *
 * Once end of file is reached the player rewinds log and start from the
 * begging.
 */
static void *dumpPlayerbackThread(void *arg)
{
   dumpPlayer *ctx = (dumpPlayer *)arg;

   std::vector<ILogFile*> fileList;
   fileList.push_back(&ctx->canLog);
   fileList.push_back(&ctx->rtpLog);

   MultiLogReader reader(fileList);

   packetType type;
   char data[2000];
   timeval prevTs = {0, 0};
   timeval ts;

   struct timespec startTimestap;
   struct timespec endTimestap;

   while(playbackActive)
   {
      int err = reader.read(type, ts,  data, sizeof(data));
      if(-1 == err)
      {
         fprintf(stderr, "reader.read() failed(%i)\n", errno);
         break;
      }
      if(0 == err)
      {
         //@TODO process rewind
         break;
      }
      int len = err;

      clock_gettime(CLOCK_MONOTONIC, &endTimestap);
      int timespend = timeDiffUsec(&endTimestap, &startTimestap);
      if((0 != prevTs.tv_sec) && (0 != prevTs.tv_usec))
      {
         int time2nextEvent = timeDiffUsec(&ts, &prevTs);
         int time2sleep = time2nextEvent - timespend;

         if(1 < time2sleep)
            usleep(time2sleep);
      }
      clock_gettime(CLOCK_MONOTONIC, &startTimestap);

      if(PACKET_TYPE_RTP == type)
      {
         err = rtpSenderSend(&ctx->rtpSend, data, len);
         if(0 != err)
         {
            fprintf(stderr,"rtpSenderSend() failed(%s)\n", strerror(err));
            break;
         }
      }
      else if (PACKET_TYPE_CAN == type)
      {
         err = canSenderSend(&ctx->canSend, data, len);
         if(0 != err)
         {
            fprintf(stderr,"canSenderSend() failed(%s)\n", strerror(err));
            break;
         }
      }
      else
      {
         fprintf(stderr,"Unknown packet type(%u)\n", type);
         continue;
      }
      prevTs = ts;
   }

   return 0;
}

/**
 * Enable events playback and create the playback thread.
 *
 * @return POSIX error code or 0 on success
 */
int dumpPlayerStart(dumpPlayer *ctx)
{
   playbackActive = true;

   int err = pthread_create(&ctx->playbackThread, NULL, dumpPlayerbackThread, ctx);
   if(0 != err)
   {
      fprintf(stderr,"pthread_create() failed(%s)\n", strerror(err));
      return err;
   }

   return err;
}

/**
 * Disable events playback and join the playback thread.
 *
 * @return POSIX error code or 0 on success
 */
int dumpPlayerStop(dumpPlayer *ctx)
{
   if(playbackActive)
   {
      playbackActive = false;
      pthread_join(ctx->playbackThread, NULL);
   }
   return 0;
}

/**
 * Opens the event log file and checks its header.
 * Also creates and configures CAN and RTP message players.
 *
 * @return POSIX error code or 0 on success
 */
int dumpPlayerInit(dumpPlayer *ctx, dumpPlayerCfg *cfg)
{
   ctx->rewind = cfg->rewind;

   int err = ctx->canLog.open(cfg->CANfname);
   if(0 != err)
   {
      fprintf(stderr, "canLog.open(%s) failed(%s)\n", cfg->CANfname, strerror(err));
      return err;
   }

   err = ctx->rtpLog.open(cfg->RTPfname);
   if(0 != err)
   {
      fprintf(stderr, "rtpLog.open(%s) failed(%s)\n", cfg->RTPfname, strerror(err));
      ctx->canLog.close();
      return err;
   }

   err = rtpSenderInit(&ctx->rtpSend, cfg->addr, cfg->port, cfg->ssrc);
   if(0 != err)
   {
      fprintf(stderr, "rtpSenderInit() failed(%s)\n", strerror(err));
      ctx->canLog.close();
      ctx->rtpLog.close();
      return err;
   }

   err = canSenderInit(&ctx->canSend, cfg->canDeviceName, cfg->canType);
   if(0 != err)
   {
      fprintf(stderr, "canSenderInit() failed(%s)\n", strerror(err));
      ctx->canLog.close();
      ctx->rtpLog.close();
      rtpSenderDeinit(&ctx->rtpSend);
      return err;
   }

   return 0;
}

/**
 * Close events log file handle and release CAN/RTP players.
 */
void dumpPlayerDeinit(dumpPlayer *ctx)
{
   ctx->canLog.close();
   ctx->rtpLog.close();
   rtpSenderDeinit(&ctx->rtpSend);
   canSenderDeinit(&ctx->canSend);
}
