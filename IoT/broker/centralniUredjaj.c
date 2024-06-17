#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdbool.h>
#include <mosquitto.h>

#include "centralniUredjaj.h"
#include "topics.h"

void on_connect(struct mosquitto* mosq, void* obj, int rc)
{
    if(rc)
    {
        printf("Error: %d\n", rc);
        return;
    }

    if(mosquitto_subscribe(mosq, NULL, GETS_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to GROUP topic.\n");
    }
}

void on_disconnect(struct mosquitto* mosq, void* obj, int rc) {}

void on_subscribe(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos) {}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
    char* topic = (char*)msg->topic;;
    char* query = (char*)msg->payload;  //GET Broker.groups

    char* message;

    if(strncmp(topic, GETS_TOPIC, 4) == 0)
    {  
        char* command = strtok(query, " ");  //GET
        char* id = strtok(NULL, ".");   //Broker
        char* param = strtok(NULL, " ");  //groups
        
        if (id != NULL)
        {
            if ( (strncmp(id, "Broker", strlen("Broker")) == 0 && strlen(id) == strlen("Broker")) )
            {
                if ( strncmp(param, "groups", strlen("groups")) == 0 && strlen(param) == strlen("groups") )
                {
                    printGroups(mosq);  //posalji grupe
                }
                else
                {
                    message = "[RESP] Unknown parameter!\n";

                    if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(message), (void*)message, 1, false) != MOSQ_ERR_SUCCESS)
                    {
                        fprintf(stderr, "Couldn't publish to RESP topic.\n");
                    }  
                }
            }
            else
            {
                if(strcmp(id, "Senzori") == 0 || strcmp(id, "Kontroler") == 0 || strcmp(id, "Aktuatori") == 0)
                {
                    return;
                }
                message = "[RESP] Unknown id!\n";

                if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(message), (void*)message, 1, false) != MOSQ_ERR_SUCCESS)
                {
                    fprintf(stderr, "Couldn't publish to RESP topic.\n");
                }  
            }
        }
    }
}


int main() 
{
    struct sockaddr_in server_addr, client_addr;
    int server_fd, client_fd, port;
    char buffer[BUF_SIZE];

    pthread_mutex_init(&m, NULL);
    pthread_mutex_init(&groupMutex, NULL);

    pthread_t timeoutThread;
    pthread_create(&timeoutThread, NULL, timeoutChecker, NULL);

    setupSockets(&server_addr, &server_fd, &client_fd);


    mosquitto_lib_init();
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


    while(1) 
    {
        printDevices(devices);
        receiveMessage(&client_addr, &server_fd, buffer);
        port = parseMessage(buffer);
        sendACK(&client_addr, &client_fd, &port);
    }

    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    pthread_mutex_destroy(&m);
    pthread_mutex_destroy(&groupMutex);

    close(server_fd);
    close(client_fd);
    pthread_join(timeoutThread, NULL);

    return 0;
}

void setupSockets(struct sockaddr_in* server_addr, int* server_fd, int* client_fd)
{
    //Soket za receive
    if ((*server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) 
    {
        perror("Neuspesno pravljenje soketa. (S)\n");
        exit(EXIT_FAILURE);
    }

    memset(server_addr, 0, sizeof(*server_addr));

    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr->sin_port = htons(SSDP_PORT);

    if (bind(*server_fd, (const struct sockaddr *)server_addr, sizeof(*server_addr)) == -1) 
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

    //Soket za send
    if ((*client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) 
    {
        perror("Neuspesno pravljenje soketa. (C)\n");
        exit(EXIT_FAILURE);
    }
}


void receiveMessage(struct sockaddr_in* client_addr, int* server_fd, char* buffer)
{
    socklen_t client_addr_len = sizeof(client_addr);
    recvfrom(*server_fd, (char *)buffer, BUF_SIZE, MSG_WAITALL, (struct sockaddr *)client_addr, &client_addr_len);
}

void sendACK(struct sockaddr_in* client_addr, int* client_fd, int* port)
{
    client_addr->sin_port = htons(*port);
    client_addr->sin_addr.s_addr = inet_addr(SSDP_ADDRESS);
    sendto(*client_fd, (const char *)ACK, strlen(ACK), MSG_CONFIRM, (const struct sockaddr *)client_addr, sizeof(*client_addr));
}

int parseMessage(const char *message) 
{
    char *lines[MAX_LINES];
    char *line;

    int i;
    bool allowed;

    //Deli poruku po linijama
    int num_lines = 0;
    char *copy = strdup(message);
    line = strtok(copy, "\r\n");
    while (line != NULL && num_lines < MAX_LINES) 
    {
        lines[num_lines++] = line;
        line = strtok(NULL, "\r\n");
    }

    free(copy);

    //Razmotri svaku liniju
    char usn[DATA_BUFFER_LENGTH] = "";
    char location[DATA_BUFFER_LENGTH] = "";
    char notificationType[DATA_BUFFER_LENGTH] = "";
    char host[DATA_BUFFER_LENGTH] = "";

    for (int i = 0; i < num_lines; i++) 
    {
        if (strstr(lines[i], "USN:") != NULL) 
        {
            strncpy(usn, lines[i] + strlen("USN: "), sizeof(usn)-1);
        } 
        else if (strstr(lines[i], "LOCATION:") != NULL) 
        {
            strncpy(location, lines[i] + strlen("LOCATION: "), sizeof(location)-1);
        }
        else if (strstr(lines[i], "NTS:") != NULL) 
        {
            strncpy(notificationType, lines[i] + strlen("NTS: "), sizeof(notificationType)-1);
        }
        else if (strstr(lines[i], "HOST:") != NULL) 
        { 
            strncpy(host, lines[i] + strlen("HOST: "), sizeof(host)-1);
        }
    }

    //Deluj spram tipa poruke
    if (!strcmp(notificationType, "ssdp:alive")) 
    {
        //pthread_mutex_lock(&m);
        //Proveri da li taj uredjaj vec postoji
        for (int i = 0; i < MAX_DEVICES; i++) 
        {
            if(devices[i] == NULL)
                continue;

             // Postoji
            if (!strcmp(devices[i]->usn, usn))
            {
                //Updateuj time stamp
                gettimeofday(&_time, NULL);
                devices[i]->lastMessageTime = (long)_time.tv_sec;
                //printf("Uredjaj \"%s\" je vec na mrezi.\n", usn);
                //pthread_mutex_unlock(&m);
                return atoi(host);
                
            }
        }
        //pthread_mutex_unlock(&m);

        //Ne postoji, dodaj novi uredjaj
        if (numberOfDevices < MAX_DEVICES) 
        {   
            allowed = false;

            for(i = 0; i < MAX_DEVICES; i++)
            {
                if(!strcmp(allowedDevices[i], usn))
                {
                    allowed = true;
                    break;
                }
            }

            if(allowed)
            {
                Device* device = (Device*)malloc(sizeof(Device));
            
                strncpy(device->usn, usn, DATA_BUFFER_LENGTH - 1);
                strncpy(device->location, location, DATA_BUFFER_LENGTH - 1);
                strncpy(device->host, host, DATA_BUFFER_LENGTH - 1);
            
                gettimeofday(&_time, NULL);
                device->lastMessageTime = (long)_time.tv_sec;

            
            
                //pthread_mutex_lock(&m);

                for(i = 0; i < MAX_DEVICES; i++)
                {
                    if(devices[i] == NULL)
                    {
                        devices[i] = device;
                        numberOfDevices++;
             
                        printf("Uredjaj \"%s\" je dodat na mrezu.\n", usn);
                        break;
                    }
                }

                if(!(strncmp(device->usn, "Senzor", strlen("Senzor"))))
                {
                    for(i = 0; i < MAX_SENZORI; i++)
                    {
                        if(groupSenzori[i] != NULL)
                        {
                            if(strcmp(groupSenzori[i]->usn, usn) == 0)
                            {
                                return atoi(host);
                            }
                        }  
                    }
                        
                    for(i = 0; i < MAX_SENZORI; i++)
                    {
                        if(groupSenzori[i] == NULL)
                        {
                            groupSenzori[i] = device;
                            break;
                        }
                    }
                }
                else if(!(strncmp(device->usn, "Aktuator", strlen("Aktuator"))))
                {
                    for(i = 0; i < MAX_AKTUATORI; i++)
                    {
                        if(groupAktuatori[i] != NULL)
                        {
                            if(strcmp(groupAktuatori[i]->usn, usn) == 0)
                            {
                                return atoi(host);
                            }
                        }
                    }

                    for(i = 0; i < MAX_AKTUATORI; i++)
                    {
                        if(groupAktuatori[i] == NULL)
                        {
                            groupAktuatori[i] = device;
                            break;
                        }
                    }
                }
                else if(!(strncmp(device->usn, "Kontroler", strlen("Kontroler"))))
                {
                    for(i = 0; i < MAX_KONTROLERI; i++)
                    {
                        if(groupKontroleri[i] != NULL)
                        {
                            if(strcmp(groupKontroleri[i]->usn, usn) == 0)
                            {
                                return atoi(host);
                            }
                        }
                    }

                    for(i = 0; i < MAX_KONTROLERI; i++)
                    {
                        if(groupKontroleri[i] == NULL)
                        {
                            groupKontroleri[i] = device;
                            break;
                        }
                    }
                }
                

                //pthread_mutex_unlock(&m);

                return atoi(host);

            }
            else
            {
                printf("Uredjaj \"%s\" nema dozvolu pristupa.\n", usn);
            }

            
        } 
        else
        {
            printf("Uredjaju \"%s\" odbijen pristup usled zasicenosti mreze.\n", usn);
        }
    } 
    else if (!strcmp(notificationType, "ssdp:byebye")) 
    {
        //pthread_mutex_lock(&m);
        //Izbaci uredjaj sa mreze usled byebye poruke
        for (int i = 0; i < MAX_DEVICES; i++) 
        {
            if(devices[i] == NULL)
                continue;
            
            if (!strcmp(devices[i]->usn, usn)) 
            {
                printf("Uredjaj \"%s\" se odjavio sa mreze. (ssdp:byebye)\n", usn);
                free(devices[i]);
                devices[i] = NULL;
                
                numberOfDevices--;
                //pthread_mutex_unlock(&m);
                return atoi(host);
            }
        }
        //pthread_mutex_unlock(&m);
    }
    
    return -1;
}

void printDevices() 
{
    printf("\n\nAktivni uredjaji:\n");

    short c = 0;
    int i;

    //pthread_mutex_lock(&m);
    for(i = 0; i < MAX_DEVICES; i++)
    {
        if(devices[i] != NULL)
        {
            printf("USN: %s Port: %s\n", devices[i]->usn, devices[i]->host);
            c++;
        }
    }
    //pthread_mutex_unlock(&m);

    if(!c)
    {
        printf("Nema aktivnih uredjaja.\n");
    }

    printf("\n\n");
}

void printGroups(struct mosquitto* mosq) 
{
    char groupInfo[BUF_SIZE];
    memset(groupInfo, '\0', sizeof(groupInfo));

    strcat(groupInfo, "Senzori:\n");

    int c = 0;

    //pthread_mutex_lock(&groupMutex);

    for(int i = 0; i < MAX_SENZORI; i++)
    {
        if(groupSenzori[i] != NULL)
        {
            char buffer[128];
            snprintf(buffer, sizeof(buffer),  "USN: %s Port: %s\n", groupSenzori[i]->usn, groupSenzori[i]->host);
            strcat(groupInfo, buffer);
            c++;
        }
    }

    if(!c)
        strcat(groupInfo, "No devices present in this group.\n\n");

    

    strcat(groupInfo, "\nKontroleri:\n");

    c = 0;

    for(int i = 0; i < MAX_KONTROLERI; i++)
    {
        if(groupKontroleri[i] != NULL)
        {
            char buffer[128];
            snprintf(buffer, sizeof(buffer),  "USN: %s Port: %s\n", groupKontroleri[i]->usn, groupKontroleri[i]->host);
            strcat(groupInfo, buffer);
            c++;
        }
    }

    if(!c)
        strcat(groupInfo, "No devices present in this group.\n\n");

    strcat(groupInfo, "\nAktuatori:\n");

    c = 0;

    for(int i = 0; i < MAX_AKTUATORI; i++)
    {
        if(groupAktuatori[i] != NULL)
        {
            char buffer[128];
            snprintf(buffer, sizeof(buffer),  "USN: %s Port: %s\n", groupAktuatori[i]->usn, groupAktuatori[i]->host);
            strcat(groupInfo, buffer);
            c++;
        }
    }

    //pthread_mutex_unlock(&groupMutex);

    if(!c)
        strcat(groupInfo, "No devices present in this group.\n\n");

    
    groupInfo[strlen(groupInfo) - 1] = '\0';


    if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(groupInfo), (void*)groupInfo, 1, false) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't publish to RESP topic.\n");
    }    
}

void* timeoutChecker()
{
    int i, j;

    while(1)
    {
        if(!numberOfDevices)
        {
            usleep(5 * SECOND);
            continue;
        }

        //pthread_mutex_lock(&m);
        
        for(i = 0; i < MAX_DEVICES; i++)
        {
            if(devices[i] == NULL)
                continue;

            gettimeofday(&_time, NULL);
            if(_time.tv_sec - devices[i]->lastMessageTime >= TIME_UNTIL_DISCONNECT)
            {
                printf("Uredjaj \"%s\" je diskonektovan usled neaktivnosti.\n", devices[i]->usn);
                
                if(!strncmp(devices[i]->usn, "Senzor", strlen("Senzor")))
                {
                    for(j = 0; j < MAX_SENZORI; j++)
                    {
                        if(groupSenzori[j] != NULL)
                        {
                            if(!strcmp(groupSenzori[j]->usn, devices[i]->usn))
                            {
                                free(devices[i]);

                                groupSenzori[j] = NULL;
                                devices[i] = NULL;

                                numberOfDevices--;
                                break;
                            }
                        }
                    }
                }
                else if(!strncmp(devices[i]->usn, "Kontroler", strlen("Kontroler")))
                {
                    for(j = 0; j < MAX_KONTROLERI; j++)
                    {
                        if(groupKontroleri[j] != NULL)
                        {
                            if(!strcmp(groupKontroleri[j]->usn, devices[i]->usn))
                            {
                                free(devices[i]);

                                groupKontroleri[j] = NULL;
                                devices[i] = NULL;

                                numberOfDevices--;
                                break;
                            }
                        }
                    }
                }
                else if(!strncmp(devices[i]->usn, "Aktuator", strlen("Aktuator")))
                {
                    for(j = 0; j < MAX_AKTUATORI; j++)
                    {
                        if(groupAktuatori[j] != NULL)
                        {
                            if(!strcmp(groupAktuatori[j]->usn, devices[i]->usn))
                            {
                                free(devices[i]);

                                groupAktuatori[j] = NULL;
                                devices[i] = NULL;

                                numberOfDevices--;
                                break;
                            }
                        }
                    }
                }
            }
        }

        //pthread_mutex_unlock(&m);

        usleep(SECOND);
    }
}