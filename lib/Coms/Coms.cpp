#include "Coms.h"

ComWiFi::ComWiFi(std::string SSID, std::string PWD)
    : _ntpClient(_ntpUdp, "pool.ntp.org", gmtOffset_sec___, daylightOffset_sec___) {  // Ajuste o intervalo de atualização conforme necessário
    _ssid = SSID;
    _pwd = PWD;
    _address = address;
    _port = port;
    _mqtt.setClient(_WifiClient);
    _mqtt.setServer(_address.c_str(), _port);
    _mqtt.setKeepAlive(300);
    _mqtt.setCallback([this](const char *topic, byte *payload, unsigned int length) { this->callBackDownlink(topic, payload, length); });
}

bool ComWiFi::getAcc1() { return _acc1; }
bool ComWiFi::getUpdateFlag() { return _updateFlag; }

void ComWiFi::raiseUpdateFlag() { _updateFlag = true; }
void ComWiFi::lowerUpdateFlag() { _updateFlag = false; }

void ComWiFi::comsLoop() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiLed(HIGH);
        if (_mqtt.connected()) {
            _mqtt.loop();
            if (millis() - _tIdle > retornaSegundo(30)) {
                if (sendData(!digitalRead(RelePin), _ntpClient.getFormattedTime(), _counter, _tOn)) {
                    _counter++;
                } else {
                    _counter = 0;
                }
                _tIdle = millis();
            }
        } else {
            this->reconnectMQTT();
        }
    } else {
        WiFiLed(false);
        WiFi.disconnect();
        this->initWiFi();
    }
}

void ComWiFi::callBackDownlink(const char *topic, byte *payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("Mensagem recebida [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);
    // Mensagem recebida [v1/devices/me/rpc/request/28]: {"method":"acc","params":false}
    //  Parseie a mensagem JSON
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (message.indexOf("acc1") > -1) {
        _acc1 = doc["params"];
    }
    if (message.indexOf("tOn") > -1) {
        _tOn = doc["params"];
        sendData(!digitalRead(RelePin), _ntpClient.getFormattedTime(), _counter, _tOn);
    }
    if (message.indexOf("panic") > -1) {
        _panic = doc["params"];
    }
}

bool ComWiFi::sendData(uint8_t porta1, String timestamp, uint32_t contador, unsigned long TON) {
    StaticJsonDocument<200> doc;
    doc["porta1"] = porta1;
    doc["timestamp"] = timestamp;
    doc["contador"] = contador;
    doc["tOn"] = TON;
    char buffer[256];
    size_t packetsize = serializeJson(doc, buffer);
    if (this->publish(buffer, packetsize)) {
        Serial.println(buffer);
        return true;
    } else {
        return false;
    }
}

void ComWiFi::initNTP() {
    _ntpClient.begin();
    _ntpClient.update();
}

String ComWiFi::getTime() {
    _ntpClient.update();
    return _ntpClient.getFormattedTime();
}

bool ComWiFi::publish(String topic, String msg, size_t msgSize_t) { return _mqtt.publish(topic.c_str(), msg.c_str(), msgSize_t); }
bool ComWiFi::publish(String msg, size_t msgSize_t) { return _mqtt.publish(topic_pub, msg.c_str(), msgSize_t); }

bool ComWiFi::reconnectMQTT() {
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
    if (WiFi.status() == WL_CONNECTED) {
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
