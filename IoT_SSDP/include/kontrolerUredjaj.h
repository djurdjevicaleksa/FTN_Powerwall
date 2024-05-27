#ifndef _KONTROLER_UREDJAJ_H_
#define _KONTROLER_UREDJAJ_H_

typedef struct
{
    unsigned a: 1;
    unsigned b: 1;
    unsigned c: 1;
    unsigned d: 1;
    unsigned e: 1;
    unsigned f: 1;

    unsigned g: 1;
    unsigned h: 1;
}bf;

bool updateSystemState(int* ocs, struct mosquitto* mosq);

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