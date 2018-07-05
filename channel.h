#include "socket_p.h"
#include "protobuf/peer-to-peer.pb.h"

/***
 * @func: start the server peer
 * @param: void
 * @return: int
 * 	Success 0
 * */
int server_start();

/***
 * @func: stop the server peer
 * @param: void
 * @return: void
 * */
void server_stop();

/***
 * @func: list the online peer
 * @param: void
 * @return: int
 * 	Success 0
 **/
int peer_listonline();

/***
 * @func: sent the hello msg
 * @param: void
 * @return: int
 * 	Success 0
 * */
int peer_hello();

/***
* @func: connect to other peers;
* @param: 
		peerIdentity: the dest peer Id;
* @return: int
* 	Success 0, False -1
* */
int peer_connect(const char* peerIdentity);

/***
* @func: start the peer
* @param: void
* @return: int
* 	Success 0
* */
int peer_start();

/***
* @func: init the peer
* @param: 
	peerIdentity: the peer's id;
	server_address: the IP of server;
	bind_tcp_port: the port of hole;
* @return: int
* 	Success 0
* */
void peer_init(const char* peerIdentity, const char* server_address, short bind_tcp_port);


template<typename T>
unsigned int serializeToArray_(char* &c, T t)
{
	unsigned int u = t.ByteSize();
	c = new char[u + 1];
	memset(c, 0, u + 1);
	t.SerializeToArray(c, u);
	
	return u;
}




