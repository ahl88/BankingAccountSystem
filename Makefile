all: bankingServer bankingClient

bankingServer: bankingServer.c
	gcc -Wall -Werror -fsanitize=address -pthread bankingServer.c -o bankingServer

bankingClient: bankingClient.c
	gcc -Wall -Werror -fsanitize=address -pthread bankingClient.c -o bankingClient

clean:
	rm -rf bankingServer
	rm -rf bankingClient

