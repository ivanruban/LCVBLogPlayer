/**
 *
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

#define SWAP2(i)           (static_cast<uint16_t>((static_cast<uint16_t>(i) << 8) | (static_cast<uint16_t>(i) >> 8)))
#define SWAP4(i)           (((i)<<24) | (((i)& 0x0000FF00)<<8) | (((i)& 0x00FF0000)>>8) | ((i)>>24) )

struct sniff_rtp
{
   unsigned int cc:4;        /* CSRC count */
   unsigned int x:1;         /* header extension flag */
   unsigned int p:1;         /* padding flag */
   unsigned int version:2;   /* protocol version */
   unsigned int pt:7;        /* payload type */
   unsigned int m:1;         /* marker bit */
   unsigned int seq:16;      /* sequence number */
   uint32_t ts;               /* timestamp */
   uint32_t ssrc;             /* synchronization source */
};

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
         fprintf(stderr, "fread() failed(%i)\n", err);
         break;
      }

      if((header->type != packetHeader.type) || (header->len != packetHeader.len))
      {
         continue;
      }

      if(PACKET_TYPE_RTP == packetHeader.type)
      {
         sniff_rtp *a = (sniff_rtp *)pkt;
         sniff_rtp *b = (sniff_rtp *)buf;
         if(SWAP4(a->ts) == SWAP4(b->ts))
         {
            ts->tv_sec = packetHeader.sec;
            ts->tv_usec = packetHeader.usec;
            return 0;
         }
         else
         {
            printf("%u - %u\n", SWAP4(a->ts), SWAP4(b->ts));
         }
      }
   }
   return -1;
}

/**
 * Calculate time difference in microseconds
 */
static int timeDiffUsec(struct timeval *a, struct timeval *b)
{
   struct timeval res;
   timersub(a, b, &res);

   return  res.tv_sec * 1e6 + res.tv_usec;
}

int main(int argc, char **argv)
{
   if(argc != 3)
   {
      printf("Usage: %s dump1.bin dump2.bin\n", argv[0]);
      return EXIT_FAILURE;
   }
   const char *afname = argv[1];
   const char *bfname = argv[2];

   FILE *a;
   a = fopen(afname, "r");
   if (a == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", afname, strerror(errno));
      return EXIT_FAILURE;
   }

   FILE *b;
   b = fopen(bfname, "r");
   if (b == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", bfname, strerror(errno));
      fclose(a);
      return EXIT_FAILURE;
   }
   fseek(a, sizeof(eventLogHeader), SEEK_SET);
   fseek(b, sizeof(eventLogHeader), SEEK_SET);

   char bufA[2000];
   struct timeval prevTSA = {0,0};
   struct timeval prevTSB = {0,0};
   eventLogPacket packetHeader;
   for(int i=0;;i++)
   {
      int err = fread(&packetHeader, 1, sizeof(eventLogPacket), a);
      if(sizeof(eventLogPacket) != err)
      {
         if(feof(a))
         {
            printf("end of file a!\n");
         }
         fprintf(stderr, "fread() failed(%i)\n", err);
         break;
      }

      if(packetHeader.len > sizeof(bufA))
      {
         fprintf(stderr, "Suspicious packet len: (%u)\n", packetHeader.len);
         break;
      }
      err = fread(bufA, 1, packetHeader.len, a);
      if(packetHeader.len != err)
      {
         fprintf(stderr, "fread() failed(%i)\n", err);
         break;
      }

      if(PACKET_TYPE_RTP != packetHeader.type)
      {
         continue;
      }

      struct timeval tsB;
      err = searchForPacket(b, &packetHeader, bufA, &tsB);
      if(0 == err)
      {
         int diff = abs(timeDiffUsec((struct timeval *)&packetHeader, &prevTSA) - timeDiffUsec(&tsB, &prevTSB));
         printf("%i  %li.%li -> %li.%li(%i)\n", i, packetHeader.sec, packetHeader.usec, tsB.tv_sec, tsB.tv_usec, diff);
      }
      else
      {
         printf("Packet %i wasn't found!\n", i);
         break;
      }
      prevTSA.tv_sec = packetHeader.sec;
      prevTSA.tv_usec= packetHeader.usec;

      prevTSB.tv_sec = tsB.tv_sec;
      prevTSB.tv_usec= tsB.tv_usec;
   }

   fclose(a);
   fclose(b);
   return EXIT_SUCCESS;
}
