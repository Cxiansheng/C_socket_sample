exec-server:exec-server.o
	gcc -o exec-server exec-server.o -lpthread

exec-server.o:exec-server.c
	gcc -c exec-server.c

.PHONY:	clean
clean:
	rm -f exec-server.o exec-server


