#ifndef _USLUZNI_UREDJAJ_H
#define _USLUZNI_UREDJAJ_H

#include <sys/time.h>

#define SSDP_ADDRESS "239.255.255.250"
#define SSDP_PORT 19000
#define PORT 55555

#define BUF_SIZE 1024

#define MAX_TIMEOUT 300
#define COOLDOWN_PERIOD 15
#define SUCCESSIVE_REQUEST_PERIOD 5

void setupSockets(struct sockaddr_in*, struct sockaddr_in*, int*, int*);

const char *ssdp_alive_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55555\r\n"
    "LOCATION: \r\n"
    "NTS: ssdp:alive\r\n"
    "SERVER: \r\n"
    "USN: Senzori\r\n";

const char *ssdp_byebye_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55555\r\n"
    "NTS: ssdp:byebye\r\n"
    "USN: Senzori\n";

#endif