/**
 * Debug utility to compare two RTP/CAN messages log in player's format(@see eventlog.h). It tries to find
 * an message occurred in the first file in the second one and outputs its timestamp(in ms) and message type in
 * CSV format.
 *      <type>; <timestamp in first file in ms>; <timestamp in second file in ms>;
 * Like:
 *   RTP; 1458726003330; 1310797554;
 * Output may processed with Excel to compute player timing performance.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <sys/time.h>


#include <limits.h>

#include "eventlog.h"

static uint64_t timeval2ms(struct timeval *a)
{
   uint64_t res = a->tv_sec * 1000;
   res+=a->tv_usec/1000;

   return  res;
}

int searchForPacket(FILE *file, eventLogPacket *header, const char* pkt, struct timeval *ts)
{
   char buf[2000];
   eventLogPacket packetHeader;
   for(;;)
   {
      int err = fread(&packetHeader, 1, sizeof(eventLogPacket), file);
      if(sizeof(eventLogPacket) != err)
      {
         if(!feof(file))
         {
            fprintf(stderr, "fread() failed(%i)\n", err);
         }
         break;
      }

      if(packetHeader.len > sizeof(buf))
      {
         fprintf(stderr, "Suspicious packet len: (%u)\n", packetHeader.len);
         break;
      }
      err = fread(buf, 1, packetHeader.len, file);
      if(packetHeader.len != err)
      {
         break;
      }

      if((header->type != packetHeader.type) || (header->len != packetHeader.len))
      {
         continue;
      }

      if(PACKET_TYPE_RTP == packetHeader.type)
      {
         rtpHeader *a = (rtpHeader *)pkt;
         rtpHeader *b = (rtpHeader *)buf;
         if(a->ts == b->ts)
         {
            ts->tv_sec = packetHeader.sec;
            ts->tv_usec = packetHeader.usec;
            return 0;
         }
      }
      else if(PACKET_TYPE_CAN == packetHeader.type)
      {
         if(0 == memcmp(pkt, buf, sizeof(canEvent)))
         {
            ts->tv_sec = packetHeader.sec;
            ts->tv_usec = packetHeader.usec;
            return 0;
         }
      }
   }
   return -1;
}

void usage(const char *name)
{
   printf("Usage: %s original_dump.bin recollected_dump.bin\n", name);
   exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
   if(argc < 3)
   {
      usage(argv[0]);
      return EXIT_FAILURE;
   }

   const char *originalDumpName = argv[1];
   const char *recollectedDumpName = argv[2];

   FILE *originalDump;
   originalDump = fopen(originalDumpName, "r");
   if (originalDump == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", originalDumpName, strerror(errno));
      return EXIT_FAILURE;
   }

   /*
    * If CAN and RTP packets are interleaved message order may be changed.
    * I.e. message CAN which originally was before an RTP one may followed
    * after that in a recreated log. To resolve this issue two descriptors are opened for CAN and
    * RTP message searching. So that each type are searched independently.
   */
   FILE *recollectedDumpRTP;
   FILE *recollectedDumpCAN;
   recollectedDumpRTP = fopen(recollectedDumpName, "r");
   recollectedDumpCAN = fopen(recollectedDumpName, "r");
   if (recollectedDumpRTP == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", recollectedDumpName, strerror(errno));
      fclose(originalDump);
      return EXIT_FAILURE;
   }

   fseek(originalDump, sizeof(eventLogHeader), SEEK_SET);
   fseek(recollectedDumpRTP, sizeof(eventLogHeader), SEEK_SET);
   fseek(recollectedDumpCAN, sizeof(eventLogHeader), SEEK_SET);

   char buf[2000];
   eventLogPacket packetHeader;
   for(;;)
   {
      int err = fread(&packetHeader, 1, sizeof(eventLogPacket), originalDump);
      if(sizeof(eventLogPacket) != err)
      {
         if(!feof(originalDump))
         {
            fprintf(stderr, "fread() failed(%i)\n", err);
         }
         break;
      }

      if(packetHeader.len > sizeof(buf))
      {
         fprintf(stderr, "Suspicious packet len: (%u)\n", packetHeader.len);
         break;
      }
      err = fread(buf, 1, packetHeader.len, originalDump);
      if(packetHeader.len != err)
      {
         fprintf(stderr, "fread() failed(%i)\n", err);
         break;
      }
      struct timeval ts;
      const char *busName;
      if(PACKET_TYPE_CAN == packetHeader.type)
      {
         busName = "CAN";
         err = searchForPacket(recollectedDumpCAN, &packetHeader, buf, &ts);
      }
      else
      {
         busName = "RTP";
         err = searchForPacket(recollectedDumpRTP, &packetHeader, buf, &ts);
      }
      if(0 == err)
      {
         printf("%s; %li; %li;\n", busName, timeval2ms((struct timeval *)&packetHeader), timeval2ms(&ts));
      }
      else
      {
         printf("Packet wasn't found!\n");
         break;
      }
   }

   fclose(originalDump);
   fclose(recollectedDumpRTP);
   fclose(recollectedDumpCAN);
   return EXIT_SUCCESS;
}
