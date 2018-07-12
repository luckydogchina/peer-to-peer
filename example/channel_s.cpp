#include "../channel.h"
#include <iostream>

int main()
{
	server_start();
	std::cin.get();
	server_stop();
}
