#include "socket_p.h"
#include "protobuf/peer-to-peer.pb.h"
#include <map>
#include <string>

using namespace p2p;
using namespace std;

static CUdp* CServer;
static CTcp* CHoleS;

typedef struct {
	string id;
	int status;
	int tcp_socket;
	sockaddr_in udp;
	sockaddr_in tcp;
}SUser, *PSUser;

static std::map<string, SUser> list_usr, list_hole;

void list_remove(std::map<string, SUser> l, string k)
{
	std::map<string, SUser>::iterator iter = l.find(k);
	if(iter != l.end())
		l.erase(iter);

	return;
}

template<typename T>
unsigned int serializeToArray_(char* c, T t)
{
	unsigned int u = t.ByteSize();
	c = new char[u + 1];
	memset(c, 0, u + 1);
	t.serializeToArray(c, u);

	return u;
}

static void* recv_cb_cs(void* p, const char* message, unsigned int len, int& s, sockaddr_in addr)
{
	Msg m;
	char *buffer;
	int buffer_len;
	if(!m.ParseFromArray(message, len))
		return NULL;

	switch(m.type())
	{
		case Msg_MsgType_REQUEST:
			{
				Request rq;
				
				if(!m.has_rqs())
					break;

				rq = m.rqs();

				if(!rq.has_action())
					break;

				switch(rq.action())
				{
				case p2p::PEERONLINE:
					//TODO:return the online users;
					{
						int send_len;
						Msg m_rp;	
						Respond rsp;
					
						for(std::map<string, SUser>::iterator iter =list_usr.begin(); 
								iter != list_usr.end(); iter++)
						{
							rsp.add_id(iter->first);
						}

						m_rp.set_allocated_rsp(&rsp);
						m_rp.set_type(Msg_MsgType_RESPOND);
					
						buffer_len = rsp.ByteSize();	
						buffer = new char[buffer_len + 1];
						memset(buffer, 0, buffer_len + 1);
						
						if(rsp.SerializeToArray(buffer, buffer_len))
						{
							send_len = CServer->sendmessage(addr, buffer, buffer_len);
							//the error maybe caused in the cases that the send buffer
							// of the udp socket is not enough or the send address can 
							// not reach.
							if(send_len < buffer_len)
							{
								//TODO: message buffer and 
								int error = errno;
								printf("the error is %s\n", strerror(error));
								break;	
							}
						}
						

					}
					break;
				default:
					break;
				}
			}
		       break;
		case Msg_MsgType_COMMAND:
			{
				Command c;
				if(!m.has_cmd())
					break;

				c = m.cmd();

				if(!c.has_action())
					break;

				switch(c.action()) 
				{
				case p2p::HELLO:
					//TODO: return the hello ack
					{
						if(!c.has_id())
							break;
						SUser usr;
						usr.udp = addr;
						usr.id = c.id();
						list_usr.insert(map<string, SUser>::value_type(usr.id,usr));

					}
					break;
				case p2p::HEART:
					//TODO: check the heart packet;
					break;
				default:
					break;
				}
			}
			break;
		case Msg_MsgType_INDICATION:
			{
				Indication i;
				if(!i.has_action() || !i.has_result())
					break;
				
				switch(i.action())
				{
				case p2p::CONNECTB:
					//TODO: info the a that b status;
					if(!i.has_result())
						break;
					switch(i.result())
					{
					case p2p::READY:
						//TODO: info a that b has been ready,
						//this case is caused by b cannot connect to a;
						break;
					case p2p::SUCCESS:
						//TODO: trans the msg to a;
						break;
					default:
						break;
					}
					break;
				case p2p::CONNECTA:
					switch(i.result())
					{
					case p2p::SUCCESS:
						//TODO: trans the msg to b;
						break;
					case p2p::FAILURE:
						//TODO: tran the msg to b;
						break;
					default:
						break;
					}
					break;
				default:
					break;	
				}


			}
			break;
		default:
			break;
	}

	return NULL;
}

static void* recv_cb_ch(void* p, const char* message , unsigned int len, int& s)
{
	Msg m;
	m.ParseFromArray(message, len);
	//TODO: record the a addr;
	SUser uA;
	
	unsigned int cbytes;
	char* cbyte;

	switch(m.type())
	{
	case Msg_MsgType_REQUEST: 
		{
			Request rq;
			if(!m.has_rqs())
				break;

			rq = m.rqs();
			switch(rq.action())
			{
			case p2p::CONNECTA:
				//TODO: indication the a request to b;
				{
					int send_len;
						
					if(!rq.has_id())
						break;

					map<string, SUser>::iterator iter = list_usr.find(rq.id());
					Msg msg_rps, msg_ind;
					msg_rps.set_type(Msg_MsgType_RESPOND);
					
					if(iter == list_usr.end())
					{
						//TODO: info A the b is not exit or online;
					}
					else
					{
						//TODO: info B that A will connect to it;
						msg_ind.set_type(Msg_MsgType_INDICATION);
						Indication ind;
						ind.set_action(p2p::CONNECTA);
						ind.set_id(uA.id);

						Address adr;
						adr.set_port(ntohs(uA.tcp.sin_port));
						adr.set_ip(inet_ntoa(uA.tcp.sin_addr.s_addr));
						ind.set_allocated_adr(&adr);
		
						cbytes = serializeToArray_(cbyte, ind);				
						if(CServer->sendmessage(iter->second.udp, cbyte, cbytes) < 0)
						{
							delete[] cbyte;
							cbyte = NULL;
							break;
						}

						delete[] cbyte;
						cbyte = NULL;	

						Respond rps;
						rps.set_result(p2p::SUCCESS);
						rps.set_action(p2p::CONNECTA);
						rps.add_id(iter->first);			
					
						serializeToArray_(cbyte, rps);	
						::send(uA.tcp_socket, cbyte, cbytes, 0);
					}
				}
				break;
				
			default:
				break;	
			}
		}
		break;
	case Msg_MsgType_RESPOND:
		break;
	case Msg_MsgType_COMMAND:
		break;
	case Msg_MsgType_INDICATION:
		break;
	case Msg_MsgType_TRANSCATION:
	}
	return NULL;
}

int start()
{
	CServer = new CUdp();
	CHoleS = new CTcp();

	CHoleS->bindaddr(NULL, TCP_PORT);
	
	CServer->registercb((void*)recv_cb_cs, NULL, READ);
	CHoleS->registercb((void*)recv_cb_ch, NULL, READ);

	CServer->recv();
	CHoleS->recv_server();
	

	return 0;	
}
