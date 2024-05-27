#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <mosquitto.h>
#include <time.h>
#include <signal.h>

#include "usluzniUredjaj.h"
#include "senzorUredjaj.h"
#include "topics.h"
#include "lecaina219.h"

void on_connect(struct mosquitto* mosq, void* obj, int rc)
{
    printf("Connected!\n");
}

void on_publish(struct mosquitto* mosq, void* obj, int rc)
{
    printf("Published!\n");
}

void on_disconnect(struct mosquitto* mosq, void* obj, int rc)
{
    printf("Disconnected!\n");
}

void on_message(struct mosquitto* mosq, void* obj, struct mosquitto_message* msg)
{
    const char* topic = msg->topic;
    const char* query = (const char*)msg->payload;

    if(strncmp(topic, GETS_TOPIC, 5) == 0)
    {
        int i;
        int c = 0;

        for(i = 0; i < strlen(query); i++)
        {
            if(query[i] != ' ')
            {
                c++;
            }
            else
                break;
            
        }

        if(strlen(unique_usn) != c - 1)
            return;

        for(i = 0; i < c; i++)
        {
            if(unique_usn[i] != query[i])
                return;
        }

        //ovde znamo da se obratio nama
        char target_data[64];
        int a = 0;

        for(i = c + 1; i < strlen(query); i++)
        {
            target_data[a] = query[i];
            a++;
        }

        target_data[a] = '\n';

        //sad znamo koji podatak trazi





        //TO DO
        //napisati jebenu .find() funkciju koja nalazi prvi put kad se neki karakter pojavljuje u datom stringu, da se resimo
        //ove bede od for petlji!!!!!!!!!
        

        
    }
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
    mosquitto_publish_callback_set(mosq, on_publish);

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

    LecaINA219* ina1 = createINA(0.1, 2);
    inaI2CInit(ina1, I2C_ADDR0, "/dev/i2c-1");
    inaConfigure(ina1, CONFIG_VOLTAGE_32V, CONFIG_GAIN_8, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_MODE_SHUNT_BUS_CONT);
    
    LecaINA219* ina2 = createINA(0.1, 2);
    inaI2CInit(ina2, I2C_ADDR1, "/dev/i2c-1");
    inaConfigure(ina2, CONFIG_VOLTAGE_32V, CONFIG_GAIN_8, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_MODE_SHUNT_BUS_CONT);
    
    LecaINA219* ina3 = createINA(0.1, 2);
    inaI2CInit(ina3, I2C_ADDR2, "/dev/i2c-1");
    inaConfigure(ina3, CONFIG_VOLTAGE_32V, CONFIG_GAIN_8, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_MODE_SHUNT_BUS_CONT);
    
    DATAPACK datapack1;
    DATAPACK datapack2;
    DATAPACK datapack3;

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
            usleep(SUCCESSIVE_REQUEST_PERIOD * 1000000);

            inaGetData(ina1, SENSOR_WANTED_DATA, &datapack1);
            inaGetData(ina2, SENSOR_WANTED_DATA, &datapack2);
            inaGetData(ina3, SENSOR_WANTED_DATA, &datapack3);

            if(mosquitto_publish(mosq, NULL, INA1_TOPIC, sizeof(datapack1), (void*)&datapack1, 1, true) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish INA1 DATA");
            }

            if(mosquitto_publish(mosq, NULL, INA2_TOPIC, sizeof(datapack2), (void*)&datapack2, 1, true) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish INA2 DATA");
            }

            if(mosquitto_publish(mosq, NULL, INA3_TOPIC, sizeof(datapack3), (void*)&datapack3, 1, true) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish INA3 DATA");
            }

            memset(&datapack1, -1, sizeof(DATAPACK));
            memset(&datapack2, -1, sizeof(DATAPACK));
            memset(&datapack3, -1, sizeof(DATAPACK));


            //byebye poruka
            //sendto(client_fd, (const char *)ssdp_byebye_msg, strlen(ssdp_byebye_msg), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        }  
    }

    close(server_fd);
    close(client_fd);

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

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