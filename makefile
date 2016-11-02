all:
	gcc -o transmit transmitter.c -lpthread
	gcc -o receive receiver.c -lpthread
