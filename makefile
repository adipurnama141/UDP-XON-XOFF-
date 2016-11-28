all:
    clear
    gcc -o transmitter transmitter.c -lpthread
    gcc -o receiver receiver.c -lpthread
