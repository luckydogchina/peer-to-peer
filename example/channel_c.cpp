#include "../channel.h"
#include <iostream>

#include <unistd.h>
#include <iostream>
int main(int argc, char* argv[])
{
	if(argc != 4)
		return -1;

	Init(argv[2], argv[3], UDP_PORT, TCP_PORT, 8090, atoi(argv[1]));
	peer_start();
	//std::cin.get();
	sleep(3);
	printf("hello");
	hello();
	sleep(3);
	printf("list");
	listonline();

	char buffer[256] = {0};
	std::cin.getline(buffer, 256);

	connect(buffer);
	std::cin.get();
}
