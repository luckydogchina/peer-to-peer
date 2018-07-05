#include "channel.h"
#include <map>
#include <string>
#include <errno.h>
#include <stdarg.h> 

using namespace p2p;
using namespace std;

static CUdp* CServerUdp;
static CTcp* CServerTcp;
static CTcp* CHoleS;

enum PeerStatus{
	NONE = 0,
	UDP_EXIST,
	TCP_EXIST,
	ALL_EXIST,
	UDP_EXCEPTAION,
	TCP_EXCEPTAION
};

typedef struct Peer{
	int status; //当前状态 暂时用不到
	int exp;
	int s; //tcp socket链接
	sockaddr_in ip_u; //udp通讯地址
	sockaddr_in ip_t; //tcp通讯地址
	
public:
	Peer()
	{
		this->status = NONE;
		this->exp = NONE;
	       	this->s = -1;
		memset(&this->ip_u, 0, sizeof(this->ip_u));
		memset(&this->ip_t, 0, sizeof(this->ip_t));
	};

	~Peer(){};
}SPeer, *PSPeer;

//现在使用stl map,以后替换为hash map
//peerList 当前在线的用户
//holeList 执行打洞的用户
static std::map<string, PSPeer> peerList, holeList;
pthread_mutex_t peerListLock, holeListLock;
pthread_mutexattr_t lockMutex;


void list_remove(std::map<string, PSPeer> l, string k)
{
	std::map<string, PSPeer>::iterator iter = l.find(k);
	if(iter != l.end())
		l.erase(iter);

	return;
}

typedef int(*SERVANT_API)(PacketRequst& pkt, int s);

#define FIND_USER(X, Y) {\
	result = 0;\
	pthread_mutex_lock(&peerListLock);\
	std::map<string, PSPeer>::iterator iter = peerList.find(X);\
	if(iter == peerList.end()){\
		printf("user is not exist!\n");\
		result = -2;\
	}\
	Y = iter->second;\
	pthread_mutex_unlock(&peerListLock);\
}

#define SEND_MESSAGE(X, Y, Z) {\
	char* cbyte = NULL; unsigned int cbytes = 0; result = 0;\
	cbytes = serializeToArray_(cbyte, (Y));\
	if(!cbytes){\
       		result = -1;\
	}\
	else{\
		switch((X)->status) \
		{\
			case PeerStatus::ALL_EXIST:\
			case PeerStatus::TCP_EXIST:\
				if(::send((X)->s, cbyte, cbytes, 0)<=0){\
					printf("the tcp is error: %s, peer: %s\n",strerror(errno), (Z).c_str());\
					if((X)->status == PeerStatus::TCP_EXIST) {\
						result = -3;\
						(X)->exp = PeerStatus::TCP_EXCEPTAION; break;\
					}\
				}else{ \
					break;\
				}\
			case PeerStatus::UDP_EXIST:\
				if(CServerUdp->sendMessage((X)->ip_u, cbyte, cbytes) <= 0){\
					printf("the udp is error: %s, user: %s\n",strerror(errno), (Z).c_str());\
					result = -3;\
					(X)->exp = PeerStatus::UDP_EXCEPTAION;\
				} break;\
			default:\
				list_remove(peerList, (Z));\
				result = -2;\
		}\
	}\
	delete[] cbyte;\
}

#define CHECK_PEERID()	{\
	if(!pktRequest.has_peeridentity()){\
		printf("the user id is not exist!\n");\
		return -1;\
	}\
}

#define GETADRESS(X) {\
		socklen_t addr_len = sizeof(struct sockaddr_in);\
		getpeername(s, (sockaddr*)&(X), &addr_len);\
}

//发起链接到另一个用户,由HoleClient调用
int servant_connect(PacketRequst& pktRequest, int s){
	int result = 0;
	
	ConnectRequest cntRequest;
	ConnectRespond *cntRespond;

	PSPeer srcPeer, dstPeer;
	PacketRespond pktRespond;

	Message msg;
	sockaddr_in sponsorAddr;
	Address *sponsor;
	/*int s = 1;
	va_list args;

	va_start(args, pktRequest);
	s = va_arg(args, int);
	va_end(args);*/

	//检查来源 peer是否在当前队列中;
	CHECK_PEERID();

	if (!pktRequest.has_connectrequest()){
		printf("the connect struct is not exist!\n");
		return -1;
	}

	cntRequest = pktRequest.connectrequest();
	if (!cntRequest.has_peeridentity()){
		printf("the peer is not exist!\n");
		return -1;
	}

	//TODO: Info the B 
	cntRespond = pktRespond.mutable_connectrespond();
	//检查来源peer是否已经存在
	FIND_USER(pktRequest.peeridentity(), srcPeer);
	if (result >= 0){
		printf("peer %s has beeb exist!!!", pktRequest.peeridentity().c_str());
		return result;
	}
		
	if (srcPeer->status != PeerStatus::TCP_EXIST){
		return -4;
	}
	
	//检查目的peer是否在线
	FIND_USER(cntRequest.peeridentity(), dstPeer);
	if(result < 0){
		//当目的peer不在线的时候，反馈给来源peer
		cntRespond->set_statustype(p2p::NOEXIST);
	}else{	
		//填入发起peer的地址
		msg.set_t(p2p::ADDRESS);
		
		GETADRESS(sponsorAddr);
		sponsor = msg.mutable_address();
		sponsor->set_id(pktRequest.peeridentity());
		sponsor->set_address(inet_ntoa(sponsorAddr.sin_addr));
		sponsor->set_port(ntohs(sponsorAddr.sin_port));
		
		SEND_MESSAGE(dstPeer, msg, cntRequest.peeridentity());
		//TODO: get the A socket, end return the reuslt
		if(result < 0){
			cntRespond->set_statustype(p2p::FAILURE);
		}else{
			cntRespond->set_statustype(p2p::SUCCESS);
		}
	}

	pktRespond.set_rpcapi(p2p::CONNECT);
	SEND_MESSAGE(srcPeer, pktRespond, pktRequest.peeridentity());
	return result;
}

//枚举当前在线的用户
int servant_getpeeronline(PacketRequst& pktRequest, int s)
{
	int result = 0; 
	PSPeer user;
	GetUsersOnlineRespond *pRspGetUserOnline;
	PacketRespond pktRsp;
	
	CHECK_PEERID();
	FIND_USER(pktRequest.peeridentity(), user);
	
	if (result < 0){
		return result;
	}
	
	//将在线的用户加入到应答包中
	pRspGetUserOnline = pktRsp.mutable_getusersonlinerespond();
	for (std::map<string, PSPeer>::iterator iter = peerList.begin();
		iter != peerList.end(); iter++){
		pRspGetUserOnline->add_usersonline(iter->first);
	}

	//返回请求;
	pktRsp.set_rpcapi(p2p::GETUSERSONLINE);
	SEND_MESSAGE(user, pktRsp, pktRequest.peeridentity());

	printf("ok ......... \n");
	return result;	
}



int serveant_hello(PacketRequst& pktRequest, int s)
{
	int result = 0;
	PSPeer newPeer;
	sockaddr_in addr;

	CHECK_PEERID();
	if (result < 0){
		return -1;
	}


	FIND_USER(pktRequest.peeridentity(), newPeer);
	if (result < 0){
		newPeer = new SPeer;
	}

	GETADRESS(addr);
	newPeer->ip_t = addr;
	newPeer->status |= PeerStatus::TCP_EXIST;
	newPeer->s = s;

	if (result < 0){
		pthread_mutex_lock(&peerListLock);
		peerList.insert(std::map<string, PSPeer>::value_type(pktRequest.peeridentity(), newPeer));
		pthread_mutex_unlock(&peerListLock);
	}

	return 0;
}

//消息转发，用于特殊的情况，比如双方无法打洞成功的情况，可以通过转发的形式进行通讯；
int servant_transmit(PacketRequst& pktRequest, int s)
{
	int result = 0;
	
	PSPeer srcPeer, dstPeer;
	NotifyRequest notifyRqs;
	PacketRespond pktRespond;
	NotifyRespond* notifyRsp = NULL;
	Message msg;

	//解析传入的参数
	
	CHECK_PEERID();
	if (!pktRequest.has_notifyrequest()){
		return -1;
	}
		
	//查找源地址和目的地址
	notifyRqs = pktRequest.notifyrequest();
		
	FIND_USER(pktRequest.peeridentity(), srcPeer);
	if (result < 0){
		return -2;
	}

	FIND_USER(notifyRqs.peeridentity(), dstPeer);
	notifyRsp  = pktRespond.mutable_notifyrespond();

	if(result < 0){
		notifyRsp->set_statustype(p2p::NOEXIST);
	}
	else{
		//将通知的内容发送给对方
		SEND_MESSAGE(dstPeer, notifyRqs, notifyRqs.peeridentity());
		
		if(result < 0){
			notifyRsp->set_statustype(p2p::FAILURE);	
		}else{
			notifyRsp->set_statustype(p2p::SUCCESS);
		}
	}
	
	pktRespond.set_rpcapi(p2p::INFO);
	
	SEND_MESSAGE(srcPeer, pktRespond, pktRequest.peeridentity());

	return result;	
}

//后台处理服务队列
std::map<p2p::RPCAPI, SERVANT_API> list_api;

void registry_servant_api(p2p:: RPCAPI type, SERVANT_API api)
{
	list_api.insert(std::map<p2p::RPCAPI, SERVANT_API>::value_type(type, api));
}

#define FIND_API(X, Y)	{\
	std::map<p2p::RPCAPI, SERVANT_API>::iterator iter;\
	iter = list_api.find(X.rpcapi());\
	if (iter == list_api.end()){\
		printf("the api is not exist\n");\
		return NULL;\
	}else if (!iter->second(X, Y)){\
		printf("the result is error \n");\
	}\
	return NULL;\
}

static void* recv_cb_ch(void* p, const char* message, unsigned int len, int& s)
{
	PacketRequst pktRequst;
	if (!pktRequst.ParseFromArray(message, len))
	{
		printf("cannot analyse the msg\n");
		return NULL;
	}

	printf("the packet version is %s \n", pktRequst.version().c_str());
	if (!pktRequst.has_rpcapi())
	{
		printf("don't have api id\n");
		return NULL;
	}

	if (pktRequst.rpcapi() == p2p::HEART)
	{
		//TODO: make heart count;
		return NULL;
	}
	

	FIND_API(pktRequst, s);
}


static void* close_cb(void* p, int& s)
{
	std::map<string, PSPeer>::iterator iter;
	pthread_mutex_lock(&peerListLock);
	for(iter = peerList.begin(); iter != peerList.end();)
	{
		if(iter->second->s == s ){
			peerList.erase(iter);
			delete(iter->second);
			break;
		}
			
		iter++;
	}	
	pthread_mutex_unlock(&peerListLock);
	return NULL;
}
//为UDP 协议的server所准备
static void* recv_cb_cs(void* p, const char* message, unsigned int len, int& s, sockaddr_in addr)
{
	int result = 0;
	PSPeer user;
	PacketRequst pkt;

	if(pkt.ParseFromArray(message, len))
	{
		printf("cannot analyse the msg\n");
		return NULL;
	}

	if (!pkt.has_rpcapi())
	{
		printf("don't have api id\n");
		return NULL;
	}

	if(pkt.rpcapi() == p2p::HEART)
	{
		//TODO: make heart count;
		return NULL;
	}
	

	FIND_API(pkt, s);
}



int start()
{
	//CServerUdp = new CUdp();
	CServerTcp = new CTcp();
	CHoleS = new CTcp();
	
	pthread_mutexattr_init(&lockMutex);
	pthread_mutexattr_settype(&lockMutex, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&peerListLock, &lockMutex);
	pthread_mutex_init(&holeListLock, &lockMutex);

	registry_servant_api(p2p::CONNECT, servant_connect);
	registry_servant_api(p2p::INFO, servant_transmit);
	registry_servant_api(p2p::GETUSERSONLINE, servant_getpeeronline);

	CServerTcp->bindAddress(NULL, TCP_PORT);
	
	//CServer->registercb((void*)recv_cb_cs, NULL, READ);
	CServerTcp->registryCallBackMethod((void*)recv_cb_ch, NULL, CBase::READ);
	CServerTcp->registryCallBackMethod((void*)close_cb, NULL, CBase::EXCEPTION);
	CServerTcp->registryCallBackMethod((void*)close_cb, NULL, CBase::CLOSE);

	CHoleS->bindAddress(NULL, HOLE_PORT);
	CHoleS->registryCallBackMethod((void*)recv_cb_ch, NULL, CBase::READ);

	CServerTcp->startServer();
	CHoleS->startServer();
	

	return 0;	
}

void stop()
{
	delete CServerTcp;
	delete CHoleS;
	CServerTcp = CHoleS = NULL;
	pthread_mutex_destroy(&peerListLock);
	pthread_mutex_destroy(&holeListLock);
	pthread_mutexattr_destroy(&lockMutex);
}


