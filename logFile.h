#ifndef _LOG_FILE_H_
#define _LOG_FILE_H_

#include <stdint.h>
#include <time.h>

#include "eventlog.h"


/**
 * Interface which abstract different log files reading like CAN, PCAP, etc logs.
 */
class ILogFile
{
public:

   virtual ~ILogFile() {};

   /**
    * Reads the next packet.
    * If end of file is reached the 0 is returned.
    *
    * @return amount of read data or -1 on error (errno is set to the error code)
    */
   virtual int read(packetType &type, timeval &ts, char *data, const int size) = 0;

   virtual int open(const char* fname) = 0;
   virtual void close() = 0;
};

#endif // _LOG_FILE_H_
