#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdbool.h>

#include "centralniUredjaj.h"

int main() 
{
    struct sockaddr_in server_addr, client_addr;
    int server_fd, client_fd, port;
    char buffer[BUF_SIZE];

    pthread_mutex_init(&m, NULL);
    pthread_t timeoutThread;
    pthread_create(&timeoutThread, NULL, timeoutChecker, NULL);

    setupSockets(&server_addr, &server_fd, &client_fd);

    while(1) 
    {
        printDevices(devices);
        receiveMessage(&client_addr, &server_fd, buffer);
        port = parseMessage(buffer);
        sendACK(&client_addr, &client_fd, &port);

        //Dalja obrada
    }

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
        pthread_mutex_lock(&m);
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
                printf("Uredjaj \"%s\" je vec na mrezi.\n", usn);
                pthread_mutex_unlock(&m);
                return atoi(host);
                
            }
        }
        pthread_mutex_unlock(&m);

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
                //i++;
            }

            if(allowed)
            {
                Device* device = (Device*)malloc(sizeof(Device));
            
                strncpy(device->usn, usn, DATA_BUFFER_LENGTH - 1);
                strncpy(device->location, location, DATA_BUFFER_LENGTH - 1);
                strncpy(device->host, host, DATA_BUFFER_LENGTH - 1);
            
                gettimeofday(&_time, NULL);
                device->lastMessageTime = (long)_time.tv_sec;

            
            
                pthread_mutex_lock(&m);

                for(i = 0; i < MAX_DEVICES; i++)
                {
                    if(devices[i] == NULL)
                    {
                        devices[i] = device;
                        numberOfDevices++;
             
                        printf("Uredjaj \"%s\" je dodat na mrezu.\n", usn);
                        pthread_mutex_unlock(&m);
                        return atoi(host);
                    }
                }

                pthread_mutex_unlock(&m);

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
        pthread_mutex_lock(&m);
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
                pthread_mutex_unlock(&m);
                return atoi(host);
            }
        }
        pthread_mutex_unlock(&m);
    }
    
    return -1;
}


void printDevices() 
{
    printf("Aktivni uredjaji:\n");

    short c = 0;
    int i;

    pthread_mutex_lock(&m);
    for(i = 0; i < MAX_DEVICES; i++)
    {
        if(devices[i] != NULL)
        {
            printf("USN: %s Port: %s\n", devices[i]->usn, devices[i]->host);
            c++;
        }
    }
    pthread_mutex_unlock(&m);

    if(!c)
    {
        printf("Nema aktivnih uredjaja.\n");
    }
}


void* timeoutChecker()
{
    int i;

    while(1)
    {
        if(!numberOfDevices)
        {
            usleep(5 * SECOND);
            continue;
        }

        pthread_mutex_lock(&m);
        
        for(i = 0; i < MAX_DEVICES; i++)
        {
            if(devices[i] == NULL)
                continue;

            gettimeofday(&_time, NULL);
            if(_time.tv_sec - devices[i]->lastMessageTime >= TIME_UNTIL_DISCONNECT)
            {
                printf("Uredjaj \"%s\" je diskonektovan usled neaktivnosti.\n", devices[i]->usn);
                free(devices[i]);
                devices[i] = NULL;
                numberOfDevices--;     
            }
        }

        pthread_mutex_unlock(&m);

        usleep(SECOND);
    }
}