#ifndef _MIXED_LOG_FILE__
#define _MIXED_LOG_FILE__

#include <stdint.h>
#include <time.h>
#include <stdio.h>

#include "logFile.h"


class MixedLogFile : public ILogFile
{
public:
   MixedLogFile();
   ~MixedLogFile();
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
};

#endif // _MIXED_LOG_FILE__
