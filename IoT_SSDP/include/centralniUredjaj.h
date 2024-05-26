#ifndef _CENTRALNI_UREDJAJ_H_
#define _CENTRALNI_UREDJAJ_H_

#include <pthread.h>

#define SSDP_ADDRESS "239.255.255.250"
#define SSDP_PORT 19000
#define MAX_DEVICES 3
#define BUF_SIZE 1024

#define MAX_LINES 10
#define MAX_LINE_LENGTH 1024
#define DATA_BUFFER_LENGTH 256

//Treba da bude <Realna vrednost> + 1
#define TIME_UNTIL_DISCONNECT 6
#define SECOND 1000000

#define ACK "ACK"

typedef struct Device 
{
    char usn[DATA_BUFFER_LENGTH];  // Unique Service Name (USN) of the device
    char location[DATA_BUFFER_LENGTH];  // Location URL of the device
    char host[DATA_BUFFER_LENGTH];
    long lastMessageTime;
} Device;


char* allowedDevices[MAX_DEVICES] = {"Senzori", "Aktuatori", "Kontroler"};

static Device* devices[MAX_DEVICES] = {NULL};
static short numberOfDevices = 0;
static struct timeval _time;
static pthread_mutex_t m;

void setupSockets(struct sockaddr_in*, int*, int*);
void receiveMessage(struct sockaddr_in*, int*, char*);
void sendACK(struct sockaddr_in*, int*, int*);
void printDevices();
void* timeoutChecker();
int parseMessage(const char*);


#endif