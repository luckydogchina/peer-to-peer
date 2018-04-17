/*
 *这是一个用于socket通讯的基础库；
 *@author：wangzhipengtest@163.com
 *@date：04/15/2018
 **/

#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <pthread.h>

typedef void* (*RECV_CALLBACK_TCP)(void* p, const char* message, unsigned int len, int& s);
typedef void* (*RECV_CALLBACK_UDP)(void* p, const char* message, unsigned int len, int& s, sockaddr_in addr);
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
	
enum CALLBACKTYPE{
	READ = 0,	//recive messages 
	WRITE,	//send messages
	EXCEPTION,	//deal with expection 
	TIMEOUT,// deal with timeout
	CLOSE, //used when close socket
	EXIT //used when service exit
};


	/**
	 * @func:
	 * 	bind the socket to the local port
	 * @param:
	 * 	short: the port
	 * @return:
	 * 	int : Success 0, errno get the error code
	 * */
	int bindAddress(const char *ip, short port);

	/*@func:
	 *	registry the callback functions that used to analyse message. 
	 *@param:
	 *	callback: the point of call back functions;
	 *	param: the param used by call back function;
	 *	type: the type of the registried call back;
	 *@return:
	 *	null
	 **/
	void registryCallBackMethod(void* callback, void* param, CALLBACKTYPE type);
	
	/*@func :
	 *	get the method matched type;
	 *@param:
	 *	type: the method type;
	 *@return:
	 *	the point of method, maybe null;
	 **/
	void* getCallBackMethod(CALLBACKTYPE type);
protected:
	//the socket to listen connection
	int mSocket;
	
	//protect the call back method list
	pthread_mutex_t	mReceiveCallBackLock;
	
	pthread_mutexattr_t mReceiveCallBackLockMutex;	
	
	// call back method list;
	void* mReceiveCallBackList[6];
	
	// call back parament list;
	void* mParamentsList[6];
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
	int sendMessage(const char* ip, short port,const char* message, unsigned int len);

	int sendMessage(sockaddr_in addr, const char* message, unsigned int len);
	/**
	 * @func:
	 * 	recv the message from the ip:port
	 * @param:
	 * 	ip, port, message, the length of message bytes
	 * @return:
	 * 	Success: the number of recv message bytes, Failure -1
	 * */	
	int receiveMessage(const char* ip, short port, char* message, unsigned int ien);
	
	/**
	 * @func: 
	 * 	start a recv udp message thread
	 * @param: 
	 * 	void
	 * @return: 
	 * 	int: Success 0, errno get the error code
	 * */
	int start();
	
private:
	//the flag of the server state;
	bool mIsRunning;
	
	//the  server thread id;
	pthread_t mReceiveProcessId;
	
	//the server method;
	static void* mReceiveProcess(void* p);
};

class CTcp: public CBase
{
public:
	CTcp();
	~CTcp();
	/*@function: 
	 *	To create a connection to a dest address;
	 *@param:
	 *	ip: dest ip
	 *	port: dest port
	 *	sec: connect max time out;
	 *return:
	 *	
	 **/
	int connect(const char* ip, short port, unsigned int sec);
	
	/*@function:
	 *	start a tcp client;
	 *@param:
	 *	null
	 *@return
	 *	
	 **/
	int startClient();
	
	/*@function:
	 *	start a tcp server;
	 *@param:
	 *	null
	 *@retrun:
	 *	
	 **/
	int startServer();
	
	/*@function: 
	 *	send messages to socket;
	 *@param:
	 *	message: char type 
	 *	length: the length of message;
	 *@return:
	 *	
	 **/
	int sendMessage(const char* message, unsigned int length);
private:
	//the flag of the server state;
	bool mIsRunning;
	
	//the thread method to client
	static void* mReceiveProcessForClient(void* p);
	
	// the thread method to server
	static void* mReceiveProcessForServer(void* p);

	//the  client thread id;
	pthread_t mReceiveClientProcessId;
	
	//the  server thread id;
	pthread_t mRecerveServcerProcessId;
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
