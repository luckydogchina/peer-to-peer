#include "channel.h"
#include <string>
#include <errno.h>
#include <unistd.h>

using namespace p2p;

static sockaddr_in _hole_addr, _server_addr;
static short _port_udp_bind, _port_tcp_bind;
static short _port_udp_server, _port_tcp_server;
static std::string _peerIdentity;
static std::string _server_ip, _server_udp;

CTcp *CListener; //用于接受其他节点的链接
CTcp *CHole; //用于与打洞服务器沟通;
CTcp *CPeer; //用于与Peer服务器进行沟通;

#define SEND_PACKET(X)	{\
	char* cbyte = NULL;\
	unsigned int cbytes = 0;\
	cbytes = serializeToArray_(cbyte, pktRequest);\
	if(cbytes <= 0){\
		printf("this is out\n");\
		return -1;\
	}\
	printf("send %d\n", (X)->sendMessage(cbyte, cbytes));\
	delete[] cbyte;\
}

void peer_init(const char* peerIdentity, const char* server_ip, short bind_tcp_port)
{
	_peerIdentity = peerIdentity;
	_server_ip = server_ip;
	_port_tcp_server = TCP_PORT;
	_port_tcp_bind = bind_tcp_port;

	return;
}

//发送Hello请求，会将peer的消息注册到Server
int peer_hello()
{
	PacketRequst pktRequest;
	pktRequest.set_rpcapi(p2p::HELLO);	
	pktRequest.set_peeridentity(_peerIdentity);
	printf("%s\n", pktRequest.version().c_str());
	SEND_PACKET(CPeer);
	return 0;	
}

//枚举当前在线的Peer
int peer_listonline()
{
	PacketRequst pktRequest;
	pktRequest.set_rpcapi(p2p::GETUSERSONLINE);
	pktRequest.set_peeridentity(_peerIdentity);

	SEND_PACKET(CPeer);

	return 0;
}

//向指定的peer发送链接的请求
int peer_connect(const char* peer)
{
	PacketRequst pktRequest;
	pktRequest.set_rpcapi(p2p::CONNECT);
	pktRequest.set_peeridentity(_peerIdentity);
	ConnectRequest *cntRequest = pktRequest.mutable_connectrequest();
	cntRequest->set_peeridentity(::std::string(peer));
	cntRequest->set_counter(0);

	//创建CHole对象
	if (CHole != NULL || CListener != NULL){
		delete CHole; delete CListener;
		CHole =CListener = NULL;
	}

	CHole = new CTcp(); CListener = new CTcp();
	//即使是发起链接也需要绑定地址
	if (0 != CHole->bindAddress(NULL, _port_tcp_bind) || 0 != CListener->bindAddress(NULL, _port_tcp_bind)){
		printf("bint the port: %d failure!!: %s\n", _port_tcp_bind, strerror(errno));
		return -1;
	}

	//链接到打洞服务器
	if (0!=CHole->connect(_server_ip.c_str(), HOLE_PORT, 5)){
		printf("connect the hole server failure\n");
		return -1;
	}

	//启动监听接受线程
	if (0 != CListener->startServer()){
		printf("start the listen failure\n");
		return -1;
	}

	//发送打洞引导请求
	SEND_PACKET(CHole);
	
	return 0;
}

//解析server返回的respond
static int packtRespondAnalysis(PacketRespond pktRespond)
{
	GetUsersOnlineRespond guoRespond;
	ConnectRespond cntRespond;
	NotifyRespond  notifyRespond;

	switch (pktRespond.rpcapi())
	{
		case p2p::HELLO:
			printf("hello from server\n");
			break;
		case p2p::GETUSERSONLINE:
			if (!pktRespond.has_getusersonlinerespond()){
				printf(" the peer list is not exist");
				return -1;
			}
			else{
				//打印当前在线的peer
				guoRespond = pktRespond.getusersonlinerespond();
				for (int j = 0; j < guoRespond.usersonline_size(); j++){
					const std::string& peer = guoRespond.usersonline(j);
					printf("===online== %s ==\n", peer.c_str());
				}

			}break;
		case p2p::CONNECT:
			if (!pktRespond.has_connectrespond()){
				printf("the cnt rp is not exist !\n");
				delete CHole;
				return -1;
			}
			else{
				cntRespond = pktRespond.connectrespond();
				printf("the connect  return status: %d\n", cntRespond.statustype());
				if (cntRespond.statustype() != p2p::SUCCESS){
					printf("Ok, waiting connection... ...\n");
					delete CHole;	
				}
			}
			break;
		case p2p::INFO:
			if (!pktRespond.has_notifyrespond()){
				printf("the info is not exist!\n");
				return -1;
			}
			else{
				notifyRespond = pktRespond.notifyrespond();
				printf("the info return status: %d\n", notifyRespond.statustype());
			}
			break;
		default:
			break;
	}

	return 0;
}


static int messageAnylasis(Message msg)
{
	Address addr;
	switch (msg.t())
	{
	case p2p::ADDRESS:
		{
			if (!msg.has_address()){
				printf("address is emptu\n");
				return -1;
			}

			if (CHole != NULL || CListener != NULL){
				delete CHole;delete CListener;
				CHole = CListener = NULL;
			}
			
			CHole = new CTcp();
			addr = msg.address();
			if (CHole->bindAddress(NULL, _port_tcp_bind)){
				printf(" bind the port %d failure:  %s \n", _port_tcp_bind, strerror(errno));
				delete CHole; CHole = NULL;
				return -1;
			}

			//开始直连
			printf("connect to the %s:%d\n", addr.address().c_str(), addr.port());
			if (!CHole->connect(addr.address().c_str(), addr.port(), 5)){
				//直连成功开始通信
				printf("connect SUCCESS, you can transcation !!!\n");
				CHole->startClient();
				break;
			}else{
				//如果不成功也相当于打洞成功，此时向server发起链接的请求；
				printf("connect server failure: %s\n", strerror(errno));
			}

			//由被请求端发起connet请求，进行反向引导
			delete CHole; CHole = NULL;
			peer_connect(addr.id().c_str());

			//开启监听状态
			// CHole->startServer();
			// if (0 > peer_connect(addr.id().c_str())){
			// 	printf("send connect request failure: %s", strerror(errno));
			// 	delete CHole;
			// 	CHole = NULL;
			// }
		}
	default:
		printf("the message type: %d is not supportted\n", msg.t());
		return -2;
	}


	return 0;
}

static void* recv_cb_ch(void* p, const char* message, unsigned int len, int& s)
{
	PacketRespond pktRespond;
	Message msg;

	if (pktRespond.ParseFromArray(message, len)){
		printf("the version of the respond is %s\n", pktRespond.version().c_str());
		packtRespondAnalysis(pktRespond);
		return NULL;
	}


	if(msg.ParseFromArray(message, len)){
		printf("the version of the message is %s\n", msg.version().c_str());
		messageAnylasis(msg);
	}

	return NULL;	
}

int peer_start()
{
	CPeer = new CTcp();
	CPeer->registryCallBackMethod((void*)recv_cb_ch, NULL, CBase::READ);
	//CPeer->bindAddress(NULL, _port_tcp_bind);
	if (!CPeer->connect(_server_ip.c_str(), _port_tcp_server, 5)){
		return CPeer->startClient();
	}

	return -1;
}


