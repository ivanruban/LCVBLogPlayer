#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include <errno.h>

#include "eventlog.h"
#include "canSender.h"

/**
 * Opens the given CAN device and configures its bitrate and frame type.
 *
 * @return POSIX error code or 0 on success
 */
int canSenderInit(canSender *ctx, const char *path, const canFrameType frameType, const int bitrate)
{
   ctx->canHdl = LINUX_CAN_Open(path, O_RDWR);
   if(NULL == ctx->canHdl)
   {
      fprintf(stderr,"LINUX_CAN_Open(%s) failed(%s)\n", path, strerror(errno));
      return errno;
   }

   char canVersion[VERSIONSTRING_LEN];
   int err = CAN_VersionInfo(ctx->canHdl, canVersion);
   if (err)
   {
      fprintf(stderr,"CAN_VersionInfo() failed(%s)\n", strerror(errno));
      return errno;
   }

   int canBitRate;
   int canType = (frameType == CAN_FRAME_STD_TYPE) ? CAN_INIT_TYPE_ST : CAN_INIT_TYPE_EX;
   switch(bitrate)
   {
      case 5:
         canBitRate = CAN_BAUD_5K;
      break;
      case 10:
         canBitRate = CAN_BAUD_10K;
      break;
      case 20:
         canBitRate = CAN_BAUD_20K;
      break;
      case 50:
         canBitRate = CAN_BAUD_50K;
      break;
      case 100:
         canBitRate = CAN_BAUD_100K;
      break;
      case 125:
         canBitRate = CAN_BAUD_125K;
      break;
      case 250:
         canBitRate = CAN_BAUD_250K;
      break;
      case 1000:
         canBitRate = CAN_BAUD_1M;
      break;
      case 500:
      default:
         canBitRate = CAN_BAUD_500K;
      break;
   }

   printf("CAN_Version: %s, type: %s bitrate: %i\n", canVersion
           , (frameType == CAN_FRAME_STD_TYPE) ? "STD" : "EXT", bitrate);

   err = CAN_Init(ctx->canHdl, canBitRate, canType);
   if (err)
   {
      fprintf(stderr,"CAN_Init() failed(%s)\n", strerror(errno));
      return errno;
   }

   return 0;
}

/**
 * Construct the CAN message according to peak-linux-driver library format
 * and send it with 100usec timeout.
 *
 * Input buffer should point to canEvent structure(@see eventlog.h).
 *
 * @return POSIX error code or 0 on success
 */
int canSenderSend(canSender *ctx, const char *buf, const int size)
{
   canEvent *canPkt = (canEvent *)buf;
   if(size != sizeof(canEvent))
   {
      return EINVAL;
   }

   TPCANMsg msg;
   msg.ID = canPkt->id;
   msg.LEN = canPkt->len;
   msg.MSGTYPE = MSGTYPE_STANDARD;
   memcpy(msg.DATA, canPkt->data, sizeof(msg.DATA));

   int err = LINUX_CAN_Write_Timeout(ctx->canHdl, &msg, 1000);
   if (err)
   {
      fprintf(stderr,"CAN_Write() failed(%s)\n", strerror(errno));
      return EIO;
   }

   return 0;
}

/**
 * Closes CAN device handler.
 */
void canSenderDeinit(canSender *ctx)
{
   if(NULL != ctx->canHdl)
   {
      CAN_Close(ctx->canHdl);
      ctx->canHdl = NULL;
   }
}

