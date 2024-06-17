#ifndef _TOPICS_H_
#define _TOPICS_H_

#define GETS_TOPIC "gets"
#define SETS_TOPIC "sets"
#define DATA_TOPIC "data"
#define COMM_TOPIC "comm"
#define RESP_TOPIC "resp"
#define TEST_TOPIC "test"
#define ALERT_TOPIC "alert"
#define CHECK_TOPIC "check"
#define GROUP_TOPIC "group"

#define ALERT_FAULT "FAULT"
#define ALERT_ALL_OK "OK"

#define INA1_TOPIC "ftn_powerwall/data/ina1"
#define INA2_TOPIC "ftn_powerwall/data/ina2"
#define INA3_TOPIC "ftn_powerwall/data/ina3"

#define SENSOR_WANTED_DATA 0b00011001

#define BROKER_ADDR "192.168.1.62"
#define BROKER_PORT 2525
#define BROKER_TIMEOUT 10

#endif //_TOPICS_H_
