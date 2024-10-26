#include "Coms.h"

ComWiFi::ComWiFi(std::string SSID, std::string PWD)
    : _ntpClient(_ntpUdp, "pool.ntp.org", gmtOffset_sec___, daylightOffset_sec___) {  // Ajuste o intervalo de atualização conforme necessário
    _ssid = SSID;
    _pwd = PWD;
    _address = address;
    _port = port;

    _mqtt.setClient(_WifiClient);
    _mqtt.setServer(_address.c_str(), _port);
}

void ComWiFi::initNTP() {
    _ntpClient.begin();
    _ntpClient.update();
}

String ComWiFi::getTime(){
    _ntpClient.update();
    return _ntpClient.getFormattedTime();
}

bool ComWiFi::publish(String topic, String msg, size_t msgSize_t){
    return _mqtt.publish(topic.c_str(), msg.c_str(), msgSize_t);
}
bool ComWiFi::publish(String msg, size_t msgSize_t){
    return _mqtt.publish(topic_pub, msg.c_str(), msgSize_t);
}

bool ComWiFi::reconnectMQTT(){
        while (!_mqtt.connected() && _contadorMQTT < 15) {
        Serial.print("Tentando conectar ao MQTT...");
        if (_mqtt.connect("ESP32Client", mqttToken, NULL)) {
            Serial.println("Conectado");
            _mqtt.subscribe("v1/devices/me/rpc/request/+");
            _contadorMQTT = 0;
        } else {
            Serial.print("falhou, rc=");
            Serial.print(_mqtt.state());
            Serial.println(" tente novamente em 5 segundos");
            delay(5000);
            _contadorMQTT++;
        }
    }
}



bool ComWiFi::initWiFi() {
    vTaskDelay(pdMS_TO_TICKS(10));
    Serial.println();
    Serial.print("Conectando a ");
    Serial.println(_ssid.c_str());
    WiFi.begin(_ssid.c_str(), _pwd.c_str());
    while (WiFi.status() != WL_CONNECTED && _contadorWifi <= 10) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
        _contadorWifi++;
    }
    if(WiFi.status() == WL_CONNECTED){
        return true;
    }
    unsigned long t1, t2, agoraw;
    if (_contadorWifi >= 10) {
        t1 = millis();
        t2 = millis();
        while (true) {
            agoraw = millis();
            // if (agoraw - t1 > retornaHora(1)) {
            if (agoraw - t1 > retornaMinuto(1)) {
                break;
            }
            if (agoraw - t2 > 500) {
                digitalWrite(WiFi_LED, !digitalRead(WiFi_LED));
                t2 = millis();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    } else {
        _contadorWifi = 0;
    }
    Serial.println("");
    Serial.println("WiFi conectado");
    Serial.println("Endereço IP: ");
    Serial.println(WiFi.localIP());
}
