#ifndef _KONTROLER_UREDJAJ_H_
#define _KONTROLER_UREDJAJ_H_

#define READING_BUFFER_SIZE 3 * 60

#include "lecaina219.h"

typedef struct
{
    DATAPACK data[READING_BUFFER_SIZE]; //na svaki minut meri
    int pointer; //pokazuje na 1. prazno mesto

}ReadingBuffer;

//senzor na panelu
ReadingBuffer ina1Buffer; 
//senzor na bateriji
ReadingBuffer ina2Buffer;
//senzor na izlazu
ReadingBuffer ina3Buffer; 

enum STATES
{
    SPOLJNO_NAPAJANJE = 0, //spoljasnje napajanje
    PANEL_NAPAJANJE, //akku je pun
    BATERIJA_NAPAJANJE
};

enum SIGNAL
{
    LOW = 0,
    HIGH
};
bool updateSystemState(int* ocs, struct mosquitto* mosq);
void initBuffer(ReadingBuffer* buffer);
void addToBuffer(ReadingBuffer* buffer, DATAPACK* datapack);

const char *ssdp_alive_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55555\r\n"
    "LOCATION: \r\n"
    "NTS: ssdp:alive\r\n"
    "SERVER: \r\n"
    "USN: Kontroler\r\n";

const char *ssdp_byebye_msg =
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 55555\r\n"
    "NTS: ssdp:byebye\r\n"
    "USN: Kontroler\n";

#endif