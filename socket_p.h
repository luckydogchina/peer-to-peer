#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <pthread.h>

typedef void* (*RECV_CALLBACK_UDP)(void* p, const char* message, unsigned int len, int& s, sockaddr_in addr);
typedef void* (*RECV_CALLBACK_TCP)(void* p, const char* message, unsigned int len, int& s);
typedef void* (*WRITE_CALLBACK)(void* p, int &s);
typedef WRITE_CALLBACK EXCEPTION_CALLBACK;
typedef WRITE_CALLBACK EXIT_CALLBACK;
typedef WRITE_CALLBACK TIMEOUT_CALLBACK;
typedef WRITE_CALLBACK CLOSE_CALLBACK;

class CBase
{
public:
	CBase();
	~CBase();
	
enum cbtype{
	READ = 0,
	WRITE,
	EXCEPTION,
	TIMEOUT,
	CLOSE,
	EXIT
};


	/**
	 * @func:
	 * 	bind the socket on the local port
	 * @param:
	 * 	short: the port
	 * @return:
	 * 	int : Success 0, errno get the error code
	 * */
	int bindaddr(const char *ip, short port);

	void registercb(void* cb, void* p, cbtype t);

	void* getcb(cbtype t);
protected:
	int socket_m;
	
	pthread_mutex_t	recv_cb_lock;
	
	pthread_mutexattr_t recv_cb_lock_mt;	

	void* recv_cb[6];

	void* pcb[6];
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
	int sendmessage(const char* ip, short port,const char* message, unsigned int len);

	int sendmessage(sockaddr_in addr, const char* message, unsigned int len);
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

#endif
