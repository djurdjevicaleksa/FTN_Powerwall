#ifndef _TOPICS_H_
#define _TOPICS_H_

#define SENSOR_TOPIC        "ftn_powerwall/senzor/"
#define ACTUATOR_TOPIC      "ftn_powerwall/aktuator/"
#define CONTROLLER_TOPIC    "ftn_powerwall/kontroler/"

#define INA1_TOPIC "ftn_powerwall/senzor/INA1"
#define INA2_TOPIC "ftn_powerwall/senzor/INA2"
#define INA3_TOPIC "ftn_powerwall/senzor/INA3"

#define RELAY1_TOPIC PROJECT_TOPIC + ACTUATOR_TOPIC + "RELAY1"
#define RELAY2_TOPIC PROJECT_TOPIC + ACTUATOR_TOPIC + "RELAY2"

#define SENSOR_WANTED_DATA 0b00011001

#define BROKER_ADDR "147.91.161.149"
#define BROKER_PORT 2525
#define BROKER_TIMEOUT 10

#endif //_TOPICS_H_