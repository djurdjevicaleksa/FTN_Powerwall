#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mosquitto.h>
#include <unistd.h>
#include "topics.h"
#include "mqtt_app.h"

int main()
{
    char message[BUF_LEN];
    int rc;

    mosquitto_lib_init();

    struct mosquitto* mosq = mosquitto_new(NULL, true, NULL);

    if (!mosq)
    {
        fprintf(stderr, "Failed to create mosquitto instance.\n");
        exit(EXIT_FAILURE);
    }

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_publish_callback_set(mosq, on_publish);
    mosquitto_subscribe_callback_set(mosq, on_subscribe);
    mosquitto_message_callback_set(mosq, on_message);

    rc = mosquitto_connect(mosq, BROKER_ADDR, BROKER_PORT, BROKER_TIMEOUT);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Failed to connect to MQTT Broker. Error code: %d\n", rc);
        exit(EXIT_FAILURE);
    }

    mosquitto_loop_start(mosq);

    while(1)
    {
        usleep(500000);
        printf("\n> ");
        if (fgets(message, sizeof(message), stdin) == NULL)
        {
            fprintf(stderr, "Error reading input.\n");
            break;
        }

        size_t len = strlen(message);

        if (len > 0 && message[len - 1] == '\n')
        {
            message[len - 1] = '\0';
        }

        if (!strncmp(message, "GET", 3))
        {
            if (mosquitto_publish(mosq, NULL, GETS_TOPIC, strlen(message), message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to GETS topic.\n");
            }

            pthread_mutex_lock(&mutex);
            while (!message_received)
            {
                pthread_cond_wait(&cond, &mutex);
            }
            message_received = 0;
            pthread_mutex_unlock(&mutex);   
        }
        else if (!strncmp(message, "SET", 3))
        {
            if (mosquitto_publish(mosq, NULL, SETS_TOPIC, strlen(message), message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to SETS topic.\n");
            }

            pthread_mutex_lock(&mutex);
            while (!message_received)
            {
                pthread_cond_wait(&cond, &mutex);
            }
            message_received = 0;
            pthread_mutex_unlock(&mutex);
        }
        else if (!strncmp(message, "exit", 4))
        {
            break;
        }
        else if (!strncmp(message, "man", 4))
        {
            printf("Command format:\n");
            printf("GET <deviceID>.<param> - gets desired info\n");
            printf("SET <deviceID>.<param> <value> - sets desired parameter\n");
            printf("Exit - quits program\n");
        }
        else if(!strncmp(message, "ALERT", 5))
        {
            if (mosquitto_publish(mosq, NULL, ALERT_TOPIC, strlen(message), message, 1, false) != MOSQ_ERR_SUCCESS)
            {
                fprintf(stderr, "Couldn't publish to ALERT topic.\n");
            }
        }
        else
        {
            printf("Invalid command!\n");
        }

        
    }

    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}

void on_connect(struct mosquitto* mosq, void* obj, int rc)
{
    if (rc == 0)
    {
        if (mosquitto_subscribe(mosq, NULL, RESP_TOPIC, 1) != MOSQ_ERR_SUCCESS)
        {
            fprintf(stderr, "Couldn't subscribe to RESP topic.\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to connect, return code %d\n", rc);
    }
}

void on_publish(struct mosquitto* mosq, void* obj, int rc) {}

void on_subscribe(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos) {}

void on_disconnect(struct mosquitto* mosq, void* obj, int rc) {}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
    if (msg->retain)
    {
        return;
    }

    pthread_mutex_lock(&mutex);
    printf("%s\n", (char*)msg->payload);
    message_received = 1;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}
