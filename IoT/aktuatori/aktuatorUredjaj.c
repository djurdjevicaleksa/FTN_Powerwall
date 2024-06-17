#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <mosquitto.h>
#include <wiringPi.h>

#include "usluzniUredjaj.h"
#include "aktuatorUredjaj.h"
#include "topics.h"

void on_connect(struct mosquitto* mosq, void* obj, int rc)
{
    if(rc)
    {
        printf("Error: %d\n", rc);
        return;
    }

    if(mosquitto_subscribe(mosq, NULL, COMM_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to COMM topic.\n");
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

void on_disconnect(struct mosquitto* mosq, void* obj, int rc) {}

void on_subscribe(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos) {}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
    char* topic = (char*)msg->topic;;
    char* query = (char*)msg->payload;

    char* message;

    if(strncmp(topic, GETS_TOPIC, 4) == 0)
    {  
        char* command = strtok(query, " ");  //GET
        char* id = strtok(NULL, ".");   //Aktuatori, *
        char* param = strtok(NULL, " ");  //info, state
        
        if (id != NULL)
        {
            if ( (strncmp(id, unique_usn, strlen(unique_usn)) == 0 && strlen(id) == strlen(unique_usn)) || strncmp(id, "*", 1) == 0)
            {
                if ( strncmp(param, "info", strlen("info")) == 0 && strlen(param) == strlen("info") )
                {
                    message = readJSON("aktuatori.json");  
                }
                else
                {
                    message = "[RESP] Unknown parameter!\n";
                }
            }
            else
            {   
                if(strcmp(id, "Senzori") == 0 || strcmp(id, "Kontroler") == 0 || strcmp(id, "Broker") == 0)
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
    else if(strncmp(topic, COMM_TOPIC, 4) == 0)
    {
        char* comm = ((char*)msg->payload);
        
  	    char command = (char)(((bf_mask*)comm)->entireByte);
	    lastIssuedCommand = command;
	    commandChanged = true;
    }
    else if(strncmp(topic, ALERT_TOPIC, 4) == 0)
    {

        char* command = strtok(query, " ");
        char* state = strtok(NULL, " ");

        if(strncmp(state, ALERT_FAULT, strlen(ALERT_FAULT)) == 0)
        {
            //alert je, obustavi
            pthread_mutex_lock(&alertMutex);

            alertMode = true;
            fprintf(stdout, "Entered ALERT mode. Service stopped.\n");

            message = "[ALERT] Aktuator acknowledges FAULT. Stopping...\n";

            if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(message), (void*)message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to RESP topic.\n");
            }

            pthread_mutex_unlock(&alertMutex);    
        }
    }
    else if(strncmp(topic, CHECK_TOPIC, 5) == 0)
    {
        pthread_mutex_lock(&stdoutMutex);

        fprintf(stdout, "\n%s\n", query);
        fflush(stdout);

        pthread_mutex_unlock(&stdoutMutex);
    }
}

int main() 
{
    struct sockaddr_in server_addr, client_addr;
    int client_fd, server_fd;
    char buffer[BUF_SIZE];
    socklen_t client_addr_len = sizeof(client_addr);

    pthread_mutex_init(&alertMutex, NULL);
    pthread_mutex_init(&stdoutMutex, NULL);

    setupSockets(&server_addr, &client_addr, &server_fd, &client_fd);


    wiringPiSetup();
    pinMode(OK1, OUTPUT);
    pinMode(OK2, OUTPUT);
    pinMode(OK3, OUTPUT);
    pinMode(OK4, OUTPUT);

    
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

                digitalWrite(OK1, LOW);
                digitalWrite(OK2, LOW);
                digitalWrite(OK3, LOW);
                digitalWrite(OK4, LOW);
                
                printf("Critical error encountered. Execution halted.\n");
                usleep(500000);
                exit(6);
            }

            pthread_mutex_unlock(&alertMutex);

            if(commandChanged)
            {
                digitalWrite(OK1, lastIssuedCommand &    0b1     ? HIGH : LOW);
                digitalWrite(OK2, lastIssuedCommand & (0b1 << 1) ? HIGH : LOW);
                digitalWrite(OK3, lastIssuedCommand & (0b1 << 2) ? HIGH : LOW);
                digitalWrite(OK4, lastIssuedCommand & (0b1 << 3) ? HIGH : LOW);

                commandChanged = false;
            }

            usleep(SUCCESSIVE_REQUEST_PERIOD * 1000000); 
        }

        //byebye poruka
        //sendto(client_fd, (const char *)ssdp_byebye_msg, strlen(ssdp_byebye_msg), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
    }

    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    pthread_mutex_destroy(&alertMutex);
    pthread_mutex_destroy(&stdoutMutex);

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
        if(mosquitto_publish((struct mosquitto*)mosq, NULL, CHECK_TOPIC, strlen(checkingInMessage), (void*)checkingInMessage, 1, false) != MOSQ_ERR_SUCCESS)
        {
            fprintf(stderr, "Couldn't alert the system of my presence.\n");
        }

        usleep(CHECKING_IN_PERIOD * 1000000);
    }
}
