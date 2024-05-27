#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <mosquitto.h>
#include <stdbool.h>

#include "usluzniUredjaj.h"
#include "kontrolerUredjaj.h"
#include "topics.h"

void on_connect(struct mosquitto* mosq, void* obj, int rc)
{
    printf("Connected.\n");
}

void on_disconnect(struct mosquitto* mosq, void* obj, int rc)
{
    printf("Disconnected!\n");
}

void on_publish(struct mosquitto* mosq, void* obj, int mid)
{
    printf("Published.\n");
}

void on_subscribe(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos)
{
    printf("Subscribed.\n");
}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
    //obradi ako poruka pocinje sa GET ili SET za onu app iz praktikuma
    //aka proveri da li su prva 3 karaktera GET ili SET (tako definisano u praktikumu)

    char* topic = (char*)msg->topic;
    
    if(strncmp(topic, DATA_TOPIC, 5) == 0)
    {
        DATAPACK* data = (DATAPACK*)msg->payload;

        char* topic = msg->topic;

        char inaName[4];
        strncpy(inaName, topic + 5, 4);

        if(strncmp(inaName, "ina1", 4) == 0)
        {
            addToBuffer(&ina1Buffer, data);
        }
        else if(strncmp(inaName, "ina2", 4) == 0)
        {
            addToBuffer(&ina2Buffer, data);
        }
        else if(strncmp(inaName, "ina3", 4) == 0)
        {
            addToBuffer(&ina3Buffer, data);
        }
        else
        {
            fprintf(stderr, "Received undefined device signature: %s", inaName);
        }
    }
    else if(strncmp(topic, GETS_TOPIC, 5) == 0)
    {
        //gets komanda
    }
    else if(strncmp(topic, SETS_TOPIC, 5) == 0)
    {

    }
    else
    {
        //bad request
    }
}

/*
    SPOLJNO NAPAJANJE:
        akumulator se puni, ova switcha su povezana

    PANEL NAPAJANJE:
        switch kod panela je raskacen, kod baterije svj, spoljasnje napajanje ugaseno relejem

    BATERIJA NAPAJANJE:
        kod panela prekid, kod baterije spojeno, relejem ugaseno spoljasnje napajanje 


*/

int main() 
{
    struct sockaddr_in server_addr, client_addr;
    int client_fd, server_fd;
    char buffer[BUF_SIZE];
    socklen_t client_addr_len = sizeof(client_addr);

    initBuffer(&ina1Buffer);
    initBuffer(&ina2Buffer);
    initBuffer(&ina3Buffer);

    int optocouplers[6] = {0};

    int systemState = SPOLJNO_NAPAJANJE;

    setupSockets(&server_addr, &client_addr, &server_fd, &client_fd);

    // OVDE IDE INA I MQTT
    
    mosquitto_lib_init();
    struct mosquitto* mosq;
    mosq = mosquitto_new(NULL, true, NULL);

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_subscribe_callback_set(mosq, on_subscribe);
    mosquitto_message_callback_set(mosq, on_message);
    mosquitto_publish_callback_set(mosq, on_publish);

    if(mosquitto_subscribe(mosq, NULL, DATA_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to SENSOR topic.\n");
    }

    if(mosquitto_subscribe(mosq, NULL, GETS_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to GETS topic.\n");
    }

    if(mosquitto_subscribe(mosq, NULL, SETS_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to SETS topic.\n");
    }


    int crc = mosquitto_connect(mosq, BROKER_ADDR, BROKER_PORT, BROKER_TIMEOUT);

    if(crc != MOSQ_ERR_SUCCESS)
    {
        printf("Neuspesno povezivanje na MQTT Broker. Kod greske: %d\n", crc);
        exit(EXIT_FAILURE);
    }

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

            switch(systemState)
            {
                case SPOLJNO_NAPAJANJE:
                {
                    optocouplers[0] = HIGH;
                    optocouplers[1] = HIGH;
                    optocouplers[2] = LOW;
                    optocouplers[3] = LOW;
                    optocouplers[4] = LOW;
                    optocouplers[5] = HIGH;

                    updateSystemState(optocouplers, mosq);

                    break;
                }

                case PANEL_NAPAJANJE:
                {
                    optocouplers[0] = LOW;
                    optocouplers[1] = LOW;
                    optocouplers[2] = HIGH;
                    optocouplers[3] = LOW;
                    optocouplers[4] = HIGH;
                    optocouplers[5] = LOW;

                    updateSystemState(optocouplers, mosq);

                    break;
                }

                case BATERIJA_NAPAJANJE:
                {
                    optocouplers[0] = LOW;
                    optocouplers[1] = LOW;
                    optocouplers[2] = LOW;
                    optocouplers[3] = HIGH;
                    optocouplers[4] = HIGH;
                    optocouplers[5] = LOW;

                    updateSystemState(optocouplers, mosq);

                    break;
                }

                default:
                {
                    exit(EXIT_FAILURE);
                }
            }

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

void initBuffer(ReadingBuffer* buffer)
{
    buffer->pointer = 0;
    memset(&buffer, 0, sizeof(buffer));
}

void addToBuffer(ReadingBuffer* buffer, DATAPACK* datapack)
{
    buffer->data[buffer->pointer] = *datapack;
    buffer->pointer++;

    if(buffer->pointer == READING_BUFFER_SIZE - 1)
        buffer->pointer = 0;
}

bool updateSystemState(int* ocs, struct mosquitto* mosq)
{
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

    bf_mask command;

    command.bitField.b0 = ocs[0];
    command.bitField.b1 = ocs[1];
    command.bitField.b2 = ocs[2];
    command.bitField.b3 = ocs[3];
    command.bitField.b4 = ocs[4];
    command.bitField.b5 = ocs[5];

    command.bitField.b6 = 0;
    command.bitField.b7 = 0;

    char formatted_command = command.entireByte;

    if(mosquitto_publish(mosq, NULL, COMM_TOPIC, sizeof(formatted_command), &formatted_command, 1, false) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Can't instruct the actuator to change its state.\n");
        return false;
    }

    return true;
}