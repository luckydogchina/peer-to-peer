#include "channel.h"
#include <map>
#include <string>
#include <errno.h>
using namespace p2p;
using namespace std;

static CUdp* CServer;
static CTcp* CHoleS;

typedef struct _User{
	int status; //当前状态 暂时用不到
	int exp;
	int s; //tcp socket链接
	sockaddr_in ip_u; //udp通讯地址
	sockaddr_in ip_t; //tcp通讯地址
	enum UserStatus{
		_NONE = 0,
		_UDP_EXIST,
		_TCP_EXIST,
		_ALL_EXIST,
		_UDP_EXCEPTAION,
		_TCP_EXCEPTAION
	};
public:
	_User()
	{
		this->status = _NONE;
		this->exp = _NONE;
	       	this->s = -1;
		memset(&this->ip_u, 0, sizeof(this->ip_u));
		memset(&this->ip_t, 0, sizeof(this->ip_t));
	};

	~_User(){};
}SUser, *PSUser;

//现在使用stl map,以后替换为hash map
static std::map<string, PSUser> list_usr, list_hole;

void list_remove(std::map<string, PSUser> l, string k)
{
	std::map<string, PSUser>::iterator iter = l.find(k);
	if(iter != l.end())
		l.erase(iter);

	return;
}

typedef int(*SERVANT_API)(Packet& pkt);

#define FIND_USER(X, Y) {\
	reslt = 0;\
	std::map<string, PSUser>::iterator iter = list_usr.find(X);\
	if(iter == list_usr.end()){\
		printf("user is not exist!\n");\
		reslt = -2;\
	}\
	Y = iter->second;\
}

#define SEND_RESULT(X, Y, Z) {\
	char* cbyte = NULL; unsigned int cbytes = 0; reslt = 0;\
	cbytes = serializeToArray_(cbyte, (Y));\
	if(!cbytes){\
       		reslt = -1;\
	}\
	else{\
		switch((X)->status) \
		{\
			case _User::_ALL_EXIST:\
			case _User::_TCP_EXIST:\
				if(::send((X)->s, cbyte, cbytes, 0)<=0){\
					printf("the tcp is error: %s, user: %s\n",strerror(errno), (Z).c_str());\
					if((X)->status == _User::_TCP_EXIST) {\
						reslt = -3;\
						(X)->exp = _User::_TCP_EXCEPTAION; break;\
					}\
				}else{ \
					break;\
				}\
			case _User::_UDP_EXIST:\
				if(CServer->sendmessage((X)->ip_u, cbyte, cbytes) <= 0){\
					printf("the udp is error: %s, user: %s\n",strerror(errno), (Z).c_str());\
					reslt = -3;\
					(X)->exp = _User::_UDP_EXCEPTAION;\
				} break;\
			default:\
				list_remove(list_usr, (Z));\
				reslt = -2;\
		}\
	}\
	delete[] cbyte;\
}

#define CHECK_USERID()	{\
	if(!pkt.has_user_id()){\
		printf("the user id is not exist!\n");\
		return -1;\
	}\
}


int servant_connect(Packet& pkt)
{
	int reslt = 0;
	CHECK_USERID();

	if (!pkt.has_cnt())
	{
		printf("the connect struct is not exist!\n");
		return -1;
	}

	Connect cnt = pkt.cnt();
	if (!cnt.has_peer())
	{
		printf("the peer is not exist!\n");
		return -1;
	}

	//TODO: Info the B 
	PSUser user, user1;	
	RPacket rp;
	Connect_r *cr = rp.mutable_cnt();

	FIND_USER(pkt.user_id(), user);
	if(reslt < 0)
		return reslt;

	if(user->status != _User::_TCP_EXIST)
		return -4;

	FIND_USER(cnt.peer(), user1);
	if(reslt < 0)
	{
		cr->set_t(p2p::NOEXIST);
	}
	else
	{	
		Initiative init;
		init.set_t(p2p::ADDRESS);
		Address *sponsor = init.mutable_adr();
		sponsor->set_id(pkt.user_id());
		sponsor->set_addr(inet_ntoa(user->ip_t.sin_addr));	
		sponsor->set_port(ntohs(user->ip_t.sin_port));
		
		SEND_RESULT(user1, init, cnt.peer());
		//TODO: get the A socket, end return the reuslt
		
		if(reslt < 0){
			cr->set_t(p2p::FAILURE);
		}else{
			cr->set_t(p2p::SUCCESS);
		}
	}

	rp.set_api_id(p2p::CONNECT);
	SEND_RESULT(user, rp,pkt.user_id());
	return reslt;
}

int servant_getuseronline(Packet& pkt)
{
	int reslt = 0; 
	PSUser user;
	CHECK_USERID();
	FIND_USER(pkt.user_id(), user);
	
	if(reslt < 0)
		return reslt;

	GetUserOnline_r *lr;
	RPacket rp;
	lr = rp.mutable_guonline();
	for (std::map<string, PSUser>::iterator iter = list_usr.begin();
		iter != list_usr.end(); iter++)
	{
		lr->add_user_online(iter->first);
	}

	//TODO: sent to requester;
	rp.set_api_id(p2p::GETUSERONLINE);
	//rp.set_allocated_guonline(&lr);
	SEND_RESULT(user, rp, pkt.user_id());

	printf("ok ......... \n");
	return reslt;	
}

int servant_info(Packet& pkt)
{
	int reslt = 0;
	CHECK_USERID();
	if(!pkt.has_inf())	
		return -1;
	
	Info i = pkt.inf();
	PSUser user, user1;
	FIND_USER(pkt.user_id(), user1);
	if(reslt < 0)
		return -2;

	FIND_USER(i.peer(), user);
	
	RPacket rp;
	Info_r *ir = rp.mutable_in();
	if(reslt < 0)
	{
		ir->set_t(p2p::NOEXIST);
	}
	else
	{
		Initiative init;
		init.set_t(p2p::ADDRESS);
		Address *respondent;
		respondent = init.mutable_adr();
		respondent->set_id(pkt.user_id());
		respondent->set_addr(inet_ntoa(user1->ip_t.sin_addr));
		respondent->set_port(ntohs(user1->ip_t.sin_port));

		SEND_RESULT(user, init, i.peer());
		
		if(reslt < 0){
			ir->set_t(p2p::FAILURE);	
		}else{
			ir->set_t(p2p::SUCCESS);
		}
	}
	
	rp.set_api_id(p2p::INFO);
	
	SEND_RESULT(user1, rp, pkt.user_id());

	return reslt;	
}

std::map<p2p::API_ID, SERVANT_API> list_api;

void registry_servant_api(p2p::API_ID apiid, SERVANT_API api)
{
	list_api.insert(std::map<p2p::API_ID, SERVANT_API>::value_type(apiid, api));
}

#define FIND_API(X)	{\
	std::map<p2p::API_ID, SERVANT_API>::iterator iter;\
	iter = list_api.find(X.api_id());\
	if (iter == list_api.end()){\
		printf("the api is not exist\n");\
		return NULL;\
	}else if (!iter->second(X)){\
		printf("the reslt is error \n");\
	}\
	return NULL;\
}

static void* recv_cb_ch(void* p, const char* message, unsigned int len, int& s)
{
	Packet pkt;
	if(!pkt.ParseFromArray(message, len))
	{
		printf("cannot analyse the msg\n");
		return NULL;
	}

	printf("the packet version is %s \n", pkt.version().c_str());
	if (!pkt.has_api_id())
	{
		printf("don't have api id\n");
		return NULL;
	}

	if(pkt.api_id() == p2p::HEART)
	{
		//TODO: make heart count;
		return NULL;
	}
	
	if(pkt.api_id() == p2p::HELLO)
	{
		sockaddr_in addr;
		socklen_t addr_len = sizeof(struct sockaddr_in);
		getpeername(s, (sockaddr*)&addr, &addr_len);

		PSUser user;
		int reslt = 0;
		FIND_USER(pkt.user_id(), user);
		if(reslt < 0){
			user = new SUser;
		}

		user->ip_t = addr;
		user->status |= _User::_TCP_EXIST;
		user->s = s;
		
		if(reslt < 0){
			list_usr.insert(std::map<string, PSUser>::value_type(pkt.user_id(), user));
			printf("addr the new user %s %s %d\n", pkt.user_id().c_str(), inet_ntoa(user->ip_t.sin_addr), ntohs(user->ip_t.sin_port));
		}
		return NULL;
	}

	FIND_API(pkt);
}


static void* close_cb(void* p, int& s)
{
	std::map<string, PSUser>::iterator iter;
	for(iter = list_usr.begin(); iter != list_usr.end();)
	{
		if(iter->second->s == s ){
			list_usr.erase(iter);
			delete(iter->second);
			break;
		}
			
		iter++;
	}	

	return NULL;
}

static void* recv_cb_cs(void* p, const char* message, unsigned int len, int& s, sockaddr_in addr)
{
	Packet pkt;
	if(pkt.ParseFromArray(message, len))
	{
		printf("cannot analyse the msg\n");
		return NULL;
	}

	if (!pkt.has_api_id())
	{
		printf("don't have api id\n");
		return NULL;
	}

	if(pkt.api_id() == p2p::HEART)
	{
		//TODO: make heart count;
		return NULL;
	}
	
	if(pkt.api_id() == p2p::HELLO)
	{
		PSUser user;
		int reslt = 0;
		FIND_USER(pkt.user_id(), user);
		if(reslt < 0)
			user = new SUser;

		user->ip_t = addr;
		user->status |= _User::_TCP_EXIST;
		user->s = s;
		
		if(reslt < 0)
			list_usr.insert(std::map<string, PSUser>::value_type(pkt.user_id(), user));

		return NULL;
	}

	FIND_API(pkt);
}



int start()
{
	//CServer = new CUdp();
	CHoleS = new CTcp();
	
	registry_servant_api(p2p::CONNECT, servant_connect);
	registry_servant_api(p2p::INFO, servant_info);
	registry_servant_api(p2p::GETUSERONLINE, servant_getuseronline);

	CHoleS->bindaddr(NULL, TCP_PORT);
	
	//CServer->registercb((void*)recv_cb_cs, NULL, READ);
	CHoleS->registercb((void*)recv_cb_ch, NULL, CBase::READ);
	CHoleS->registercb((void*)close_cb, NULL, CBase::EXCEPTION);
	CHoleS->registercb((void*)close_cb, NULL, CBase::CLOSE);

	//CServer->recv();
	CHoleS->recv_server();
	
	return 0;	
}

void stop()
{
//	delete CServer;
	delete CHoleS;
}


