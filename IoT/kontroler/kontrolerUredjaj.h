#ifndef _KONTROLER_UREDJAJ_H_
#define _KONTROLER_UREDJAJ_H_

#define PORT 55556
#define READING_BUFFER_SIZE 12 * 60

#include "lecaina219.h"
#include <pthread.h>

#define SOLAR_VOLTAGE_HIGH (float)18
#define BATTERY_CURRENT_FULL (float)50
#define BATTERY_VOLTAGE_EMPTY (float)10.5

#define CHECKING_IN_PERIOD 30

enum STATES
{
    SPOLJNO_NAPAJANJE = 0, //spoljasnje napajanje
    PANEL_NAPAJANJE, //akku je pun
    BATERIJA_NAPAJANJE,
    FAULT
};

int optocouplers[4] = {1, 0, 0, 1};
bool remoteControl = true;
int systemState = SPOLJNO_NAPAJANJE;
struct mosquitto* mosq;

pthread_mutex_t inaMutex;
pthread_mutex_t stdoutMutex;
pthread_mutex_t systemStateMutex;
pthread_mutex_t remoteControlMutex;

bool alertMode = false;
pthread_mutex_t alertMutex;

typedef struct
{
    DATAPACK data[READING_BUFFER_SIZE]; //na svaki minut meri
    int firstFree; //pokazuje na 1. prazno mesto
    int lastPlaced;

}ReadingBuffer;

//senzor na panelu
ReadingBuffer ina1Buffer; 
//senzor na bateriji
ReadingBuffer ina2Buffer;
//senzor na izlazu
ReadingBuffer ina3Buffer; 

enum SIGNAL
{
    LOW = 0,
    HIGH
};

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

bool updateSystemState(struct mosquitto* mosq);
void initBuffer(ReadingBuffer* buffer);
void addToBuffer(ReadingBuffer* buffer, DATAPACK* datapack);

void setSupplySolar(struct mosquitto* mosq);
void setSupplyBattery(struct mosquitto* mosq);
void setSupplyGrid(struct mosquitto* mosq);

void raiseAlarm(struct mosquitto* mosq);

void* checkingIn(void*);

const char *ssdp_alive_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55556\r\n"
    "LOCATION: \r\n"
    "NTS: ssdp:alive\r\n"
    "USN: Kontroler1\r\n";

const char *ssdp_byebye_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55556\r\n"
    "NTS: ssdp:byebye\r\n"
    "USN: Kontroler1\n";

const char* checkingInMessage =
    "[CHECKING IN] \"Kontroler1\" is available on the network.\n";

const char* unique_usn = "Kontroler1";

#endif