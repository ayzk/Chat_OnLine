client:client_function.o client.o
	cc -o client client_function.o client.o -lpthread
client_function.o:client_function.h client_function.c
	cc -c client_function.c
client.o:client.c
	cc -c client.c
clean:
	rm client.o client_function.o client
