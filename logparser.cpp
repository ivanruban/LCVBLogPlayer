/**
 * Parses two given packets log files: PCAP and CAN. Based on this file the internal representation of
 * events is constructed in the format described at eventlog.h. Packets of CAN and PCAP logs are sorted in time
 * order.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <limits.h>
#include <pcap.h>

#include "eventlog.h"

#define SWAP2(i)           (static_cast<uint16_t>((static_cast<uint16_t>(i) << 8) | (static_cast<uint16_t>(i) >> 8)))
#define SWAP4(i)           (((i)<<24) | (((i)& 0x0000FF00)<<8) | (((i)& 0x00FF0000)>>8) | ((i)>>24) )

#define ETHERNET_FRAME_MAX_SIZE      (2000u)
//MIN size of RTP packet to sort out some unrelated data and make parsing more safe
#define ETHERNET_FRAME_MIN_SIZE      (12u+14u+20u+8u)

#define SIZE_UDP 8
/* Ethernet header */
struct sniff_ethernet {
   uint8_t ether_dhost[6];
   uint8_t ether_shost[6];
   uint16_t ether_type;
};

/* IP header */
struct sniff_ip {
   uint8_t ip_vhl;    /* version << 4 | header length >> 2 */
   uint8_t ip_tos;    /* type of service */
   uint16_t ip_len;   /* total length */
   uint16_t ip_id;    /* identification */
   uint16_t ip_off;   /* fragment offset field */
   uint8_t ip_ttl;    /* time to live */
   uint8_t ip_p;      /* protocol */
   uint16_t ip_sum;   /* checksum */
   uint32_t ip_src;
   uint32_t ip_dst;
};
#define IP_HL(ip)    (((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)     (((ip)->ip_vhl) >> 4)

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

struct sniff_udp_rtp {
   uint16_t sport;
   uint16_t dport;
   uint16_t length;
   uint16_t checksum;
   sniff_rtp rtp;
};

/**
 * Internal can log parser data contains file handler and timestamp base greeped from
 * CAN log header.
*/
typedef struct
{
   FILE *canDump;
   uint64_t  timeBase;//rts value form log header(rts: 1458726428015650 ts: 2659501121)
}canReader;


/**
 * Reads the next packet from the CAN log and provide it as pointer to a static buffer.
 * If end of file is reached the ENODATA is returned.
 *
 * @return POSIX error code or 0 on success
 */
static int canReadNextPkt(canReader *ctx, struct timeval *ts, char **data, int *size)
{
   static canEvent pkt;
   uint32_t canData[8];

   char buf[256];
   while(fgets(buf, sizeof(buf), ctx->canDump) != NULL)
   {
      if(0==strncmp(buf, "ts: ", 4))//ts: 000000092770   ID:  47 LEN:8 DATA:20 00 00 00 00 00 00 00
      {
         uint64_t pktts;
         int count = sscanf(buf,"ts: %lu   ID: %x LEN:%u DATA:%x %x %x %x %x %x %x %x", &pktts, &pkt.id, &pkt.len
                            ,canData, canData+1, canData+2, canData+3, canData+4, canData+5, canData+6, canData+7 );
         if(11 != count)
         {
            fprintf(stderr, "Wrong line format: (%s)", buf);
            continue;
         }
         uint64_t  msgTimestamp = pktts + ctx->timeBase;
         ldiv_t parsedTime = ldiv(msgTimestamp, 1e6);

         for(int i=0; i<8; i++)
         {
            pkt.data[i] = (uint8_t)canData[i];
         }

         ts->tv_sec = parsedTime.quot;
         ts->tv_usec = parsedTime.rem;
         *size = sizeof(canEvent);
         *data = (char*)&pkt;

         return 0;
      }
   }

   if(feof(ctx->canDump))
   {
      return ENODATA;
   }

   return EIO;
}

/**
 * Open the CAN log file for reading and searching for the rts base time.
 *
 * @return POSIX error code or 0 on success
 */
static int canReaderInit(canReader *ctx, const char *fname)
{
   ctx->timeBase = 0u;

   ctx->canDump = fopen(fname, "r");
   if (ctx->canDump == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", fname, strerror(errno));
      return errno;
   }

   char buf[256];
   uint64_t rts = 0u;
   uint64_t baseTs = 0u;
   while(fgets(buf, sizeof(buf), ctx->canDump) != NULL)
   {
      if(0==strncmp(buf, "rts: ", 5))
      {
         int count = sscanf(buf,"rts: %lu  ts: %lu", &rts, &baseTs);
         if(2 != count)
         {
            fprintf(stderr, "Wrong RTS specification string: (%s)", buf);
            fclose(ctx->canDump);
            ctx->canDump = NULL;
            return EIO;
         }
         ctx->timeBase = rts;
         time_t startTime = rts/1e6;
         printf("CAN log started from  %s", ctime(&startTime));

         break;
      }
   }

   if(0u == ctx->timeBase)
   {
      fprintf(stderr, "Failed to init can dump timebase!\n");
      fclose(ctx->canDump);
      ctx->canDump = NULL;
      return EIO;
   }

   return 0;
}

/**
 * Close CAN log file handler.
 */
void canReaderClose(canReader *ctx)
{
   fclose(ctx->canDump);
   ctx->canDump = NULL;
   return;
}

/**
 * Internal PCAP log parser data contains pcap library handler
*/
typedef struct
{
   pcap_t *fp;
}pcapReader;

/**
 * Open and verify the PCAP dump file with libpcap.
 *
 * @return POSIX error code or 0 on success
 */
int pcapReaderInit(pcapReader *ctx, const char *fname)
{
   char errbuf[PCAP_ERRBUF_SIZE];
   if((ctx->fp = pcap_open_offline(fname, errbuf)) == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", fname, errbuf);
      return EIO;
   }

   return 0;
}

/**
 * Reads the next packet from the PCAP log and filters out a RTP packet.
 * If end of file is reached the ENODATA is returned.
 *
 * @return POSIX error code or 0 on success
 */
int pcapReadNextPkt(pcapReader *ctx, struct timeval *ts, char **data, int *size)
{
   struct pcap_pkthdr *header;
   const uint8_t *pkt_data;
   int res;
   static bool firstPktProcessed = false;

   while((res = pcap_next_ex(ctx->fp, &header, &pkt_data)) >= 0)
   {
      if(!firstPktProcessed)
      {
         printf("PCAP log started from %s", ctime(&header->ts.tv_sec));
         firstPktProcessed = true;
      }

      if(header->len <= ETHERNET_FRAME_MIN_SIZE || header->len > ETHERNET_FRAME_MAX_SIZE)
      {
         continue;
      }
      struct sniff_ethernet *ethernet;
      ethernet = (sniff_ethernet*)pkt_data;
      if(0x0008 != ethernet->ether_type)//IP packet
      {
         continue;
      }

      struct sniff_ip *ip = (sniff_ip*)(pkt_data + sizeof(sniff_ethernet));
      u_int size_ip;
      size_ip = IP_HL(ip)*4;
      if (size_ip < 20)
      {
         continue;
      }
      if(17 != ip->ip_p)//UDP packet
      {
         continue;
      }

      sniff_udp_rtp *udp =(sniff_udp_rtp *)(pkt_data + sizeof(sniff_ethernet) + size_ip);
      if((98 != udp->rtp.pt) || (2 != udp->rtp.version))//likely not an RTP packet
      {
         continue;
      }

//      printf("Packets dump timestamp: %li; len: %u(%u->%u[%u]):ssrc: %x; ts: %u version: %u pt: %u\n", header->ts.tv_sec, header->len
//              , SWAP2(udp->sport), SWAP2(udp->dport), SWAP2(udp->length)
//              , SWAP4(udp->rtp.ssrc), SWAP4(udp->rtp.ts)
//              , udp->rtp.version, udp->rtp.pt
//             );

      *ts = header->ts;
      *data = (char*)&udp->rtp;
      *size = SWAP2(udp->length) - SIZE_UDP;
      break;
   }

   if (res == -1)
   {
      printf("Error reading the packets: %s\n", pcap_geterr(ctx->fp));
      return EIO;
   }
   if (res == -2)
   {
      return ENODATA;
   }
   return 0;
}

/**
 * Close the libpcap handler.
 */
void pcapReaderClose(pcapReader *ctx)
{
   pcap_close(ctx->fp);
   return;
}


int main(int argc, char **argv)
{
   if(argc != 4)
   {
      printf("Usage: %s dump.pcap dump.can out.bin\n", argv[0]);
      return EXIT_FAILURE;
   }
   const char *pcapFile = argv[1];
   const char *canFile = argv[2];
   const char *outFile = argv[3];

   printf("Convert %s/%s -> %s\n",pcapFile, canFile, outFile);

   pcapReader pcapFp;
   int err = pcapReaderInit(&pcapFp, pcapFile);
   if (0 != err)
   {
      fprintf(stderr, "pcapReaderInit(%s) failed(%s)\n", pcapFile, strerror(err));
      return EXIT_FAILURE;
   }

   canReader canFp;
   err = canReaderInit(&canFp, canFile);
   if (0 != err)
   {
      fprintf(stderr, "canReaderInit(%s) failed(%s)\n", canFile, strerror(err));
      pcapReaderClose(&pcapFp);
      return EXIT_FAILURE;
   }

   FILE *file;
   file = fopen(outFile, "w+b");
   if (file == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", outFile, strerror(errno));
      pcapReaderClose(&pcapFp);
      canReaderClose(&canFp);
      return EXIT_FAILURE;
   }
   eventLogHeader outHeader = { {'E','L','O','G'}, 1};
   fwrite(&outHeader, sizeof(eventLogHeader), 1, file);

   /*
    * The cycle below reads a PCAP and a CAN message, then compare timestamps and writes out a
    * packet with earlier ts. After that a next message of written type is read again.
    * */
   struct timeval pcapts = {LONG_MAX, LONG_MAX};
   char *pcapdata = NULL;
   int pcapsize;

   struct timeval cants = {LONG_MAX, LONG_MAX};
   char *candata = NULL;
   int cansize;

   int canMsgCount = 0;
   int rtpMsgCount = 0;
   for(;;)
   {
      eventLogPacket logPacket;
      if(NULL == pcapdata)
      {
         err = pcapReadNextPkt(&pcapFp, &pcapts, &pcapdata, &pcapsize);
         if (0 != err)
         {
            //to mark that the end of file is reached set the ts to MAX value
            if(ENODATA == err)
            {
               pcapts.tv_sec = pcapts.tv_usec = LONG_MAX;
               pcapdata = NULL;
            }
            else
            {
               fprintf(stderr, "pcapReadNextPkt failed(%s)\n", strerror(err));
               break;
            }
         }
      }

      if(NULL == candata)
      {
         err = canReadNextPkt(&canFp, &cants, &candata, &cansize);
         if (0 != err)
         {
            //to mark that the end of file is reached set the ts to MAX value
            if(ENODATA == err)
            {
               cants.tv_sec = cants.tv_usec = LONG_MAX;
               candata = NULL;
            }
            else
            {
               fprintf(stderr, "canReadNextPkt failed(%s)\n", strerror(err));
               break;
            }
         }
      }
      if(timercmp(&pcapts, &cants, <) && pcapdata)
      {
         logPacket.type = PACKET_TYPE_RTP;
         logPacket.sec = pcapts.tv_sec;
         logPacket.usec = pcapts.tv_usec;
         logPacket.len = pcapsize;

         fwrite(&logPacket, sizeof(logPacket), 1, file);
         fwrite(pcapdata, pcapsize, 1, file);
         pcapdata = NULL;
         rtpMsgCount++;
      }
      else if(candata)
      {
         logPacket.type = PACKET_TYPE_CAN;
         logPacket.sec = cants.tv_sec;
         logPacket.usec = cants.tv_usec;
         logPacket.len = cansize;

         fwrite(&logPacket, sizeof(logPacket), 1, file);
         fwrite(candata, cansize, 1, file);
         candata = NULL;
         canMsgCount++;
      }

      if((LONG_MAX == cants.tv_sec)&&(LONG_MAX == pcapts.tv_sec))
      {
         printf("Processed %i CAN and %i RTP packets.\n", canMsgCount, rtpMsgCount);
         break;
      }
   }

   canReaderClose(&canFp);
   pcapReaderClose(&pcapFp);
   fclose(file);
   return EXIT_SUCCESS;
}
