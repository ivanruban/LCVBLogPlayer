#ifndef _EVENT_LOG_H
#define _EVENT_LOG_H

/**
 * Describes events(packets) list file which will be read and replayed by server to
 * reproduce the logged session. Two packet types CAN and RTP are currently listed, but
 * it can be extended to support other kind of event.
*/


#include <stdint.h>


enum packetType
{
   PACKET_TYPE_CAN = 0,
   PACKET_TYPE_RTP,
   PACKET_TYPE_MAX
};

/**
 * Events(packets) list file header for verification and version check.
*/
struct eventLogHeader
{
   uint8_t id[4]; //'ELOG'
   uint32_t version;
};

struct eventLogPacket
{
   uint64_t sec;  //timestamp seconds
   uint64_t usec; //timestamp microseconds
   uint16_t type; //@see packetType enum
   uint16_t len;  //total length
};

/**
 * Can packet(PACKET_TYPE_CAN) content
*/
struct canEvent
{
   uint32_t id;
   uint32_t len;   // count of data bytes (0..8)
   uint8_t data[8];
};

/**
 * RTP packet(PACKET_TYPE_RTP) header(@see https://en.wikipedia.org/wiki/Real-time_Transport_Protocol)
 * followed by packet data.
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

#endif // _EVENT_LOG_H
