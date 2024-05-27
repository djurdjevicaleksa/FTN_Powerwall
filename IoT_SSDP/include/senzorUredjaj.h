#ifndef _SENZOR_UREDJAJ_H_
#define _SENZOR_UREDJAJ_H_

#define DATA_BUFFER_LENGTH 256

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

const char* unique_usn = "Senzori";

#endif