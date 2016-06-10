#include <stdint.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <stdlib.h>

#include "eventlog.h"
#include "canLogFile.h"


int CanLogFile::open(const char* fname)
{
   this->fp = fopen(fname , "r");
   if (this->fp == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", fname , strerror(errno));
      return errno;
   }

   char buf[256];
   uint64_t rts = 0u;
   uint64_t baseTs = 0u;
   while(fgets(buf, sizeof(buf), this->fp) != NULL)
   {
      if(0==strncmp(buf, "rts: ", 5))
      {
         int count = sscanf(buf,"rts: %lu  ts: %lu", &rts, &baseTs);
         if(2 != count)
         {
            fprintf(stderr, "Wrong RTS specification string: (%s)", buf);
            fclose(this->fp);
            this->fp = NULL;
            return EIO;
         }
         this->timeBase = rts;
         break;
      }
   }

   if(0u == this->timeBase)
   {
      fprintf(stderr, "Failed to init can dump timebase!\n");
      fclose(this->fp);
      this->fp = NULL;
      return EIO;
   }
   return 0;
}

void CanLogFile::close()
{
   if(NULL != this->fp)
   {
      fclose(this->fp);
      this->fp = NULL;
      this->timeBase = 0u;
   }
}

CanLogFile::~CanLogFile()
{
   close();
}


CanLogFile::CanLogFile()
{
   this->timeBase = 0u;
   this->fp = NULL;
}

/**
 * Reads the next packet.
 * If end of file is reached the 0 is returned.
 *
 * @return amount of read data or -1 on error (errno is set to the error code)
 */
int CanLogFile::read(packetType &type, timeval &ts, char *data, const int size)
{
   if(size < (int)sizeof(canEvent))
   {
      errno = EINVAL;
      return -1;
   }
   type = PACKET_TYPE_CAN;

   canEvent *pkt = (canEvent *)(data);
   uint32_t canData[8];

   char buf[256];
   while(fgets(buf, sizeof(buf), this->fp) != NULL)
   {
      if(0==strncmp(buf, "ts: ", 4))//ts: 000000007938   084   [8]  66 D2 66 AE 04 50 71 E9
      {
         uint64_t pktts;
         int count = sscanf(buf,"ts: %lu %x [%u] %x %x %x %x %x %x %x %x", &pktts, &pkt->id, &pkt->len
                            ,canData, canData+1, canData+2, canData+3, canData+4, canData+5, canData+6, canData+7 );
         if(11 != count)
         {
            fprintf(stderr, "Wrong line format: (%s)", buf);
            continue;
         }
         uint64_t  msgTimestamp = pktts + this->timeBase;
         ldiv_t parsedTime = ldiv(msgTimestamp, 1e6);

         for(int i=0; i<8; i++)
         {
            pkt->data[i] = (uint8_t)canData[i];
         }

         ts.tv_sec = parsedTime.quot;
         ts.tv_usec = parsedTime.rem;
         return sizeof(canEvent);
      }
   }

   if(feof(this->fp))
   {
      return 0;
   }

   return -1;
}
