all: server.out client.out

re:
	$(MAKE) fclean
	$(MAKE) all

fclean:
	rm *.out

server.out: server.c
	$(CC) $<  -o $@

client.out: client.c
	$(CC) $<  -o $@