#ifndef _CAN_SENDER_H
#define _CAN_SENDER_H

#include <stdint.h>
#include <stdio.h>

enum canFrameType
{
   CAN_FRAME_STD_TYPE = 0,
   CAN_FRAME_EXT_TYPE,
   CAN_FRAME_TYPE_MAX
};

typedef struct
{
   int canSocket;
}canSender;

/**
 * Opens the given CAN device and configures its frame type.
 *
 * @return POSIX error code or 0 on success
 */
int canSenderInit(canSender *ctx, const char *path, const canFrameType frameType);

/**
 * Construct the CAN message according to desired format and send it.
 *
 * Input buffer should point to canEvent structure(@see eventlog.h).
 *
 * @return POSIX error code or 0 on success
 */
int canSenderSend(canSender *ctx, const char *buf, const int size);

/**
 * Closes CAN device handler.
 */
void canSenderDeinit(canSender *ctx);

#endif // _CAN_SENDER_H
