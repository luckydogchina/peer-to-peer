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
	memset(this->recv_cb, 0, sizeof(this->recv_cb));
	memset(this->pcb, 0, sizeof(this->pcb));
	pthread_mutexattr_init(&this->recv_cb_lock_mt);
	pthread_mutexattr_settype(&this->recv_cb_lock_mt, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&this->recv_cb_lock, &this->recv_cb_lock_mt);	
}

CBase::~CBase()
{
	pthread_mutex_destroy(&this->recv_cb_lock);
	pthread_mutexattr_destroy(&this->recv_cb_lock_mt);
}


int CBase::bindaddr(const char *ip, short port)
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

	return bind(this->socket_m, (sockaddr*)&address, sizeof(sockaddr));
}

void CBase::registercb(void* cb, void* p, cbtype t)
{
	if ((int)t < (int)sizeof(this->recv_cb))
	{
		pthread_mutex_lock(&this->recv_cb_lock);	
		this->recv_cb[t] = cb;
		this->pcb[t] = p;
		pthread_mutex_unlock(&this->recv_cb_lock);
	}
	return;
}

void* CBase::getcb(cbtype t)
{
	void* cb = NULL;
	
	if((int)t < (int)sizeof(this->recv_cb))
	{
		pthread_mutex_lock(&this->recv_cb_lock);
		cb = this->recv_cb[t];
		pthread_mutex_unlock(&this->recv_cb_lock);
	}

	return cb;
}

CUdp::CUdp()
{
	//unsigned int ok = 1;	
	this->socket_m = socket(AF_INET, SOCK_DGRAM, 0);
//	setsockopt( this->socket_m, SOL_SOCKET, SO_REUSEADDR, ( const char* )&ok, sizeof(ok));
	memset(&this->recv_proc_id, 0, sizeof(pthread_t));
	this->isrun = true;
}

CUdp::~CUdp()
{
	this->isrun = false;
	close(this->socket_m);
	printf("exit\n");
	if(this->recv_proc_id !=  0)
		pthread_join(this->recv_proc_id, NULL);
}

int CUdp::recv()
{
	int reslt  = 0;
	
	//set no_block on the socket.
	if(!noblock(this->socket_m,true))
		return -1;

	//bind the socket on `7395` port in local ip.
	reslt = bindaddr(NULL, UDP_PORT);
	if(0 != reslt) 
		return reslt;

	//start the recv thread.
	reslt =  pthread_create(&this->recv_proc_id, NULL, this->recv_proc, (void*) this);
	return reslt;
}


void* CUdp::recv_proc(void* p)
{
	int reslt;
	fd_set r_set, e_set, o_set;
	
	CUdp* U = (CUdp*)p;
	char buffer[1024] = {0};
	
	struct sockaddr_in address;
	unsigned int size_addr;
	struct timeval t;

	void* cb = NULL;

	memset(&t,0,sizeof(t));
	t.tv_sec=3;
	
	FD_ZERO(&o_set);
	FD_SET(U->socket_m,&o_set);

	while(U->isrun)
	{
		r_set = e_set = o_set;	
		t.tv_sec = 3;
		reslt = select(U->socket_m + 1, &r_set, NULL, &e_set, &t);
		
		switch(reslt)
		{
		case 0:
			if(NULL != (cb = U->getcb(CBase::TIMEOUT)))
				((TIMEOUT_CALLBACK)cb)(U->pcb[ TIMEOUT], U->socket_m);
			continue;
		case -1:
			goto end;
		default:
			//the expection is throw;
			if(FD_ISSET(U->socket_m, &e_set))
			{
				if(NULL != (cb = U->getcb(CBase::EXCEPTION)))
				{
					((EXCEPTION_CALLBACK)cb)(U->pcb[CBase::EXCEPTION], U->socket_m);
				}

				goto end;
			}
			
			//the read buffer have data;
			if(FD_ISSET(U->socket_m, &r_set))
			{
				memset(&address,0, sizeof(address));
				size_addr = sizeof(address);
				reslt = recvfrom(U->socket_m, buffer, sizeof(buffer),0,
						   	(sockaddr*)&address, &size_addr);
			
				printf("from: %s %d | message %s \n", inet_ntoa(address.sin_addr),
						ntohs(address.sin_port), buffer );

				if(reslt > 0)
				{
					if(NULL !=(cb = U->getcb( CBase::READ)))
					{
				
						((RECV_CALLBACK_UDP)cb)(U->pcb[CBase::READ], buffer, reslt, U->socket_m, address);
					}
				}
			
				if(reslt < 0)
				{
					goto end;
				}

				memset(buffer, 0, sizeof(buffer));
			}

		}
		
	}								
end:
	if(NULL !=(cb = U->getcb( EXIT)))
	{
		((EXIT_CALLBACK)cb)(U->pcb[CBase::EXIT], U->socket_m);
	}
	
	return NULL;
}

int CUdp::sendmessage(const char* ip, short port, const char* message, unsigned int len)
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
	
	return sendmessage(address, message, len);
}

int CUdp::sendmessage(sockaddr_in addr, const char* message, unsigned int len)
{
	return sendto(this->socket_m, message, len ,0,(struct sockaddr*)&addr, sizeof(sockaddr_in));
}

int CUdp::recvmessage(const char* ip, short port, char* message, unsigned int len)
{
	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	if(ip != NULL)
	{
		address.sin_addr.s_addr=inet_addr(ip);
		address.sin_family=AF_INET;
		address.sin_port=htons(port);
	}
	
	unsigned int recvlen = sizeof(sockaddr_in);

	int reslt = recvfrom(this->socket_m, message, len, 0, (struct sockaddr*)&address, &recvlen);
	return reslt;
}

bool noblock(int& s, bool is_no)
{
	int flags, reslt;
	flags  = fcntl(s, F_GETFL, 0);
	if(is_no)
	{
		reslt = fcntl(s, F_SETFL, flags | O_NONBLOCK);
	}
	else
	{
		reslt = fcntl(s, F_SETFL, flags & ~O_NONBLOCK);
	}

	return !reslt;
}

CTcp::CTcp()
{
	unsigned int ok = 1;
	this->recv_servcer_id = 0;
	this->recv_client_id = 0;
	this->isrun = true;
	this->socket_m = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt( this->socket_m, SOL_SOCKET, SO_REUSEADDR, (const void*)&ok, sizeof(ok));
}

CTcp::~CTcp()
{
	this->isrun = false;
	close(this->socket_m);

	if(this->recv_client_id)
		pthread_join(this->recv_client_id, NULL);

	if(this->recv_servcer_id)
		pthread_join(this->recv_servcer_id, NULL);
}

int CTcp::connect(const char* ip, short port, unsigned int sec)
{
	int reslt, error = 0;
	fd_set w_set;	
	timeval timeout;
	sockaddr_in address;
	
	unsigned int len=sizeof(error);

	if (!ip) 
		return -1;

	if(!noblock(this->socket_m, true))
		return	-1;
	
	memset(&address, 0, sizeof(address));
	address.sin_addr.s_addr = inet_addr(ip);
	address.sin_port= htons(port);
	address.sin_family=AF_INET;
	
	printf("connect .....\n");	
	reslt = ::connect(this->socket_m,(sockaddr*)&address, sizeof(address));
	if (!reslt)
	{
		printf("connect is success!\n");
		noblock(this->socket_m, false);	
		return 0;
	}

	if( errno != EINPROGRESS)
	{
		printf("connect is failure: %s\n", strerror(errno));

		noblock(this->socket_m, false);
		return -1;
	}

	FD_ZERO(&w_set);
	FD_SET(this->socket_m, &w_set);

	memset(&timeout, 0, sizeof(timeout));
	timeout.tv_sec= sec;	
	
 	reslt = select(this->socket_m + 1, NULL, &w_set, NULL, &timeout);
	switch(reslt)
	{
	case 0:
		printf("connect is time out\n");
		noblock(this->socket_m, false);
		return -1;
	case -1:
		printf("select is error: %s\n", strerror(errno));
		
		noblock(this->socket_m, false);
		return -1;
	default :
		if(FD_ISSET(this->socket_m, &w_set))
		{
			if(0 <= getsockopt(this->socket_m,SOL_SOCKET,SO_ERROR,(void*)&error,&len)
					&& error)
			{
					         
				printf("getsockopt error:%s\n",strerror(error));
				return -1;
			}
			
			printf("connect is success!\n");	
			noblock(this->socket_m, false);
			return 0;
		}
	}

	noblock(this->socket_m, false);
	return -1;
}

int CTcp::recv_client()
{
	int reslt  = 0;
	
	//set no_block on the socket.
	if(!noblock(this->socket_m,true))
		return -1;

	reslt = pthread_create(&this->recv_client_id,NULL, recv_proc_client,(void*)this);	

	return reslt;
}

void* CTcp::recv_proc_client(void* p)
{
	int reslt;
	fd_set r_set, e_set, o_set;
	
	CTcp* T = (CTcp*)p;
	char buffer[1024] = {0};
	
	struct sockaddr_in address;
	unsigned int size_addr;
	struct timeval t;
	
	memset(&t,0,sizeof(t));
	t.tv_sec=3;
	
	FD_ZERO(&o_set); 
	FD_SET(T->socket_m, &o_set);
	
	void* cb = NULL;

	while(T->isrun)
	{
		r_set = e_set = o_set;		
		t.tv_sec = 3;
		reslt = select(T->socket_m + 1, &r_set, NULL, &e_set, &t);
		
		switch(reslt)
		{
		case 0:
			if ( NULL != (cb = T->getcb(CBase::TIMEOUT)))
				((TIMEOUT_CALLBACK)(cb))(T->pcb[CBase::TIMEOUT], T->socket_m);
			continue;
		case -1:
			goto end;
		default:
			//the expection is throw;
			if(FD_ISSET(T->socket_m, &e_set))
			{
				if (NULL != (cb = T->getcb(CBase::EXCEPTION)))
					((EXCEPTION_CALLBACK)cb)(T->pcb[CBase::EXCEPTION], T->socket_m);
				goto end;
			}
			
			//the read buffer have data;
			if(FD_ISSET(T->socket_m, &r_set))
			{
				memset(&address,0, sizeof(address));
				size_addr = sizeof(address);
				reslt = recv(T->socket_m, buffer, sizeof(buffer),0);
			
				printf("server back message %s \n", buffer );

				if(reslt > 0)
				{
					if(NULL !=(cb = T->getcb(CBase::READ)))
					{
						((RECV_CALLBACK_TCP)cb)(T->pcb[CBase::READ], buffer, reslt, T->socket_m);
					}
				
				}
			
				if(reslt <= 0)
				{
					printf("the recv tcp is error: %s \n", strerror(errno));
					goto end;
				}

				memset(buffer, 0, sizeof(buffer));
			}

		}
		
	}								
end:
	
	if (NULL != (cb = T->getcb(CBase::EXIT)))
		((EXIT_CALLBACK)cb)(T->pcb[EXIT], T->socket_m);
	return NULL;
}

int CTcp::recv_server()
{
	if(!noblock(this->socket_m, true))
		return -1;

	if(listen(this->socket_m, 5))
	{
	
		printf("listen is error: %s\n",strerror(errno) );
		return -1;
	}

	return  pthread_create(&this->recv_servcer_id,NULL, recv_proc_server,(void*)this);	
}
      

void* CTcp::recv_proc_server(void* p)
{
	int reslt;
	CTcp* T = (CTcp*)p;
	fd_set r_set, e_set, o_set;
	
	timeval timeout;
	std::list<int> s_list; 

	sockaddr_in address;
	unsigned int addr_size;

	std::list<int>::iterator iter, iter1;
	int Max_Fd=T->socket_m;
	bool change= false;

	FD_ZERO(&o_set);
	FD_SET(T->socket_m, &o_set);

	void* cb;
	while(T->isrun)
	{
		r_set = e_set = o_set;	
		timeout.tv_sec = 3;
			
		reslt = select(Max_Fd+1, &r_set, NULL, &e_set, &timeout);
		if(change)
			Max_Fd = s_list.size()!=0 ? *(--s_list.end()):T->socket_m;

		printf("the max is %d\n", Max_Fd);

		switch(reslt)
		{
		case 0:
			printf("the server time is out\n");
			if(NULL != (cb = T->getcb(CBase::TIMEOUT)))
				((TIMEOUT_CALLBACK)cb)(T->pcb[CBase::TIMEOUT], T->socket_m);
			continue;
		case -1:
			printf("the server select is wrong %s \n", strerror(errno));
			goto s_end;
		default:
			for(iter = s_list.begin(); iter != s_list.end() ;)
			{
				printf("<<<<<<<<<<<<>>>>>>>>>>>>>>: %d\n", *iter);
				if(FD_ISSET(*iter, &r_set))
				{
					char buffer[1024] = {0};
					int recv_len = ::recv(*iter,  buffer, 1024, 0);	

					printf("recv the message %d %s \n", recv_len, buffer);
					if(recv_len <= 0)
					{
						printf("close the socket %d\n", *iter);
	
						if(NULL !=  (cb = T->getcb(CBase::CLOSE)))
							((CLOSE_CALLBACK)cb)(T->pcb[CBase::CLOSE], *iter);	

						close(*iter);	
						FD_CLR(*iter,&o_set);	
						change = (Max_Fd == *iter);
						iter = s_list.erase(iter);
					
						continue;
					}
					else if(NULL != (cb = T->getcb(CBase::READ)))
					{
						((RECV_CALLBACK_TCP)cb)(T->pcb[CBase::READ], buffer, recv_len, *iter);
					}
				}
				else if(FD_ISSET(*iter, &e_set))
				{

					printf("close the socket %d\n", *iter);
									
					if(NULL != (cb = T->getcb(CBase::EXCEPTION)))	
							((EXCEPTION_CALLBACK)cb)(T->pcb[CBase::EXCEPTION],*iter);
		
					close(*iter);	
					FD_CLR(*iter,&o_set);
					change = (Max_Fd == *iter);
					iter = s_list.erase(iter);
					continue;
				}

				iter++;
			}
		
			printf(">>>>>>>>>>>>>>>>>\n");	
			if(FD_ISSET(T->socket_m, &r_set))
			{
				memset(&address, 0, sizeof(address));	
				addr_size = sizeof(address);
				reslt = accept(T->socket_m, (sockaddr*)&address, &addr_size);
				
				printf("accept the socket from : %s %d %d", inet_ntoa(address.sin_addr), ntohs(address.sin_port), reslt);	
				s_list.push_back(reslt);
				FD_SET(reslt, &o_set);	
				Max_Fd = (reslt > Max_Fd)?reslt:Max_Fd;
			}

			printf("<<<<<<<<<<<<<<<<<<<\n");
			if(FD_ISSET(T->socket_m, &e_set))
			{
				if (NULL != (cb = T->getcb(CBase::EXCEPTION)))
					((EXCEPTION_CALLBACK)cb)(T->pcb[CBase::EXCEPTION], T->socket_m);
				goto s_end;
			}
		}	
	}

s_end:
	for(iter = s_list.begin(); iter!=s_list.end();iter++)
		close(*iter);

	
	if (NULL != (cb = T->getcb(CBase::EXIT)))
		((EXIT_CALLBACK)cb)(T->pcb[CBase::EXIT], T->socket_m);

	printf("exit the thread \n");
	return NULL;
}

int CTcp::send(const char* message, unsigned int len)
{
	if(NULL == message)
		return -1;

	return ::send(this->socket_m, message, len, 0);
}
