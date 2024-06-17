#ifndef _AKTUATOR_UREDJAJ_H_
#define _AKTUATOR_UREDJAJ_H_

#include <stdbool.h>
#include <pthread.h>

#define PORT 55557
#define CHECKING_IN_PERIOD 30

#define OK1 0
#define OK2 1
#define OK3 2
#define OK4 3

bool alertMode = false;
pthread_mutex_t alertMutex;
pthread_mutex_t stdoutMutex;

char lastIssuedCommand = 0b00001001;
bool commandChanged = true;

void* checkingIn(void*);

typedef struct
{
    unsigned b0: 1;
    unsigned b1: 1;
    unsigned b2: 1;
    unsigned b3: 1;
    unsigned b4: 1;
    unsigned b5: 1;
    unsigned b6: 1;
    unsigned b7: 1;
}bf;

typedef union
{
    bf bitField;
    unsigned char entireByte;
    
}bf_mask;

const char *ssdp_alive_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55557\r\n"
    "LOCATION: \r\n"
    "NTS: ssdp:alive\r\n"
    "USN: Aktuator1\r\n";

const char *ssdp_byebye_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55557\r\n"
    "NTS: ssdp:byebye\r\n"
    "USN: Aktuator1\n";

const char* checkingInMessage = 
    "[CHECKING IN] \"Aktuator1\" is available on the network.\n";

const char* unique_usn = "Aktuator1";

#endif
