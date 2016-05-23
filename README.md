# LCVBLogPlayer

Simulation suite for testing of LCVB platform by replaying of previously collected CAN and PCAP(which contains RTP 
video stream) logs. It depends on libpcap(http://www.tcpdump.org/). SocketCAN(https://en.wikipedia.org/wiki/SocketCAN)
interface is used for CAN message handling.

Project contains the following components as a separate executable:

   logparser
   
   Parses two given packets log files: PCAP and CAN. Based on this file the internal representation of
   events is constructed in the format described at eventlog.h. Packets of CAN and PCAP logs are sorted in time
   order.

   logplayer
   
   Implements limited RTSP server(@see https://en.wikipedia.org/wiki/Real_Time_Streaming_Protocol) trying to
   simulate Panasonic wv-sp105 IP camera behavior.
   It waits for a 'PLAY' request from a client and starts to playback the given RTP/CAN messages log.
   Can and networks configuration items(like can device path, network port/address to bind, etc)
   are read from the cmd line arguments.

   logdumper
   
   Debug utility to test logplayer performance. It reads data from the given
   CAN device and UDP port and stores it in the logplayer format. Later this data
   can be compared with the original log with the logcmp utility to verify the
   player performance.

   logcmp
   
   Debug utility to compare two RTP/CAN messages log in player's format(@see eventlog.h). It tries to find 
   an message occurred in the first file in the second one and outputs its timestamp(in ms) and message type in 
   CSV format.
      <type>; <timestamp in first file in ms>; <timestamp in second file in ms>;   
   Like:
   RTP; 1458726003330; 1310797554;
   Output may processed with Excel to compute player timing performance.

Performance verifying methodology

1. Create a virtual can driver
   modprobe can
   modprobe can_raw
   modprobe vcan
   sudo ip link add dev vcan0 type vcan
   sudo ip link set up vcan0
   ip link show vcan0

2. Run logdumper pointing a port, can device and output log file 
   ./logdump 8888 vcan0 dump.bin
   
3. Run logplayer pointing a port, address, can device and input log file
   ./logplayer -r -f -d vcan0 -p 8888 -i 127.0.0.1 23022016.bin

4. Stop player/dumper once end of input file is reached(logplayer prints: 'rewind dump log!')
   ./logplayer -r -f -d vcan0 -p 8888 -i 127.0.0.1 23022016.bin
   Stream file 23022016.bin to 127.0.0.1:8888
   rewind dump log!

5. Run logcmp pointing logplayer's log as a first argument and the logdump output as the second one
   ./logcmp 23022016.bin dump.bin > 23022016.csv
   logcmp ouputs a line for each message in the first dump in the following format: 
         <type>; <timestamp in first file in ms>; <timestamp in second file in ms>;   
   Like:
         RTP; 1458726003330; 1310797554;
         ...

6. Open the logcmp output file in Excel and compute the required statistic.
   For example to compute a difference of timeslots between original and
   recreated log's messages the following formula may be used:  ABS(ABS(C2-C1) - ABS(B2-B1)).

Installation
   1. Install libpcap(http://www.tcpdump.org/)
   2. Run make
