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

   eventLogPacket packetHeader;
   char buf[2000];
   timeval prevTs = {0, 0};

   struct timespec startTimestap;
   struct timespec endTimestap;
   while(playbackActive)
   {
      clock_gettime(CLOCK_MONOTONIC, &startTimestap);

      int err = fread(&packetHeader, 1, sizeof(eventLogPacket), ctx->fp);
      if(sizeof(eventLogPacket) != err)
      {
         if(feof(ctx->fp))
         {
            if(ctx->rewind)
            {
               printf("rewind dump log!\n");
               prevTs.tv_sec = prevTs.tv_usec = 0;
               fseek(ctx->fp, sizeof(eventLogHeader), SEEK_SET);
               continue;
            }
            else
            {
               break;
            }
         }

         fprintf(stderr, "fread() failed(%i)\n", err);
         break;
      }

      if(packetHeader.len > sizeof(buf))
      {
         fprintf(stderr, "Suspicious packet len: (%u)\n", packetHeader.len);
         break;
      }
      err = fread(buf, 1, packetHeader.len, ctx->fp);
      if(packetHeader.len != err)
      {
         fprintf(stderr, "fread() failed(%i)\n", err);
         break;
      }

      if(PACKET_TYPE_RTP == packetHeader.type)
      {
         err = rtpSenderSend(&ctx->rtpSend, buf, packetHeader.len);
         if(0 != err)
         {
            fprintf(stderr,"rtpSenderSend() failed(%s)\n", strerror(err));
            break;
         }
      }
      else if (PACKET_TYPE_CAN == packetHeader.type)
      {
         err = canSenderSend(&ctx->canSend, buf, packetHeader.len);
         if(0 != err)
         {
            fprintf(stderr,"canSenderSend() failed(%s)\n", strerror(err));
            break;
         }
      }
      else
      {
         fprintf(stderr,"Unknown packet type(%u)\n", packetHeader.type);
         continue;
      }
      clock_gettime(CLOCK_MONOTONIC, &endTimestap);

      int timespend = timeDiffUsec(&endTimestap, &startTimestap);

      if((0 != prevTs.tv_sec) && (0 != prevTs.tv_usec))
      {
         int time2nextEvent = timeDiffUsec((struct timeval *)&packetHeader, &prevTs);
         int time2sleep = time2nextEvent - timespend;
         if(1 < time2sleep)
            usleep(time2sleep);
      }
      prevTs.tv_sec = packetHeader.sec;
      prevTs.tv_usec = packetHeader.usec;
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

   if((ctx->fp = fopen(cfg->fname, "r")) == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", cfg->fname, strerror(errno));
      return errno;
   }

   eventLogHeader logHeader;
   int err = fread(&logHeader, 1, sizeof(eventLogHeader), ctx->fp);
   if(sizeof(eventLogHeader) != err)
   {
      fprintf(stderr, "fread() failed(%i)\n", err);
      fclose(ctx->fp);
      ctx->fp = NULL;
      return EINVAL;
   }
   if((0 != memcmp(logHeader.id, "ELOG", sizeof(logHeader.id))) || (1 != logHeader.version))
   {
      fprintf(stderr, "Incorrect log file format\n");
      fclose(ctx->fp);
      ctx->fp = NULL;
      return EINVAL;
   }

   err = rtpSenderInit(&ctx->rtpSend, cfg->addr, cfg->port, cfg->ssrc);
   if(0 != err)
   {
      fprintf(stderr, "rtpSenderInit() failed(%s)\n", strerror(err));
      fclose(ctx->fp);
      ctx->fp = NULL;
      return err;
   }

   err = canSenderInit(&ctx->canSend, cfg->canDeviceName, cfg->canType ,cfg->canBitrate);
   if(0 != err)
   {
      fprintf(stderr, "canSenderInit() failed(%s)\n", strerror(err));
      fclose(ctx->fp);
      ctx->fp = NULL;
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
   if(NULL != ctx->fp)
   {
      fclose(ctx->fp);
      ctx->fp = NULL;
   }
   rtpSenderDeinit(&ctx->rtpSend);
   canSenderDeinit(&ctx->canSend);
}
