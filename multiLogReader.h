#ifndef _MULTI_LOG_READER__
#define _MULTI_LOG_READER__

#include <stdint.h>
#include <time.h>
#include <stdio.h>

#include "logFile.h"
#include <vector>

class MultiLogReader
{
public:

   MultiLogReader(std::vector<ILogFile*> &fileList);

   /**
    * Reads the next packet.
    * If end of file is reached the 0 is returned.
    *
    * @return amount of read data or -1 on error (errno is set to the error code)
    */
   int read(packetType &type, timeval &ts, char *data, const int size);

   struct fileContext
   {
      fileContext()
      {
         type = PACKET_TYPE_MAX;
         ts.tv_sec = 0;
         ts.tv_usec = 0;
         endIsReached = false;
         valid = false;
         size = 0;
      }

      packetType type;
      timeval ts;
      char buff[2000];
      bool endIsReached;
      bool valid;
      int size;
   };

private:
   std::vector<ILogFile*> &files;
   std::vector<fileContext> contexts;
};

#endif // _MULTI_LOG_READER__
