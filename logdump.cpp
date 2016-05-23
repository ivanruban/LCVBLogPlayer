/**
 * Debug utility to test logplayer performance. It reads data from the given
 * CAN device and UDP port and stores it in the logplayer format. Later this data
 * can be compared with the original log with the logcmp utility to verify the
 * player's performance.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <sys/time.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "eventlog.h"

int openCAN(const char *path)
{
   int canSocket;
   if((canSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
   {
      fprintf(stderr,"socket(PF_CAN, SOCK_RAW, CAN_RAW) failed(%s)\n", strerror(errno));
      return -1;
   }

   struct sockaddr_can addr;
   struct ifreq ifr;
   strcpy(ifr.ifr_name, path);
   int err = ioctl(canSocket, SIOCGIFINDEX, &ifr);
   if(-1 == err)
   {
      fprintf(stderr,"ioctl(SIOCGIFINDEX) failed(%s)\n", strerror(errno));
      return -1;
   }
   addr.can_ifindex = ifr.ifr_ifindex;
   addr.can_family  = AF_CAN;
   if(bind(canSocket, (struct sockaddr *)&addr, sizeof(addr)) < 0)
   {
      fprintf(stderr,"bind(AF_CAN) failed(%s)\n", strerror(errno));
      return -1;
   }

   return canSocket;
}

int main(int argc, char **argv)
{
   if(argc != 4)
   {
      printf("Usage: %s rtp_port can_device dump.bin\n", argv[0]);
      printf("Like: %s 554 can0 dump.bin\n", argv[0]);
      return EXIT_FAILURE;
   }
   int port = atoi(argv[1]);
   const char *canName = argv[2];
   const char *dumpName = argv[3];

   FILE *dump;
   dump = fopen(dumpName, "w+");
   if (dump == NULL)
   {
      fprintf(stderr, "Unable to open the file %s(%s)\n", dumpName, strerror(errno));
      return EXIT_FAILURE;
   }

   eventLogHeader outHeader = { {'E','L','O','G'}, 1};
   fwrite(&outHeader, sizeof(eventLogHeader), 1, dump);

   int canSocket = openCAN(canName);
   if(-1 == canSocket)
   {
      fprintf(stderr, "Unable to CAN(%s) device(%s)\n",canName, strerror(errno));
      return EXIT_FAILURE;
   }

   struct sockaddr_in si_me;
   int udpSocket;
   if ((udpSocket=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
   {
      fprintf(stderr, "socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) failed(%s)\n", strerror(errno));
      return EXIT_FAILURE;
   }

   memset((char *) &si_me, 0, sizeof(si_me));

   si_me.sin_family = AF_INET;
   si_me.sin_port = htons(port);
   si_me.sin_addr.s_addr = htonl(INADDR_ANY);

   if( bind(udpSocket, (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
   {
      fprintf(stderr, "bind(AF_INET) failed(%s)\n", strerror(errno));
      return EXIT_FAILURE;
   }

   struct timespec tp;
   fd_set rfds;
   char buf[2000];
   while(1)
   {
      FD_ZERO(&rfds);
      FD_SET(udpSocket, &rfds);
      FD_SET(canSocket, &rfds);

      //nfds is the highest-numbered file descriptor in any of the three sets, plus 1.
      int err = select(udpSocket+1, &rfds, NULL, NULL, NULL);
      if (-1 == err)
      {
         fprintf(stderr, "select() failed(%s)\n", strerror(errno));
         return EXIT_FAILURE;
      }
      clock_gettime(CLOCK_MONOTONIC, &tp);

      if(FD_ISSET(udpSocket, &rfds))
      {
         err = recv(udpSocket, buf, sizeof(buf), 0);
         if(-1 == err)
         {
            fprintf(stderr, "UDP: recv() failed(%s)\n", strerror(errno));
            break;
         }
         eventLogPacket logPacket = {(uint64_t)tp.tv_sec, (uint64_t)tp.tv_nsec/1000, PACKET_TYPE_RTP, (uint16_t)err};
         fwrite(&logPacket, sizeof(logPacket), 1, dump);
         fwrite(buf, err, 1, dump);
      }
      if(FD_ISSET(canSocket, &rfds))
      {
         can_frame frame;
         err = recv(canSocket, &frame, sizeof(can_frame), 0);
         if(-1 == err)
         {
            fprintf(stderr, "CAN: recv() failed(%s)\n", strerror(errno));
            break;
         }
         eventLogPacket logPacket = {(uint64_t)tp.tv_sec, (uint64_t)tp.tv_nsec/1000, PACKET_TYPE_CAN, (uint16_t)sizeof(canEvent)};
         fwrite(&logPacket, sizeof(logPacket), 1, dump);

         canEvent canMsg;
         canMsg.id = frame.can_id;
         canMsg.len = frame.can_dlc;
         memcpy(canMsg.data, frame.data, sizeof(canMsg.data));

         fwrite(&canMsg, sizeof(canEvent), 1, dump);
      }
   }

   close(udpSocket);
   close(canSocket);
   fclose(dump);
   return EXIT_SUCCESS;
}
