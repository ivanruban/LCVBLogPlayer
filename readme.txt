Simulation suite for testing of LVCB platform by replaying of previously collected CAN and PCAP(which contains RTP 
video stream) logs. It depends on libpcap(http://www.tcpdump.org/) and 
PEAK-System LINUX libpcan(http://www.peak-system.com/fileadmin/media/linux/index.htm).
 
It contains two components as a separate executable: logparser and logplayer.
   
logparser

Parses two given packets log files: PCAP and CAN. Based on this file the internal representation of
events is constructed in the format described at eventlog.h. Packets of CAN and PCAP logs are sorted in time
order.

logplayer

Implements limited TRSP server(@see https://en.wikipedia.org/wiki/Real_Time_Streaming_Protocol) trying to
simulate Panasonic wv-sp105 IP camera behavior.
It waits for a 'PLAY' request from a client and starts to playback the given RTP/CAN messages log.
Can and networks configuration items(like can device path, bitrate, network port/address to bind, etc)
are read from the cmd line arguments.

Installation
1. Install libpcap(http://www.tcpdump.org/) and PEAK-System LINUX libpcan(http://www.peak-system.com/fileadmin/media/linux/index.htm).
2. Run make

