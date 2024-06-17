#ifndef _MQTT_APP_H_
#define _MQTT_APP_H_

#define BUF_LEN 64

struct mosquitto* mosq;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int message_received = 0;

void on_connect(struct mosquitto*, void*, int);
void on_publish(struct mosquitto*, void*, int );
void on_subscribe(struct mosquitto*, void*, int, int, const int*);
void on_message(struct mosquitto*, void*, const struct mosquitto_message*);
void on_disconnect(struct mosquitto*, void*, int);


#endif //_MQTT_APP_H_