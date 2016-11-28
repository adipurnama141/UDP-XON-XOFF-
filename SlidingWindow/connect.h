#ifndef _CONNECT_H_
#define _CONNECT_H_

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SOH 0x01
#define STX 0x02
#define ETX 0x03
#define ACK 0x06
#define NAK 0x15
#define XON 0x11
#define XOFF 0x13
#define STARTFILE 0x04
#define ENDFILE 0x1a
#define FRAMEDATASIZE 10
#define	FRAMESIZE (FRAMEDATASIZE + 8)
#define WINDOWSIZE 2

#endif

