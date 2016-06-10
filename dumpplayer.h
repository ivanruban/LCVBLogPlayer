#ifndef _DUMP_PLAYER_H
#define _DUMP_PLAYER_H

#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <pthread.h>

#include "canLogFile.h"
#include "mixedLogFile.h"

#include "rtpSender.h"
#include "canSender.h"

/**
 * Player of evens log described in "eventlog.h". It contains a processor
 *  for RTP and CAN message types. Playback is done in a separate thread(playbackThread).
 */
typedef struct
{
   CanLogFile canLog;
   MixedLogFile rtpLog;
   pthread_t playbackThread;
   rtpSender rtpSend;
   canSender canSend;
   int rewind;
}dumpPlayer;

typedef struct
{
   const char* RTPfname;
   const char* CANfname;
   const char* addr; //RTP client IP address
   int port;         //client UDP port for RTP streaming
   uint32_t ssrc;    //RTP: Synchronization source identifier uniquely identifies the source of a stream
   const char* canDeviceName; //like can0
   canFrameType canType; //standard or extended CAN frames
   int rewind;       //if 1 - the log is rewind once end of file is reached
}dumpPlayerCfg;

/**
 * Opens the event log file and checks its header.
 * Also creates and configures CAN and RTP message players.
 *
 * @return POSIX error code or 0 on success
 */
int dumpPlayerInit(dumpPlayer *ctx, dumpPlayerCfg *cfg);

/**
 * Enable events playback and create the playback thread.
 *
 * @return POSIX error code or 0 on success
 */
int dumpPlayerStart(dumpPlayer *ctx);

/**
 * Disable events playback and join the playback thread.
 *
 * @return POSIX error code or 0 on success
 */
int dumpPlayerStop(dumpPlayer *ctx);

/**
 * Close events log file handle and release CAN/RTP players.
 */
void dumpPlayerDeinit(dumpPlayer *ctx);

#endif // _DUMP_PLAYER_H
