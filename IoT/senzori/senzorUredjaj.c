#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <mosquitto.h>
#include <time.h>
#include <signal.h>

#include "usluzniUredjaj.h"
#include "senzorUredjaj.h"
#include "topics.h"
#include "lecaina219.h"

void on_connect(struct mosquitto* mosq, void* obj, int rc)
{
    if(rc)
    {
        printf("Error: %d\n", rc);
        return;
    }

    if(mosquitto_subscribe(mosq, NULL, GETS_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to GETS topic.\n");
    }

    if(mosquitto_subscribe(mosq, NULL, SETS_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to SETS topic.\n");
    }

    if(mosquitto_subscribe(mosq, NULL, ALERT_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to ALERT topic.\n");
    }

    if(mosquitto_subscribe(mosq, NULL, CHECK_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to CHECK topic.\n");
    }
}

void on_subscribe(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos) {}

void on_publish(struct mosquitto* mosq, void* obj, int rc) {}

void on_disconnect(struct mosquitto* mosq, void* obj, int rc) {}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
    char* topic = (char*)msg->topic;;
    char* query = (char*)msg->payload;

    if(strncmp(topic, CHECK_TOPIC, 5) == 0)
    {
        pthread_mutex_lock(&stdoutMutex);
        fprintf(stdout, "\n%s\n", query);
        fflush(stdout);

        pthread_mutex_unlock(&stdoutMutex);
    }

    char* command = strtok(query, " ");  //GET
    char* id = strtok(NULL, ".");   // Senzori, *
    char* param = strtok(NULL, " ");  //info, s

    char* message;

    if(strncmp(topic, GETS_TOPIC, 4) == 0)
    {
        if(id != NULL)
        {
            if ( (strncmp(id, unique_usn, strlen(unique_usn)) == 0 && strlen(id) == strlen(unique_usn)) || strncmp(id, "*", 1) == 0)
            {
                if ( strncmp(param, "info", strlen("info")) == 0 && strlen(param) == strlen("info") )
                {
                    message = readJSON("senzori.json"); 
                }
                else
                {
                    message = "[RESP] Unknown parameter!\n";
                }
            }
            else
            {
                if(strcmp(id, "Aktuatori") == 0 || strcmp(id, "Kontroler") == 0 || strcmp(id, "Broker") == 0)
                {
                    return;
                }

                message = "[RESP] Unknown id!\n";
            }

            if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(message), (void*)message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to RESP topic.\n");
            }
        }
    }
    else if(strncmp(topic, ALERT_TOPIC, strlen(ALERT_TOPIC)) == 0)
    {

        if(strncmp(id, ALERT_FAULT, strlen(ALERT_FAULT)) == 0)
        {
            //alert je, obustavi
            pthread_mutex_lock(&alertMutex);

            alertMode = true;
            fprintf(stdout, "Entered ALERT mode. Service stopping...\n");

            message = "[ALERT] Senzori acknowledges FAULT. Stopping...\n";

            if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(message), (void*)message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to RESP topic.\n");
            }

            pthread_mutex_unlock(&alertMutex);       
        }
    } 
}

int main() 
{
    struct sockaddr_in server_addr, client_addr;
    int client_fd, server_fd;
    char buffer[BUF_SIZE];
    socklen_t client_addr_len = sizeof(client_addr);

    setupSockets(&server_addr, &client_addr, &server_fd, &client_fd);

    pthread_mutex_init(&alertMutex, NULL);
    pthread_mutex_init(&stdoutMutex, NULL);

    mosquitto_lib_init();
    struct mosquitto* mosq;
    mosq = mosquitto_new(NULL, true, NULL);

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_subscribe_callback_set(mosq, on_subscribe);
    mosquitto_message_callback_set(mosq, on_message);
    mosquitto_publish_callback_set(mosq, on_publish);

    int crc = mosquitto_connect(mosq, BROKER_ADDR, BROKER_PORT, BROKER_TIMEOUT);

    if(crc != MOSQ_ERR_SUCCESS)
    {
        printf("Neuspesno povezivanje na MQTT Broker. Kod greske: %d\n", crc);
        exit(EXIT_FAILURE);
    }

    LecaINA219* ina1 = createINA(0.1, 2);
    inaI2CInit(ina1, 0x40, "/dev/i2c-1");
    inaConfigure(ina1, CONFIG_VOLTAGE_32V, CONFIG_GAIN_8, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_MODE_SHUNT_BUS_CONT);
    
    LecaINA219* ina2 = createINA(0.1, 2);
    inaI2CInit(ina2, 0x4c, "/dev/i2c-1");
    inaConfigure(ina2, CONFIG_VOLTAGE_32V, CONFIG_GAIN_8, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_MODE_SHUNT_BUS_CONT);
    
    LecaINA219* ina3 = createINA(0.1, 2);
    inaI2CInit(ina3, 0x48, "/dev/i2c-1");
    inaConfigure(ina3, CONFIG_VOLTAGE_32V, CONFIG_GAIN_8, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_ADC_12BIT_4SAMPLES, CONFIG_MODE_SHUNT_BUS_CONT);
    
    DATAPACK datapack1;
    DATAPACK datapack2;
    DATAPACK datapack3;

    mosquitto_loop_start(mosq);

    pthread_t checkInThread;
    pthread_create(&checkInThread, NULL, checkingIn, (void*)mosq);
    
    while(1)
    {
        sendto(client_fd, (const char *)ssdp_alive_msg, strlen(ssdp_alive_msg), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        
        int success = recvfrom(server_fd, (char*)&buffer, BUF_SIZE, MSG_WAITALL, (struct sockaddr *)&client_addr, &client_addr_len);
        
        if(success < 0) //timeout ili greska
        { 
            printf("Doslo je do tajmauta servera. Ponovni pokusaj za %.01f sekundi.\n", COOLDOWN_PERIOD);
            usleep(COOLDOWN_PERIOD * 1000000);
            continue;
        }
        else    //success
        {

            pthread_mutex_lock(&alertMutex);

            if(alertMode)
            {
                pthread_mutex_unlock(&alertMutex);

                printf("Critical error encountered. Execution halted.\n");
                usleep(500000);
                exit(6);
            }

            pthread_mutex_unlock(&alertMutex);


            inaGetData(ina1, SENSOR_WANTED_DATA, &datapack1);
            inaGetData(ina2, SENSOR_WANTED_DATA, &datapack2);
            inaGetData(ina3, SENSOR_WANTED_DATA, &datapack3);
/*
            printf("INA1\n");
            printDatapack(&datapack1);
            printf("\nINA2\n");
            printDatapack(&datapack2);
            printf("\nINA3\n");
            printDatapack(&datapack3);
            printf("\n\n");
*/
            if(mosquitto_publish(mosq, NULL, INA1_TOPIC, sizeof(datapack1), (void*)&datapack1, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish INA1 DATA");
            }

            if(mosquitto_publish(mosq, NULL, INA2_TOPIC, sizeof(datapack2), (void*)&datapack2, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish INA2 DATA");
            }

            if(mosquitto_publish(mosq, NULL, INA3_TOPIC, sizeof(datapack3), (void*)&datapack3, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish INA3 DATA");
            }

            memset(&datapack1, -1, sizeof(DATAPACK));
            memset(&datapack2, -1, sizeof(DATAPACK));
            memset(&datapack3, -1, sizeof(DATAPACK));

            usleep(SUCCESSIVE_REQUEST_PERIOD * 1000000);

            //byebye poruka
            //sendto(client_fd, (const char *)ssdp_byebye_msg, strlen(ssdp_byebye_msg), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        }  
    }

    close(server_fd);
    close(client_fd);

    pthread_mutex_destroy(&alertMutex);
    pthread_mutex_destroy(&stdoutMutex);

    pthread_cancel(checkInThread);

    mosquitto_loop_stop(mosq, true);
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

char* readJSON(const char* filename) 
{
    FILE *file = fopen(filename, "r");

    if (!file) 
    {
        fprintf(stderr, "Error: Could not open JSON file.\n");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_buffer = (char *)malloc(file_size + 1);

    if (!json_buffer) 
    {
        fprintf(stderr, "Error: Out of memory.\n");
        fclose(file);
        return NULL;
    }

    fread(json_buffer, 1, file_size, file);
    fclose(file);

    json_buffer[file_size] = '\0';

    return json_buffer;
}

void* checkingIn(void* mosq)
{
    while(1)
    {   
        pthread_mutex_lock(&stdoutMutex);

        if(mosquitto_publish((struct mosquitto*)mosq, NULL, CHECK_TOPIC, strlen(checkingInMessage), (void*)checkingInMessage, 1, false) != MOSQ_ERR_SUCCESS)
        {
            fprintf(stderr, "Couldn't alert the system of my presence.\n");
        }

        pthread_mutex_unlock(&stdoutMutex);

        usleep(CHECKING_IN_PERIOD * 1000000);

        
    }
}