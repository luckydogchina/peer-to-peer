#include "socket_p.h"
#include "protobuf/peer-to-peer.pb.h"

/***
 * @func: start the server peer
 * @param: void
 * @return: int
 * 	Success 0
 * */
int start();

/***
 * @func: stop the server peer
 * @param: void
 * @return: void
 * */
void stop();

/***
 * @func: list the online peer
 * @param: void
 * @return: int
 * 	Success 0
 **/
int listonline();

/***
 * @func: sent the hello msg
 * @param: void
 * @return: int
 * 	Success 0
 * */
int hello();

int connect(const char* peer);

void Init(const char* user_id, const char* server_id, short udp_port, short tcp_port, short bind_udp_port, short bind_tcp_port);


template<typename T>
unsigned int serializeToArray_(char* &c, T t)
{
	unsigned int u = t.ByteSize();
	c = new char[u + 1];
	memset(c, 0, u + 1);
	t.SerializeToArray(c, u);
	
	return u;
}



int peer_start();
