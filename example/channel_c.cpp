#include "../channel.h"


#include <unistd.h>
#include <iostream>
#include <errno.h>

void connect(){
	char buffer[256] = {0};
	printf("please input the dest peer identity:\n");
	std::cin.get();
	std::cin.getline(buffer, 256);
	assert(0 == peer_connect(buffer));
}

int main(int argc, char* argv[])
{
	int choice = 0;
	if(argc != 4){
		printf("input arguments wrong\n");
		return -1;
	}

	peer_init(argv[2], argv[3],  atoi(argv[1]));
	if(0 != peer_start()) {
		printf("start peer failure: %s", strerror(errno));
		return -1;
	}

	assert(peer_hello() == 0);

	for (;;) {
		printf("1: list peer online, 2: connect to one peer, 0: exit: \n");
		scanf("%d", &choice);
		switch(choice){
			case 0:
				return 0;
			case 1:
				assert(0 == peer_listonline());
				break;
			case 2:
				connect();
				break;
			default:
				printf("the choice is not supported!\n");
		}
	}
}
