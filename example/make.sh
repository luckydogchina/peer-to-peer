#!/bin/sh
if [ $1 = 0 ]; then
	g++ -o channel_s channel_s.cpp ../socket_p.cpp ../channel.cpp ../protobuf/*.cc -lpthread -lprotobuf 
else
	g++ -o channel_c channel_c.cpp ../socket_p.cpp ../peer.cpp ../protobuf/*.cc -lpthread -lprotobuf 
fi
 
