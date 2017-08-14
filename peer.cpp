#include "channel.h"
#include <string>
#include <errno.h>
#include <unistd.h>

using namespace p2p;

#define HOLE_PORT 9798

static sockaddr_in hole, server;
static short port_udp_bind, port_tcp_bind;
static short port_udp_server, port_tcp_server;
static std::string user_id;
static std::string server_ip, server_udp;

CTcp *CHole;
CTcp *CP2p;
#define SEND_PACKET	{\
	char* cbyte = NULL;\
	unsigned int cbytes = 0;\
	cbytes = serializeToArray_(cbyte, pkt);\
	if(cbytes <= 0){\
		printf("this is out\n");\
		return -1;\
	}\
	printf("send %d\n", CHole->send(cbyte, cbytes));\
	printf(">>>>>>>>>>>>>>>>\n");\
	delete[] cbyte;\
}

void Init(const char* _user_id, const char* _server_ip, short udp_port, short tcp_port, short bind_udp_port, short bind_tcp_port)
{
	user_id = _user_id;
	server_ip = _server_ip;
	port_tcp_server = tcp_port;
	port_udp_server = udp_port;
	port_tcp_bind = bind_tcp_port;
	port_udp_bind = bind_udp_port;

	return;
}



int hello()
{
	Packet pkt;
	//pkt.set_version();
	pkt.set_api_id(p2p::HELLO);
	
	pkt.set_user_id(user_id);
	printf("%s\n", pkt.version().c_str());	
	SEND_PACKET;
	return 0;	
}

int listonline()
{
	Packet pkt;
	pkt.set_api_id(p2p::GETUSERONLINE);
	pkt.set_user_id(user_id);

	SEND_PACKET;

	return 0;
}


int connect(const char* peer)
{
	Packet pkt;
	pkt.set_api_id(p2p::CONNECT);
	pkt.set_user_id(user_id);
	Connect *c =  pkt.mutable_cnt();
	c->set_peer(::std::string(peer));
	SEND_PACKET;
	
	return 0;
}

static int rpanalyse(RPacket rp)
{
	switch(rp.api_id())
	{
		case p2p::HELLO:
			printf("hello from server\n");
			break;
		case p2p::GETUSERONLINE:
			if(!rp.has_guonline()){
				printf(" the peer list is not exist");
				return -1;
			}
			else{
				GetUserOnline_r gr;
				gr = rp.guonline();
				for(int j = 0 ; j < gr.user_online_size(); j++){
					const std::string& a = gr.user_online(j);
					printf("===online== %s ==\n", a.c_str());
				}

			}break;
		case p2p::CONNECT:
			if(!rp.has_cnt()){
				printf("the cnt rp is not exist !\n");
				delete CP2p;
				return -1;
			}
			else{
				Connect_r cnr;
				cnr = rp.cnt();
				printf("the connect  return : %d\n", cnr.t());
				if(cnr.t() != p2p::SUCCESS)
				{
					printf(" the loser!!!!");
					delete CP2p;	
				}
			}
			break;
		case p2p::INFO:
			if(!rp.has_in()){
				printf("the info is not exist!\n");
				return -1;
			}
			else{
				Info_r ir = rp.in();
				return -1;
				printf("the info return: %d\n", ir.t());
			}
		default:
			break;
	}

	return 0;
}


static int initanalyse(Initiative in)
{
	Packet pkt;
	if(!in.has_adr())
		return -1;
	
	if(CP2p != NULL)
		delete CP2p;

	CP2p = new CTcp();

	Address addr = in.adr();
	if(CP2p->bindaddr(NULL, HOLE_PORT))
	{

		printf(" bind the port %d error %s \n", port_tcp_bind, strerror(errno));
		delete CP2p; CP2p = NULL;
		return -1;
	}
	else
	{
		printf("connect to the %s %d\n", addr.addr().c_str(), HOLE_PORT);
		if(!CP2p->connect(addr.addr().c_str(), HOLE_PORT, 5))
		{
			printf("connect SUCCESS, you can transcation !!!\n");
			CP2p->recv_client();
			return 0;
		}
	}		

	delete CP2p;
	CP2p = new CTcp();
	if(CP2p->bindaddr(NULL, HOLE_PORT)){

		printf(" bind the port %d error %s\n", port_tcp_bind, strerror(errno));
		return -1;
	}

		
	CP2p->recv_server();
	sleep(1);
	
	pkt.set_api_id(p2p::INFO);
	pkt.set_user_id(user_id);

	Info* inf = pkt.mutable_inf();
	inf->set_peer(addr.id());
	printf("info the %s , i am ready\n", addr.id().c_str());

	SEND_PACKET;

	return 0;
}

static void* recv_cb_ch(void* p, const char* message, unsigned int len, int& s)
{
	RPacket rp;
	if(rp.ParseFromArray(message, len))
	{
		rpanalyse(rp);
		return NULL;
	}

	printf("the version is %s\n", rp.version().c_str());	
	Initiative init;
	if(init.ParseFromArray(message, len))
	{
		initanalyse(init);
	}

	return NULL;	
}

int peer_start()
{
	CHole = new CTcp();
	CHole->registercb((void*)recv_cb_ch, NULL, CBase::READ);
	CHole->bindaddr(NULL, port_tcp_bind);
	if(!CHole->connect(server_ip.c_str(), port_tcp_server, 5))
	{
		return CHole->recv_client();
	}

	return -1;
}


