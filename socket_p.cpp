#include"socket_p.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <list>
#include <algorithm>

CBase::CBase()
{
	memset(this->mReceiveCallBackList, 0, sizeof(this->mReceiveCallBackList));
	memset(this->mParamentsList, 0, sizeof(this->mParamentsList));
	pthread_mutexattr_init(&this->mReceiveCallBackLockMutex);
	pthread_mutexattr_settype(&this->mReceiveCallBackLockMutex, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&this->mReceiveCallBackLock, &this->mReceiveCallBackLockMutex);	
}

CBase::~CBase()
{
	pthread_mutex_destroy(&this->mReceiveCallBackLock);
	pthread_mutexattr_destroy(&this->mReceiveCallBackLockMutex);
}


int CBase::bindAddress(const char *ip, short port)
{
	struct sockaddr_in address;
	address.sin_family=AF_INET;
	if(ip)
	{
		address.sin_addr.s_addr=inet_addr(ip);
	}
	else
	{
		address.sin_addr.s_addr=htonl(INADDR_ANY);
	}
	address.sin_port=htons(port);

	return bind(this->mSocket, (sockaddr*)&address, sizeof(sockaddr));
}

void CBase::registryCallBackMethod(void* callback, void* param, CALLBACKTYPE type)
{
	if ((int)type < (int)sizeof(this->mReceiveCallBackList))
	{
		pthread_mutex_lock(&this->mReceiveCallBackLock);	
		this->mReceiveCallBackList[type] = callback;
		this->mParamentsList[type] = param;
		pthread_mutex_unlock(&this->mReceiveCallBackLock);
	}
	return;
}

void* CBase::getCallBackMethod(CALLBACKTYPE type)
{
	void* callBack = NULL;
	
	if((int)type < (int)sizeof(this->mReceiveCallBackList))
	{
		pthread_mutex_lock(&this->mReceiveCallBackLock);
		callBack = this->mReceiveCallBackList[type];
		pthread_mutex_unlock(&this->mReceiveCallBackLock);
	}

	return callBack;
}

CUdp::CUdp()
{
	//unsigned int ok = 1;	
	this->mSocket = socket(AF_INET, SOCK_DGRAM, 0);
//	setsockopt( this->mSocket, SOL_SOCKET, SO_REUSEADDR, ( const char* )&ok, sizeof(ok));
	memset(&this->mReceiveProcessId, 0, sizeof(pthread_t));
	this->mIsRunning = true;
}

CUdp::~CUdp()
{
	this->mIsRunning = false;
	close(this->mSocket);
	printf("exit\n");
	if(this->mReceiveProcessId !=  0)
		pthread_join(this->mReceiveProcessId, NULL);
}

int CUdp::start()
{
	int result  = 0;
	
	//set no_block on the socket.
	if(!noblock(this->mSocket,true))
		return -1;

	//bind the socket on `7395` port in local ip.
	result = bindAddress(NULL, UDP_PORT);
	if (0 != result)
		return result;

	//start the recv thread.
	result = pthread_create(&this->mReceiveProcessId, NULL, this->mReceiveProcess, (void*) this);
	return result;
}


void* CUdp::mReceiveProcess(void* param)
{
	int result;
	fd_set r_set, e_set, o_set;
	
	CUdp* CCUdp = (CUdp*)param;
	char buffer[1024] = {0};
	
	struct sockaddr_in address;
	unsigned int sizeAddress;
	struct timeval timeOut;

	void* callBack = NULL;

	memset(&timeOut, 0, sizeof(timeOut));
	timeOut.tv_sec=3;


	FD_ZERO(&o_set);
	FD_SET(CCUdp->mSocket, &o_set);

	while(CCUdp->mIsRunning)
	{
		r_set = e_set = o_set;	
		timeOut.tv_sec = 3;
		result = select(CCUdp->mSocket + 1, &r_set, NULL, &e_set, &timeOut);
		
		switch (result)
		{
		case 0:
			if(NULL != (callBack = CCUdp->getCallBackMethod(CBase::TIMEOUT)))
				((TIMEOUT_CALLBACK)callBack)(CCUdp->mParamentsList[TIMEOUT], CCUdp->mSocket);
			continue;
		case -1:
			goto end;
		default:
			//the expection is throw;
			if(FD_ISSET(CCUdp->mSocket, &e_set))
			{
				if(NULL != (callBack = CCUdp->getCallBackMethod(CBase::EXCEPTION)))
				{
					((EXCEPTION_CALLBACK)callBack)(CCUdp->mParamentsList[CBase::EXCEPTION], CCUdp->mSocket);
				}

				goto end;
			}
			
			//the read buffer have data;
			if(FD_ISSET(CCUdp->mSocket, &r_set))
			{
				memset(&address,0, sizeof(address));
				sizeAddress = sizeof(address);
				result = recvfrom(CCUdp->mSocket, buffer, sizeof(buffer),0,
						   	(sockaddr*)&address, &sizeAddress);
			
				printf("from: %s %d | message %s \n", inet_ntoa(address.sin_addr),
						ntohs(address.sin_port), buffer);

				if(result > 0)
				{
					if(NULL !=(callBack = CCUdp->getCallBackMethod( CBase::READ)))
					{
				
						((RECV_CALLBACK_UDP)callBack)(CCUdp->mParamentsList[CBase::READ], buffer, result, CCUdp->mSocket, address);
					}
				}
			
				if(result < 0)
				{
					goto end;
				}

				memset(buffer, 0, sizeof(buffer));
			}
		}
	}								
end:
	if(NULL !=(callBack = CCUdp->getCallBackMethod(EXIT)))
	{
		((EXIT_CALLBACK)callBack)(CCUdp->mParamentsList[CBase::EXIT], CCUdp->mSocket);
	}
	
	return NULL;
}

int CUdp::sendMessage(const char* ip, short port, const char* message, unsigned int len)
{
	struct sockaddr_in address;
	if(!ip)
	{
		return -1;
	}
	
	memset(&address, 0, sizeof(address));
	address.sin_family=AF_INET;
	address.sin_port=htons(port);
	address.sin_addr.s_addr=inet_addr(ip);
	
	return sendMessage(address, message, len);
}

int CUdp::sendMessage(sockaddr_in addr, const char* message, unsigned int len)
{
	return sendto(this->mSocket, message, len ,0,(struct sockaddr*)&addr, sizeof(sockaddr_in));
}

int CUdp::receiveMessage(const char* ip, short port, char* message, unsigned int len)
{
	struct sockaddr_in address;
	unsigned int recvLength = sizeof(sockaddr_in);
	
	memset(&address, 0, sizeof(address));
	if(ip != NULL)
	{
		address.sin_addr.s_addr=inet_addr(ip);
		address.sin_family=AF_INET;
		address.sin_port=htons(port);
	}

	return recvfrom(this->mSocket, message, len, 0, (struct sockaddr*)&address, &recvLength);
}

bool noblock(int& s, bool is_no)
{
	int flags, result;
	flags  = fcntl(s, F_GETFL, 0);
	if(is_no)
	{
		result = fcntl(s, F_SETFL, flags | O_NONBLOCK);
	}
	else
	{
		result = fcntl(s, F_SETFL, flags & ~O_NONBLOCK);
	}

	return !result;
}

CTcp::CTcp()
{
	unsigned int ok = 1;
	linger ln;
	
	memset(&ln, 0, sizeof(ln));
	//l_onoff = 1 l_linger=0时强制关闭，缓存区的消息直接清除，socket关闭时不会进入TIME_WAIT状态;
	//现在的状态是直接关闭，缓存区的消息尽量发送到接收方
	ln.l_onoff = 0;
	//ln.l_linger = 0;

	this->mReceiveClientProcessId = 0;
	this->mReceiveServerProcessId = 0;
	this->mIsRunning = true;
	this->mSocket = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt( this->mSocket, SOL_SOCKET, SO_REUSEADDR, (const void*)&ok, sizeof(ok));
	setsockopt(this->mSocket, SOL_SOCKET, SO_LINGER, (const void*)&ln, sizeof(ln));
}

CTcp::~CTcp()
{
	this->mIsRunning = false;
	close(this->mSocket);

	if (this->mReceiveClientProcessId){
		pthread_join(this->mReceiveClientProcessId, NULL);
	}

	if (this->mReceiveServerProcessId){
		pthread_join(this->mReceiveServerProcessId, NULL);
	}
}

int CTcp::connect(const char* ip, short port, unsigned int sec)
{
	fd_set w_set;	
	timeval timeout;
	sockaddr_in address;

	int result, error = 0;
	unsigned int len=sizeof(error);

	if (NULL == ip){
		return -1;
	}

	if (!noblock(this->mSocket, true)){
		return	-1;
	}
		
	
	memset(&address, 0, sizeof(address));
	address.sin_addr.s_addr = inet_addr(ip);
	address.sin_port= htons(port);
	address.sin_family=AF_INET;
	
	printf("connect .....\n");	
	result = ::connect(this->mSocket,(sockaddr*)&address, sizeof(address));
	if (!result)
	{
		printf("connect is success!\n");
		noblock(this->mSocket, false);	
		return 0;
	}

	if( errno != EINPROGRESS)
	{
		printf("connect is failure: %s\n", strerror(errno));

		noblock(this->mSocket, false);
		return -1;
	}

	FD_ZERO(&w_set);
	FD_SET(this->mSocket, &w_set);

	memset(&timeout, 0, sizeof(timeout));
	timeout.tv_sec= sec;	
	
 	result = select(this->mSocket + 1, NULL, &w_set, NULL, &timeout);
	switch(result)
	{
	case 0:
		printf("connect is time out\n");
		noblock(this->mSocket, false);
		return -1;
	case -1:
		printf("select is error: %s\n", strerror(errno));
		
		noblock(this->mSocket, false);
		return -1;
	default :
		if(FD_ISSET(this->mSocket, &w_set))
		{
			if(0 <= getsockopt(this->mSocket,SOL_SOCKET,SO_ERROR,(void*)&error,&len)
					&& error)
			{
					         
				printf("getsockopt error:%s\n",strerror(error));
				return -1;
			}
			
			printf("connect is success!\n");	
			noblock(this->mSocket, false);
			return 0;
		}
	}

	noblock(this->mSocket, false);
	return -1;
}

int CTcp::startClient()
{
	int result  = 0;
	
	//set no_block on the socket.
	if(!noblock(this->mSocket,true))
		return -1;

	result = pthread_create(&this->mReceiveClientProcessId,NULL, this->mReceiveProcessForClient,(void*)this);	

	return result;
}

void* CTcp::mReceiveProcessForClient(void* param)
{
	int result;
	fd_set r_set, e_set, o_set;
	
	CTcp* CCTcp = (CTcp*)param;
	char buffer[1024] = {0};
	
	struct sockaddr_in address;
	unsigned int sizeAddress;
	struct timeval timeOut;
	
	memset(&timeOut, 0, sizeof(timeOut));
	timeOut.tv_sec = 3;
	
	FD_ZERO(&o_set); 
	FD_SET(CCTcp->mSocket, &o_set);
	
	void* callBack = NULL;

	while (CCTcp->mIsRunning)
	{
		r_set = e_set = o_set;		
		timeOut.tv_sec = 3;
		result = select(CCTcp->mSocket + 1, &r_set, NULL, &e_set, &timeOut);
		
		switch(result)
		{
		case 0:
			if (NULL != (callBack = CCTcp->getCallBackMethod(CBase::TIMEOUT))){
				((TIMEOUT_CALLBACK)(callBack))(CCTcp->mParamentsList[CBase::TIMEOUT], CCTcp->mSocket);
			}
			continue;
		case -1:
			goto end;
		default:
			//the expection is throw;
			if (FD_ISSET(CCTcp->mSocket, &e_set))
			{
				if (NULL != (callBack = CCTcp->getCallBackMethod(CBase::EXCEPTION))){
					((EXCEPTION_CALLBACK)callBack)(CCTcp->mParamentsList[CBase::EXCEPTION], CCTcp->mSocket);
				}
				goto end;
			}
			
			//the read buffer have data;
			if (FD_ISSET(CCTcp->mSocket, &r_set)){
				memset(&address,0, sizeof(address));
				sizeAddress = sizeof(address);
				result = recv(CCTcp->mSocket, buffer, sizeof(buffer),0);
			
				printf("server back message %s \n", buffer );
				if(result > 0){
					if(NULL !=(callBack = CCTcp->getCallBackMethod(CBase::READ))){
						((RECV_CALLBACK_TCP)callBack)(CCTcp->mParamentsList[CBase::READ], buffer, result, CCTcp->mSocket);
					}
				}
			
				if(result <= 0){
					printf("the recv tcp is error: %s \n", strerror(errno));
					goto end;
				}

				memset(buffer, 0, sizeof(buffer));
			}
		}
		
	}

end:
	if (NULL != (callBack = CCTcp->getCallBackMethod(CBase::EXIT))){
		((EXIT_CALLBACK)callBack)(CCTcp->mParamentsList[EXIT], CCTcp->mSocket);
	}
	return NULL;
}

int CTcp::startServer()
{
	if(!noblock(this->mSocket, true))
		return -1;

	if(listen(this->mSocket, 5))
	{
	
		printf("listen is error: %s\n",strerror(errno) );
		return -1;
	}

	return  pthread_create(&this->mReceiveServerProcessId, NULL, this->mReceiveProcessForServer, (void*)this);	
}
      

void* CTcp::mReceiveProcessForServer(void* param)
{
	int result;
	CTcp* CCTcp = (CTcp*)param;
	fd_set r_set, e_set, o_set;
	
	timeval timeout;
	std::list<int> s_list; 

	sockaddr_in address;
	unsigned int addr_size;

	std::list<int>::iterator iter, iter1;
	int maxFd = CCTcp->mSocket;
	bool change = false;

	FD_ZERO(&o_set);
	FD_SET(CCTcp->mSocket, &o_set);

	void* callBack;
	while(CCTcp->mIsRunning)
	{
		r_set = e_set = o_set;	
		timeout.tv_sec = 3;
			
		result = select(maxFd + 1, &r_set, NULL, &e_set, &timeout);
		if (change){
			maxFd = s_list.size() != 0 ? *(--s_list.end()) : CCTcp->mSocket;
		}
			
		printf("the max is %d\n", maxFd);

		switch(result)
		{
		case 0:
			printf("the server time is out\n");
			if (NULL != (callBack = CCTcp->getCallBackMethod(CBase::TIMEOUT))){
				((TIMEOUT_CALLBACK)callBack)(CCTcp->mParamentsList[CBase::TIMEOUT], CCTcp->mSocket);
			}
			continue;
		case -1:
			printf("the server select is wrong %s \n", strerror(errno));
			goto s_end;
		default:
			for(iter = s_list.begin(); iter != s_list.end(); ){
				printf("<<<<<<<<<<<<>>>>>>>>>>>>>>: %d\n", *iter);
				if(FD_ISSET(*iter, &r_set)){
					char buffer[1024] = {0};
					int recv_len = ::recv(*iter,  buffer, 1024, 0);	

					printf("recv the message %d %s \n", recv_len, buffer);
					if(recv_len <= 0){
						printf("close the socket %d\n", *iter);
						if (NULL != (callBack = CCTcp->getCallBackMethod(CBase::CLOSE))){
							((CLOSE_CALLBACK)callBack)(CCTcp->mParamentsList[CBase::CLOSE], *iter);
						}
						
						//关闭socket
						close(*iter);	
						FD_CLR(*iter, &o_set);

						//判断最大监听上限是否改变
						change = (maxFd == *iter);
						iter = s_list.erase(iter);
						continue;
					}else if(NULL != (callBack = CCTcp->getCallBackMethod(CBase::READ))){
						((RECV_CALLBACK_TCP)callBack)(CCTcp->mParamentsList[CBase::READ], buffer, recv_len, *iter);
					}
				}else if(FD_ISSET(*iter, &e_set)){
					//移除发生异常的socket
					printf("close the socket %d\n", *iter);
									
					if(NULL != (callBack = CCTcp->getCallBackMethod(CBase::EXCEPTION)))	
							((EXCEPTION_CALLBACK)callBack)(CCTcp->mParamentsList[CBase::EXCEPTION],*iter);
		
					close(*iter);	
					FD_CLR(*iter,&o_set);
					change = (maxFd == *iter);
					iter = s_list.erase(iter);
					continue;
				}

				iter++;
			}
		
			printf(">>>>>>>>>>>>>>>>>\n");	
			//处理新接入的socket连接
			if(FD_ISSET(CCTcp->mSocket, &r_set)){
				memset(&address, 0, sizeof(address));	
				addr_size = sizeof(address);
				result = accept(CCTcp->mSocket, (sockaddr*)&address, &addr_size);
				
				printf("accept the socket from : %s %d %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port), result);	
				s_list.push_back(result);
				FD_SET(result, &o_set);	

				//更新最大监听范围
				maxFd = (result > maxFd)?result:maxFd;
			}

			printf("<<<<<<<<<<<<<<<<<<<\n");
			if (FD_ISSET(CCTcp->mSocket, &e_set)){
				if (NULL != (callBack = CCTcp->getCallBackMethod(CBase::EXCEPTION))){
					((EXCEPTION_CALLBACK)callBack)(CCTcp->mParamentsList[CBase::EXCEPTION], CCTcp->mSocket);
				}
				goto s_end;
			}
		}	
	}

s_end:
	for (iter = s_list.begin(); iter != s_list.end(); iter++){
		close(*iter);
	}
	
	if (NULL != (callBack = CCTcp->getCallBackMethod(CBase::EXIT))){
		((EXIT_CALLBACK)callBack)(CCTcp->mParamentsList[CBase::EXIT], CCTcp->mSocket);
	}
		
	printf("exit the thread \n");
	return NULL;
}

int CTcp::sendMessage(const char* message, unsigned int len)
{
	if(NULL == message)
		return -1;

	return ::send(this->mSocket, message, len, 0);
}
