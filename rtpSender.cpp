#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <time.h>

#include "rtpSender.h"

#define SWAP4(i)           (((i)<<24) | (((i)& 0x0000FF00)<<8) | (((i)& 0x00FF0000)>>8) | ((i)>>24) )

/**
 * Describes RTP packet header(@see https://en.wikipedia.org/wiki/Real-time_Transport_Protocol)
 */
struct rtpHeader
{
   uint32_t cc:4;        /* CSRC count */
   uint32_t x:1;         /* header extension flag */
   uint32_t p:1;         /* padding flag */
   uint32_t version:2;   /* protocol version */
   uint32_t pt:7;        /* payload type */
   uint32_t m:1;         /* marker bit */
   uint32_t seq:16;      /* sequence number */
   uint32_t ts;              /* timestamp */
   uint32_t ssrc;            /* synchronization source */
};

/**
 * Initializes UDP socket, convert the addr string to numeric representation.
 * The given TTP:SSRC is used for all sent packet.
 *
 * @return POSIX error code or 0 on success
 */
int rtpSenderInit(rtpSender *ctx, const char* addr, int port, uint32_t ssrc)
{
   ctx->ssrc = ssrc;

   ctx->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if(-1 == ctx->socket)
   {
      fprintf(stderr, "socket() failed(%s)\n", strerror(errno));
      return errno;
   }

   memset(&ctx->sockAddr, 0, sizeof(sockaddr_in));
   ctx->sockAddr.sin_family = AF_INET;
   ctx->sockAddr.sin_port = htons(port);
   if(inet_aton(addr, &ctx->sockAddr.sin_addr)==0)
   {
      fprintf(stderr, "inet_aton(%s) failed\n", addr);
      close(ctx->socket);
      ctx->socket = -1;
      return errno;
   }

   return 0;
}

/**
 * Set the TTP:SSRC to the given ID and send the packet as UDP stream.
 *
 * @return POSIX error code or 0 on success
 */
int rtpSenderSend(rtpSender *ctx, const char *buf, const int size)
{
   rtpHeader *rtp = (rtpHeader *)buf;

   rtp->ssrc = SWAP4(ctx->ssrc);

   int err = sendto(ctx->socket, buf, size, 0, (struct sockaddr *)&ctx->sockAddr, sizeof(sockaddr_in));
   if(-1 == err)
   {
      fprintf(stderr,"sendto() failed(%s)\n", strerror(errno));
      return errno;
   }
   return 0;
}

/**
 * Close network socket.
 */
void rtpSenderDeinit(rtpSender *ctx)
{
   if(-1 != ctx->socket)
   {
      close(ctx->socket);
      ctx->socket = -1;
   }
}
