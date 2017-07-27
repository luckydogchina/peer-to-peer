#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <pthread.h>

typedef void* (*RECV_CALLBACK)(void* p, const char* message, unsigned int len, int& s);

class CBase
{
public:
	CBase();
	~CBase();
	
	/**
	 * @func:
	 * 	bind the socket on the local port
	 * @param:
	 * 	short: the port
	 * @return:
	 * 	int : Success 0, errno get the error code
	 * */
	int bindaddr(short port);

	void registercb(RECV_CALLBACK cb, void* p);
protected:
	int socket_m;

	
	RECV_CALLBACK recv_cb;

	void* pcb;
};

class CUdp:public CBase
{
public:
	CUdp();
	~CUdp();

	/**
	 * @func:
	 * 	send the message to the ip:port
	 * @param:
	 * 	ip, port , message type char
	 * @return:
	 * 	Success: the number of send message bytes, Failure -1
	 * */
	int sendmessage(const char* ip, short port,const char* message);
	/**
	 * @func:
	 * 	recv the message from the ip:port
	 * @param:
	 * 	ip, port, message, the length of message bytes
	 * @return:
	 * 	Success: the number of recv message bytes, Failure -1
	 * */	
	int recvmessage(const char* ip, short port, char* message, unsigned int ien);
	
	/**
	 * @func: 
	 * 	start a recv udp message thread
	 * @param: 
	 * 	void
	 * @return: 
	 * 	int: Success 0, errno get the error code
	 * */
	int recv();
	
private:
	bool isrun;

	pthread_t recv_proc_id;

	static void* recv_proc(void* p);
};

class CTcp: public CBase
{
public:
	CTcp();
	~CTcp();

	int connect(const char* ip, short port, unsigned int sec);
	int recv_client();
	int recv_server();
	int send(const char* message, unsigned int len);
private:
	bool isrun;

	static void* recv_proc_client(void* p);

	static void* recv_proc_server(void* p);

	pthread_t recv_client_id;
	
	pthread_t recv_servcer_id;
};

/**
 * @func: change the socket block attribute
 *
 * @param:
 * 	bool: true is no_block, false is block
 *
 * @return:
 * 	bool: Success or failure
 * */
bool noblock(int& s, bool is_no);

#define UDP_PORT 7395

#define TCP_PORT 7394
