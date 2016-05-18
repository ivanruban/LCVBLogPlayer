#ifndef _RTP_SENDER_H
#define _RTP_SENDER_H

#include <stdint.h>
#include <sys/socket.h>

/**
 * Describes internal data necessary for establish RTP over UDP transmitting such as
 * socket fd, network address, etc.
 */
typedef struct
{
   int socket;
   struct sockaddr_in sockAddr;
   uint32_t ssrc; //RTP: Synchronization source identifier uniquely identifies the source of a stream
}rtpSender;

/**
 * Initializes UDP socket, convert the addr string to numeric representation.
 * The given TTP:SSRC is used for all sent packet.
 *
 * @return POSIX error code or 0 on success
 */
int rtpSenderInit(rtpSender *ctx, const char* addr, int port, uint32_t ssrc);

/**
 * Set the TTP:SSRC to the given ID and send the packet as UDP stream.
 *
 * @return POSIX error code or 0 on success
 */
int rtpSenderSend(rtpSender *ctx, const char *buf, const int size);

/**
 * Close network socket.
 */
void rtpSenderDeinit(rtpSender *ctx);

#endif // _RTP_SENDER_H
