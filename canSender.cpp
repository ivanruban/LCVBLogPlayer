#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>

#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "eventlog.h"
#include "canSender.h"

/**
 * Opens the given CAN device and configures its frame type.
 *
 * @return POSIX error code or 0 on success
 */
int canSenderInit(canSender *ctx, const char *path, const canFrameType frameType)
{
   if((ctx->canSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
   {
      fprintf(stderr,"socket(PF_CAN, SOCK_RAW, CAN_RAW) failed(%s)\n", strerror(errno));
      return errno;
   }

   struct sockaddr_can addr;
   struct ifreq ifr;
   strcpy(ifr.ifr_name, path);
   int err = ioctl(ctx->canSocket, SIOCGIFINDEX, &ifr);
   if(-1 == err)
   {
      fprintf(stderr,"ioctl(SIOCGIFINDEX) failed(%s)\n", strerror(errno));
      return errno;
   }
   addr.can_ifindex = ifr.ifr_ifindex;
   addr.can_family  = AF_CAN;
   if(bind(ctx->canSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
   {
      fprintf(stderr,"bind(AF_CAN) failed(%s)\n", strerror(errno));
      return errno;
   }

   return 0;
}

/**
 * Construct the CAN message according to desired format and send it.
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

   can_frame frame;

   frame.can_id = canPkt->id;
   frame.can_dlc = canPkt->len;
   memcpy(frame.data, canPkt->data, sizeof(frame.data));

   int retryCount=1000;
write:
   int err = write(ctx->canSocket, &frame, sizeof(can_frame));
   if (-1 == err)
   {
      if((ENOBUFS == errno) && (retryCount--))
      {
         usleep(10);
         goto write;
      }

      fprintf(stderr,"CAN: write() failed(%s)\n", strerror(errno));
      return errno;
   }

   return 0;
}

/**
 * Closes CAN device handler.
 */
void canSenderDeinit(canSender *ctx)
{
   if(-1 != ctx->canSocket)
   {
      close(ctx->canSocket);
      ctx->canSocket = -1;
   }
}
