#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <mosquitto.h>

#include "usluzniUredjaj.h"
#include "topics.h"
#include "lecaina219.h"

void on_connect(struct mosquitto* mosq, void* obj, int rc)
{
    printf("Connected!\n");
}

void on_disconnect(struct mosquitto* mosq, void* obj, int rc)
{
    printf("Disconnected!\n");
}

void on_subscribe(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos)
{
    printf("Subscribed!");
}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
    printf("Received message:\n");
    printf("%s\n", (char*)msg->payload);
}

int main() 
{
    struct sockaddr_in server_addr, client_addr;
    int client_fd, server_fd;
    char buffer[BUF_SIZE];
    socklen_t client_addr_len = sizeof(client_addr);

    setupSockets(&server_addr, &client_addr, &server_fd, &client_fd);

    // OVDE IDE INA I MQTT
    
    mosquitto_lib_init();
    struct mosquitto* mosq;
    mosq = mosquitto_new(NULL, true, NULL);

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_subscribe_callback_set(mosq, on_subscribe);
    mosquitto_message_callback_set(mosq, on_message);

    int crc = mosquitto_connect(mosq, BROKER_ADDR, BROKER_PORT, BROKER_TIMEOUT);

    if(crc != MOSQ_ERR_SUCCESS)
    {
        printf("Neuspesno povezivanje na MQTT Broker. Kod greske: %d\n", crc);
        exit(EXIT_FAILURE);
    }

    mosquitto_subscribe(mosq, NULL, CONTROLLER_TOPIC, 1);
    
    //---------------------
    
    while(1)
    {
        sendto(client_fd, (const char *)ssdp_alive_msg, strlen(ssdp_alive_msg), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        
        int success = recvfrom(server_fd, (char*)&buffer, BUF_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &client_addr_len);
        
        if(success < 0) //timeout ili greska
        { 
            printf("Doslo je do tajmauta servera. Ponovni pokusaj za %d sekundi.\n", COOLDOWN_PERIOD);
            //cooldown
            usleep(COOLDOWN_PERIOD * 1000000);
            continue;
        }
        else    //success
        {
            printf("Primljen ACK.\n");
            //sleep, kasnije merenje i obrada podataka za slanje
            usleep(3000000);

            //byebye poruka
            //sendto(client_fd, (const char *)ssdp_byebye_msg, strlen(ssdp_byebye_msg), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        }  
    }

    close(server_fd);
    close(client_fd);
    return 0;
}


void setupSockets(struct sockaddr_in* server_addr, struct sockaddr_in* client_addr, int* server_fd, int* client_fd)
{
    //Soket za recv
    if ((*client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) 
    {
        perror("Neuspesno pravljenje soketa. (C)\n");
        exit(EXIT_FAILURE);
    }

    memset(server_addr, 0, sizeof(*server_addr));
    
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(SSDP_PORT);
    server_addr->sin_addr.s_addr = inet_addr(SSDP_ADDRESS);

    //Soket za send
    if ((*server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) 
    {
        perror("Neuspesno pravljenje soketa. (S)\n");
        exit(EXIT_FAILURE);
    }

    memset(client_addr, 0, sizeof(*client_addr));

    client_addr->sin_family = AF_INET;
    client_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr->sin_port = htons(PORT);

    //Namestanje timeout-a za soket
    struct timeval timeout;
    timeout.tv_sec = MAX_TIMEOUT / 1000;
    timeout.tv_usec = MAX_TIMEOUT * 1000;

    if(setsockopt(*server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0)
    {
        perror("Neuspesno namestanje tajmauta za prijem poruke.\n");
        exit(EXIT_FAILURE);
    }

    if (bind(*server_fd, (const struct sockaddr *)client_addr, sizeof(*client_addr)) == -1) 
    {
        perror("Neuspesan bind.\n");
        exit(EXIT_FAILURE);
    }

    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(SSDP_ADDRESS);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(*server_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) 
    {
        perror("Greska prilikom podesavanja opcija soketa.\n");
        exit(EXIT_FAILURE);
    }
}