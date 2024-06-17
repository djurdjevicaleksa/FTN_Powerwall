#ifndef _SENZOR_UREDJAJ_H_
#define _SENZOR_UREDJAJ_H_

#include <stdbool.h>
#include <pthread.h>

#define PORT 55555
#define DATA_BUFFER_LENGTH 256

bool alertMode = false;
pthread_mutex_t alertMutex;
pthread_mutex_t stdoutMutex;

void* checkingIn(void*);

const char *ssdp_alive_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55555\r\n"
    "LOCATION: \r\n"
    "NTS: ssdp:alive\r\n"
    "USN: Senzor1\r\n";

const char *ssdp_byebye_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55555\r\n"
    "NTS: ssdp:byebye\r\n"
    "USN: Senzor1\n";

const char* checkingInMessage = 
    "[CHECKING IN] \"Senzor1\" is available on the network.\n";

#define CHECKING_IN_PERIOD 30

const char* unique_usn = "Senzor1";

#endif