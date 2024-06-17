#ifndef _NOVI_UREDJAJ_H_
#define _NOVI_UREDJAJ_H_

#include <pthread.h>

#define PORT 55558
#define DATA_BUFFER_LENGTH 256

pthread_mutex_t stdoutMutex;

const char *ssdp_alive_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55558\r\n"
    "LOCATION: \r\n"
    "NTS: ssdp:alive\r\n"
    "USN: Kontroler2\r\n";

const char *ssdp_byebye_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55558\r\n"
    "NTS: ssdp:byebye\r\n"
    "USN: Kontroler2\n";

const char* checkingInMessage = 
    "[CHECKING IN] \"Kontroler2\" is available on the network.\n";

#define CHECKING_IN_PERIOD 30

const char* unique_usn = "Kontroler2";

void* checkingIn(void* mosq);

#endif