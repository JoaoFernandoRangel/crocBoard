#include <NTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "conf.h"


//Macros para thingsboard
#define address "demo.thingsboard.io"
#define mqttToken "C14W6ZGOQKxuKucdCFMj"
#define port 1883
#define topic_pub "v1/devices/me/telemetry"


// Macros para NTP
#define gmtOffset_sec___ -10800    // Offset GMT em segundos
#define daylightOffset_sec___ 60000    // Offset horario de verao
#define ntpServer1 "pool.ntp.org"  // Servidor NTP 1
#define ntpServer2 "a.st1.ntp.br"  // Servidor NTP 2

class ComWiFi {
   private:
    std::string _ssid, _pwd;
    std::string _address;
    uint _port;
    uint _contadorWifi;
    uint _contadorMQTT;

    WiFiClient _WifiClient;
    PubSubClient _mqtt;
    WiFiUDP _ntpUdp;
    NTPClient _ntpClient;

    unsigned long _tOn, _tOff;

   public:
    ComWiFi(std::string SSID, std::string PWD);
    bool initWiFi();
    bool reconnectMQTT();
    void initNTP();
    String getTime();
    bool publish(String topic, String msg, size_t msgSize_t);
    bool publish(String msg, size_t msgSize_t);
};