// Confidential data
// WiFi, MQTT, Server ...etc

#include <WiFi.h>
#include <PubSubClient.h>  // mqtt lib: https://github.com/knolleary/pubsubclient/

const char* ssid = "****";  //AP ID, PW
const char* password = "****";

const char* mqtt_server = "***.***.**.**"; //Broker ip
const char* ID = "****"; //Should be unique id
//const char* TOPIC = "";

const char* mqtt_topic_status = "jibmusil/gwangjingu/first/01/wall";