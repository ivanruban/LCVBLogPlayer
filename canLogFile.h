#ifndef _CAN_LOG_FILE__
#define _CAN_LOG_FILE__

#include <stdint.h>
#include <time.h>
#include <stdio.h>

#include "logFile.h"


class CanLogFile : public ILogFile
{
public:
   CanLogFile();
   ~CanLogFile();
   /**
    * Reads the next packet.
    * If end of file is reached the 0 is returned.
    *
    * @return amount of read data or -1 on error (errno is set to the error code)
    */
   int read(packetType &type, timeval &ts, char *data, const int size);

   int open(const char* fname);
   void close();

private:
   FILE *fp;
   uint64_t  timeBase;//rts value form log header(rts: 1458726428015650 ts: 2659501121)
};

#endif // _CAN_LOG_FILE__
