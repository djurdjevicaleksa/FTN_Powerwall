
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <mosquitto.h>
#include <stdbool.h>

#include "usluzniUredjaj.h"
#include "kontrolerUredjaj.h"
#include "topics.h"

void on_connect(struct mosquitto* mosq, void* obj, int rc)
{
    if(rc)
    {
        printf("Error: %d\n", rc);
        return;
    }

    if(mosquitto_subscribe(mosq, NULL, DATA_TOPIC, 1) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Couldn't subscribe to DATA topic.\n");
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

void on_publish(struct mosquitto* mosq, void* obj, int mid) {}

void on_subscribe(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos) {}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{

    char* topic = (char*)msg->topic;;
    char* query = (char*)msg->payload;

    char* message;

    if(strncmp(topic, ALERT_TOPIC, strlen(ALERT_TOPIC)) == 0)
    {
        //query = payload

        char* command = strtok(query, " ");
        char* state = strtok(NULL, " ");

        if(strncmp(state, ALERT_FAULT, strlen(ALERT_FAULT)) == 0)
        {
            pthread_mutex_lock(&alertMutex);

            alertMode = true;
            fprintf(stdout, "Entered ALERT mode. Service stopped.\n");

            message = "[ALERT] Kontroler acknowledges FAULT. Stopping...\n";

            if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(message), (void*)message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to RESP topic.\n");
            }

            pthread_mutex_unlock(&alertMutex);
        }
        else if(strncmp(state, ALERT_ALL_OK, strlen(ALERT_ALL_OK)) == 0)
        {
            pthread_mutex_lock(&alertMutex);

            alertMode = false;
            fprintf(stdout, "\n[ALERT] ALL_OK received. Proceeding with execution.\n");

            message = "[ALERT] Kontroler acknowledges ALL_OK. Continuing...";

            if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(message), (void*)message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to RESP topic.\n");
            }

            pthread_mutex_unlock(&alertMutex);
        }

    }

    pthread_mutex_lock(&alertMutex);

    if(alertMode)
    {
        pthread_mutex_unlock(&alertMutex);
        return;
    }

    pthread_mutex_unlock(&alertMutex);

    if(strncmp(topic, DATA_TOPIC, 4) == 0)
    {
        DATAPACK* data = (DATAPACK*)msg->payload;

        data->current_A = abs(data->current_A);
        data->current_mA = abs(data->current_mA);

        char topicArray[16];
        strcpy(topicArray, topic);
        
        
        char inaName[4];
        strncpy(inaName, &topicArray[5], 4);


        pthread_mutex_lock(&inaMutex);

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
            fprintf(stderr, "Received undefined device signature: %s\n", inaName);
        }

        pthread_mutex_unlock(&inaMutex);
    }
    else if(strncmp(topic, GETS_TOPIC, 4) == 0)
    {
        char* command = strtok(query, " ");  //GET, SET
        char* id = strtok(NULL, ".");   //Kontroler, *
        char* param = strtok(NULL, " ");  //info, state, remoteControl, systemState

	    if (id != NULL)
        {
            if ( (strncmp(id, unique_usn, strlen(unique_usn)) == 0 && strlen(id) == strlen(unique_usn)) || strncmp(id, "*", 1) == 0)
            {
                if ( strncmp(param, "info", strlen("info")) == 0 && strlen(param) == strlen("info") )
                {
                    message = readJSON("kontroler.json"); 
                }
                else
                {
                    message = "[RESP] Unknown parameter!\n";
                } 
            }
            else
            {   
                if(strcmp(id, "Senzori") == 0 || strcmp(id, "Aktuatori") == 0 || strcmp(id, "Broker") == 0)
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
    else if(strncmp(topic, SETS_TOPIC, 4) == 0)
    {	
        char* command = strtok(query, " ");  //GET, SET
        char* id = strtok(NULL, ".");   //Kontroler, *
        char* param = strtok(NULL, " ");  //info, state, remoteControl, systemState
        char* value = strtok(NULL, " "); //true, false, battery, panel, external

        if(id != NULL)
        {
            if( (strncmp(id, unique_usn, strlen(unique_usn)) == 0) && (strlen(id) == strlen(unique_usn)) )
            {
                if ( (strncmp(param, "remoteControl", strlen("remoteControl")) == 0) && (strlen(param) == strlen("remoteControl")) )
                {	
                    if (strncmp(value, "true", strlen("true")) == 0 && strlen(value) == strlen("true"))
                    {
                        pthread_mutex_lock(&remoteControlMutex);
                        remoteControl = true;
                        pthread_mutex_unlock(&remoteControlMutex);

                        message = "[RESP] remoteControl set to true.\n";
                    }
                    else if (strncmp(value, "false", strlen("false")) == 0 && strlen(value) == strlen("false"))
                    {
                        pthread_mutex_lock(&remoteControlMutex);
                        remoteControl = false;
                        pthread_mutex_unlock(&remoteControlMutex);

                        message =  "[RESP] remoteControl set to false.\n";
                    }
                    else
                    {
                        message = "[RESP] Invalid remoteControl value!\n";
                    }
  
                }
                else if (strncmp(param, "systemState", strlen("systemState")) == 0 && strlen(param) == strlen("systemState"))
                {
                    pthread_mutex_lock(&remoteControlMutex);
                    if(remoteControl)
                    {   
                        if (strncmp(value, "battery", strlen("battery")) == 0 && strlen(value) == strlen("battery"))
                        {
                            printf("[REMOTE] ");

                            pthread_mutex_lock(&systemStateMutex);
                            setSupplyBattery(mosq);
                            pthread_mutex_unlock(&systemStateMutex);

                            message = "[RESP] systemState set to battery.\n";
                        }
                        else if (strncmp(value, "panel", strlen("panel")) == 0 && strlen(value) == strlen("panel"))
                        {
                            printf("[REMOTE] ");

                            pthread_mutex_lock(&systemStateMutex);
                            setSupplySolar(mosq);
                            pthread_mutex_unlock(&systemStateMutex);
                            
                            message = "[RESP] systemState set to panel.\n";
                        }
                        else if (strncmp(value, "external", strlen("external")) == 0 && strlen(value) == strlen("external"))
                        {
                            printf("[REMOTE] ");
                            pthread_mutex_lock(&systemStateMutex);
                            setSupplyGrid(mosq);
                            pthread_mutex_unlock(&systemStateMutex);

                            message = "[RESP] systemState set to external.\n";
                        }
                        else
                        {
                            message = "[RESP] Invalid systemState value!\n";
                        }
                    }
                    else
                    {
                        message = "[RESP] remoteControl must be enabled first.\n";
                    }
                    pthread_mutex_unlock(&remoteControlMutex);
                }
                else
                {
                    message = "[RESP] Unknown parameter!\n";		    
                }

            }
            else
            {
                message = "[RESP] Unknown ID!\n";
            }

            if(mosquitto_publish(mosq, NULL, RESP_TOPIC, strlen(message), (void*)message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to RESP topic.\n");
            }
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

    pthread_mutex_init(&stdoutMutex, NULL);
    pthread_mutex_init(&inaMutex, NULL);
    pthread_mutex_init(&systemStateMutex, NULL);
    pthread_mutex_init(&remoteControlMutex, NULL);
    pthread_mutex_init(&alertMutex, NULL);

    pthread_mutex_lock(&inaMutex);
    initBuffer(&ina1Buffer);
    initBuffer(&ina2Buffer);
    initBuffer(&ina3Buffer);
    pthread_mutex_unlock(&inaMutex);

    setupSockets(&server_addr, &client_addr, &server_fd, &client_fd);


    mosquitto_lib_init();
    
    mosq = mosquitto_new(NULL, true, NULL);

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_subscribe_callback_set(mosq, on_subscribe);
    mosquitto_message_callback_set(mosq, on_message);
    mosquitto_publish_callback_set(mosq, on_publish);

    int ec = mosquitto_connect(mosq, BROKER_ADDR, BROKER_PORT, BROKER_TIMEOUT);
    if(ec != MOSQ_ERR_SUCCESS)
    {
        printf("Neuspesno povezivanje na MQTT Broker. Kod greske: %d\n", ec);
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
            pthread_mutex_lock(&stdoutMutex);
            printf("Doslo je do tajmauta servera. Ponovni pokusaj za %.01f sekundi.\n", COOLDOWN_PERIOD);
            pthread_mutex_unlock(&stdoutMutex);

            usleep(COOLDOWN_PERIOD * 1000000);
            continue;
        }
        else    //success
        {
            pthread_mutex_lock(&alertMutex);

            if(alertMode)
            {
                pthread_mutex_unlock(&alertMutex);
                usleep(SUCCESSIVE_REQUEST_PERIOD * 1000000);
                continue;
            }

            pthread_mutex_unlock(&alertMutex);

            pthread_mutex_lock(&remoteControlMutex);
            if(!remoteControl)
            {
                pthread_mutex_lock(&systemStateMutex);
                switch(systemState)
                {
                    case SPOLJNO_NAPAJANJE:
                    {
                        pthread_mutex_lock(&inaMutex);
                        DATAPACK tempDataBat = ina2Buffer.data[ina2Buffer.lastPlaced];
                        DATAPACK tempDataSol = ina1Buffer.data[ina1Buffer.lastPlaced];
                        pthread_mutex_unlock(&inaMutex);

                	//        printf("Struja punjenja baterije: %.02f\n", tempDataBat.current_mA);
		
                        if(tempDataBat.current_mA < BATTERY_CURRENT_FULL)
                        {
                            if(tempDataSol.busVoltage_V > SOLAR_VOLTAGE_HIGH)
                            {
                                setSupplySolar(mosq);
                            }
                            else
                            {
                                setSupplyBattery(mosq);
                            }
                        }

                        break;
                    }

                    case PANEL_NAPAJANJE:
                    {
                        pthread_mutex_lock(&inaMutex);
                        DATAPACK tempDataSol = ina1Buffer.data[ina1Buffer.lastPlaced];
                        pthread_mutex_unlock(&inaMutex);

                        if(tempDataSol.busVoltage_V < SOLAR_VOLTAGE_HIGH)
                        {
                            setSupplyBattery(mosq);
                        }

                        break;
                    }

                    case BATERIJA_NAPAJANJE:
                    {
                        pthread_mutex_lock(&inaMutex);
                        DATAPACK tempDataBat = ina2Buffer.data[ina2Buffer.lastPlaced];
                        pthread_mutex_unlock(&inaMutex);

                        if(tempDataBat.busVoltage_V < BATTERY_VOLTAGE_EMPTY)
                        {
                            setSupplyGrid(mosq);
                        }

                        break;
                    }

                    default: {}
                }
                pthread_mutex_unlock(&systemStateMutex);   

            }
            pthread_mutex_unlock(&remoteControlMutex);

            usleep(SUCCESSIVE_REQUEST_PERIOD * 1000000);
            

            //byebye poruka
            //sendto(client_fd, (const char *)ssdp_byebye_msg, strlen(ssdp_byebye_msg), MSG_CONFIRM, (const struct sockaddr *)&server_addr, sizeof(server_addr));
        }  
    }
    mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    pthread_mutex_destroy(&stdoutMutex);
    pthread_mutex_destroy(&inaMutex);
    pthread_mutex_destroy(&systemStateMutex);
    pthread_mutex_destroy(&remoteControlMutex);
    pthread_mutex_destroy(&alertMutex);

    pthread_cancel(checkInThread);

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
    buffer->firstFree = 0;
    buffer->lastPlaced = 0;
    memset(&buffer, 0, sizeof(buffer));
}

void addToBuffer(ReadingBuffer* buffer, DATAPACK* datapack)
{
    buffer->data[buffer->firstFree] = *datapack;
    buffer->lastPlaced = buffer->firstFree;

    buffer->firstFree++;

    if(buffer->firstFree == READING_BUFFER_SIZE)
        buffer->firstFree = 0;
}

bool updateSystemState(struct mosquitto* mosq)
{
    bf_mask command;

    command.bitField.b0 = optocouplers[0];
    command.bitField.b1 = optocouplers[1];
    command.bitField.b2 = optocouplers[2];
    command.bitField.b3 = optocouplers[3];

    command.bitField.b4 = 0;
    command.bitField.b5 = 0;
    command.bitField.b6 = 0;
    command.bitField.b7 = 0;
    
    char formatted_command = command.entireByte;

    if(mosquitto_publish(mosq, NULL, COMM_TOPIC, sizeof(formatted_command), (void*)&formatted_command, 1, false) != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Can't instruct the actuator to change its state.\n");
        return false;
    }

    return true;
}

void setSupplySolar(struct mosquitto* mosq)
{
    optocouplers[0] = LOW;
    optocouplers[1] = LOW;
    optocouplers[2] = HIGH;
    optocouplers[3] = LOW;
    
    if(!updateSystemState(mosq))
    {
        fprintf(stderr, "Critical failure. Cannot swith system state to SOLAR!\n");
        exit(EXIT_FAILURE);
    }

    systemState = PANEL_NAPAJANJE;

    pthread_mutex_lock(&stdoutMutex);
    printf("Changed systemState to PANEL_NAPAJANJE.\n");
    pthread_mutex_unlock(&stdoutMutex);
    fflush(stdout);
}
void setSupplyBattery(struct mosquitto* mosq)
{
    optocouplers[0] = LOW;
    optocouplers[1] = HIGH;
    optocouplers[2] = LOW;
    optocouplers[3] = HIGH;
    
    if(!updateSystemState(mosq))
    {
        fprintf(stderr, "Critical failure. Cannot swith system state to BATTERY!\n");
        exit(EXIT_FAILURE);
    }

    systemState = BATERIJA_NAPAJANJE;

    pthread_mutex_lock(&stdoutMutex);
    printf("Changed systemState to BATERIJA_NAPAJANJE.\n");
    pthread_mutex_unlock(&stdoutMutex);
    fflush(stdout);
}
void setSupplyGrid(struct mosquitto* mosq)
{
    optocouplers[0] = HIGH;
    optocouplers[1] = LOW;
    optocouplers[2] = LOW;
    optocouplers[3] = HIGH;
    
    if(!updateSystemState(mosq))
    {
        fprintf(stderr, "Critical failure. Cannot swith system state to GRID!\n");
        exit(EXIT_FAILURE);
    }

    systemState = SPOLJNO_NAPAJANJE;

    pthread_mutex_lock(&stdoutMutex);
    printf("Changed systemState to SPOLJNO_NAPAJANJE.\n");
    pthread_mutex_unlock(&stdoutMutex);
    fflush(stdout);
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
