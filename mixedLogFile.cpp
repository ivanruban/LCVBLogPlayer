#include <stdint.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "eventlog.h"
#include "mixedLogFile.h"


int MixedLogFile::open(const char* fname)
{
   this->fp = fopen(fname, "r");
   if (this->fp == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", fname, strerror(errno));
      return errno;
   }

   return 0;
}

void MixedLogFile::close()
{
   if(NULL != this->fp)
   {
      fclose(this->fp);
      this->fp = NULL;
   }
}

MixedLogFile::~MixedLogFile()
{
   close();
}


MixedLogFile::MixedLogFile()
{
   this->fp = NULL;
}

/**
 * Reads the next packet.
 * If end of file is reached the 0 is returned.
 *
 * @return amount of read data or -1 on error (errno is set to the error code)
 */
int MixedLogFile::read(packetType &type, timeval &ts, char *data, const int size)
{
   eventLogPacket packetHeader;
   int err = fread(&packetHeader, 1, sizeof(eventLogPacket), this->fp);
   if(sizeof(eventLogPacket) != err)
   {
      fprintf(stderr, "fread() failed(%i)\n", err);
      if(feof(this->fp))
      {
         return 0;
      }
      return -1;
   }

   if(packetHeader.len > size)
   {
      fprintf(stderr, "Suspicious packet len: (%u)\n", packetHeader.len);
      errno = ENOMEM;
      return -1;
   }

   err = fread(data, 1, packetHeader.len, this->fp);
   if(packetHeader.len != err)
   {
      fprintf(stderr, "fread() failed(%i)\n", err);
      return -1;
   }

   type = (packetType)(packetHeader.type);
   ts.tv_sec = packetHeader.sec;
   ts.tv_usec  = packetHeader.usec;

   return packetHeader.len;
}
