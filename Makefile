GCC   = "g++"

INCLUDE = -I"."

PARSER_LIBRARY = -lpcap
SERVER_LIBRARY = -pthread

FLAGS = -Wall -Os

all: logplayer logparser logcmp

logplayer : logplayer.cpp dumpplayer.cpp rtpSender.cpp canSender.cpp
	$(GCC) -o logplayer logplayer.cpp dumpplayer.cpp rtpSender.cpp  canSender.cpp  $(FLAGS) $(INCLUDE) $(SERVER_LIBRARY)

logparser : logparser.cpp
	$(GCC) -o logparser logparser.cpp  $(FLAGS) $(INCLUDE) $(PARSER_LIBRARY)

logcmp : logcmp.cpp
	$(GCC) -o logcmp logcmp.cpp $(FLAGS) $(INCLUDE)


clean:
	rm -rf logplayer logparser logcmp
