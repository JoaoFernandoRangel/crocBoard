#include <ArduinoJson.h> // Inclua a biblioteca ArduinoJson
#include <NTPClient.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WiFi.h>

#include "cJSON.h"
#include "conf.h"
#include "myFS.h"

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP _ntpUDP;
NTPClient _timeClient(_ntpUDP, "pool.ntp.org", -10800);

unsigned long tOn = 15, tOff = 4;
unsigned long flagT, t0;
bool ligado = false;

// Configurações WiFi
// const char *ssid = "S23";
// const char *password = "bemvindo";

// Configurações do ThingsBoard
const char *ssid;
const char *password;
const char *mqtt_server = "demo.thingsboard.io";

// Variáveis de Controle
unsigned long t;
uint8_t cont = 0, contadorWiFi = 0;
unsigned long agora, antes0, antes1;
bool flag = false, panic = false, religaWifi, acc1 = false;

bool sendData(uint8_t porta1, String timestamp, uint8_t contador, unsigned long TON, bool invert);
void reconnectMQTT();
void callback(char *topic, byte *payload, unsigned int length);
void thingsBoardTask(void *pvParameters);
void autoOpTask(void *pvParameters);
void getWifiData(bool serial, int index);
bool connectToWifi();
void manageWiFi();
void manageMQTT();
void manageRelay();

void setup() {
    Serial.begin(115200);
    pinMode(RelePin, OUTPUT);
    pinMode(WiFi_LED, OUTPUT);
    digitalWrite(RelePin, inverted);

    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    xTaskCreatePinnedToCore(thingsBoardTask, "thingsBoardTask", 10000, NULL, 1, NULL, 1); // Executa no núcleo APP (Core 1)
    xTaskCreatePinnedToCore(autoOpTask, "autoOpTask", 10000, NULL, 1, NULL, 0);           // Executa no núcleo PRO (Core 0)
}

void loop() {
    vTaskSuspend(NULL); // O loop original está vazio pois a task loopTask está rodando no Core 1
}

uint8_t digitalReadOld, autoOn;
// Task que executa o conteúdo original do loop() no núcleo APP (Core 1)
void thingsBoardTask(void *pvParameters) {
    digitalReadOld = digitalRead(RelePin);
    connectToWifi();
    _timeClient.begin();
    _timeClient.update();
    manageMQTT();
    while (true) {
        manageWiFi();
        if (!client.connected()) {
            manageMQTT();
        } else {
            client.loop();
            _timeClient.update();
            if (millis() - t > retornaSegundo(30)) {
                if (sendData(digitalRead(RelePin), _timeClient.getFormattedTime(), cont, tOn, inverted)) {
                    cont++;
                    _timeClient.update();
                } else {
                    cont = 1;
                }
                t = millis();
            }
        }
        manageRelay();
        if (digitalReadOld != digitalRead(RelePin)) { // Se houve mudança de estado envia para o servidor
            sendData(digitalRead(RelePin), _timeClient.getFormattedTime(), cont, tOn, inverted);
            digitalReadOld = digitalRead(RelePin);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Pequeno delay para não ocupar 100% da CPU
    }
}

// Task que executa a função autoOp no núcleo PRO (Core 0)
void autoOpTask(void *pvParameters) {
    while (true) {
        agora = millis();
        // Se a bomba está desligada e o tempo toff passou
        // if (!flag && (agora - antes0 >= retornaSegundo(10))){
        if (!flag && (agora - antes0 >= retornaHora(tOff))) {
            Serial.printf("Passou %d minuto\n", tOff);
            Serial.printf("Bomba Ligada\n");
            antes0 = agora;
            antes1 = agora;
            digitalWrite(RelePin, !inverted); // Liga o relé
            autoOn = true;
            flag = true; // Liga a bomba
        }
        // Se a bomba está ligada e o tempo ton passou
        // else if (flag && (agora - antes1 >= retornaSegundo(5))){
        else if (flag && (agora - antes1 >= retornaMinuto(tOn))) {
            Serial.printf("Passou %d segundos\n", tOn);
            Serial.printf("Bomba Desligada\n");
            antes1 = agora;
            digitalWrite(RelePin, inverted); // Desliga o relé
            autoOn = false;
            flag = false; // Desliga a bomba
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void manageWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWifi();
        digitalWrite(WiFi_LED, LOW); // Acende LED WiFi indicando desconexão
    } else {
        digitalWrite(WiFi_LED, HIGH); // Acende LED WiFi indicando conexão
    }
}
void manageMQTT() {
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    reconnectMQTT();
}

void manageRelay() {
    if (acc1) {
        digitalWrite(RelePin, !inverted); // Liga o relé
        if (millis() - t0 > retornaMinuto(tOn)) {
            acc1 = false;
            digitalWrite(RelePin, inverted); // Desliga o relé
            t0 = millis();
        }
    } else {
        if (!autoOn) {
            digitalWrite(RelePin, inverted); // Garante que o relé está desligado
        }
        t0 = millis();
    }

    if (panic) {
        digitalWrite(RelePin, inverted); // Desliga o relé em modo pânico
        acc1 = false;                    // Para o timer
    }
}

// TODO - trocar por função de busca em JSON

void callback(char *topic, byte *payload, unsigned int length) {
    /*
    Mensagem recebida [v1/devices/me/rpc/request/25]: {"method":"slider2","params":80}
    Mensagem recebida [v1/devices/me/rpc/request/26]: {"method":"slider1","params":60}
    */

    // Converta o payload para uma string
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
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (message.indexOf("acc1") > -1) {
        acc1 = doc["params"];
        manageRelay();
    }
    if (message.indexOf("tOn") > -1) {
        tOn = doc["params"];
    }
    if (message.indexOf("tOff") > -1) {
        tOff = doc["params"];
    }
    if (message.indexOf("panic") > -1) {
        panic = doc["params"];
    }
    if (message.indexOf("restart") > -1) {
        ESP.restart();
    }
    sendData(digitalRead(RelePin), _timeClient.getFormattedTime(), cont, tOn, inverted);
}

void reconnectMQTT() {
    int contadorMQTT = 0;
    while (!client.connected() && contadorMQTT < 15) {
        Serial.print("Tentando conectar ao MQTT...");
        if (client.connect("ESP32Client", BombaTarget, NULL)) {
            Serial.println("Conectado");
            client.subscribe("v1/devices/me/rpc/request/+");
            contadorMQTT = 0;
        } else {
            Serial.print("falhou, rc=");
            Serial.print(client.state());
            Serial.println(" tente novamente em 5 segundos");
            delay(5000);
            contadorMQTT++;
        }
    }
}

bool sendData(uint8_t porta1, String timestamp, uint8_t contador, unsigned long TON, bool invert) {
    JsonDocument doc;
    if (invert) {
        doc["porta1"] = !porta1; // distância
    } else {
        doc["porta1"] = porta1; // distância
    }
    doc["timestamp"] = timestamp;
    doc["contador"] = contador;
    doc["tOn"] = TON;
    doc["tOff"] = tOff; // variável global
    doc["panic"] = panic; // variável global
    char buffer[256];
    size_t packetsize = serializeJson(doc, buffer);
    if (client.publish("v1/devices/me/telemetry", buffer, packetsize)) {
        Serial.println(buffer);
        return true;
    } else {
        return false;
    }
}
bool connectToWifi() {
    int maxAttemptsPerNetwork = MAX_ATTEMPTS;
    bool notConnected = true;
    String jsonString = readFile(LittleFS, RedeData, false);
    delay(2000);
    Serial.println("jsonString:");
    Serial.println(jsonString);
    cJSON *json = cJSON_Parse(jsonString.c_str());
    if (json == NULL) {
        Serial.println("Erro ao analisar JSON.");
    }
    // Verifica se o campo 'networks' existe e é um array
    cJSON *networks = cJSON_GetObjectItemCaseSensitive(json, "networks");
    // if (!cJSON_IsArray(networks)) {
    //     Serial.println("Campo 'networks' não encontrado ou não é um array.");
    //     cJSON_Delete(json);
    // }
    // Obtém o tamanho do array
    int networkCount = cJSON_GetArraySize(networks);
    Serial.print("networkCount: ");
    Serial.println(networkCount);
    WiFi.mode(WIFI_MODE_STA);
    std::string ssid_str, pwd_str;
    // Itera sobre as redes para garantir que os dados estejam corretos
    for (int i = 0; i < networkCount; i++) {
        int contador = 0;
        cJSON *network = cJSON_GetArrayItem(networks, i);
        if (cJSON_IsObject(network)) {
            cJSON *ssid = cJSON_GetObjectItemCaseSensitive(network, "SSID");
            cJSON *pwd = cJSON_GetObjectItemCaseSensitive(network, "PWD");

            if (cJSON_IsString(ssid) && cJSON_IsString(pwd)) {
                ssid_str = ssid->valuestring; // Atribui o valor do JSON
                pwd_str = pwd->valuestring;   // Atribui o valor do JSON

                Serial.print("SSID: ");
                Serial.println(ssid_str.c_str());
                Serial.print("PWD: ");
                Serial.println(pwd_str.c_str());

                WiFi.begin(ssid_str.c_str(), pwd_str.c_str());
                unsigned long startTime = millis();
                while (WiFi.status() != WL_CONNECTED && contador <= maxAttemptsPerNetwork) {
                    Serial.print(".");
                    contador++;
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.println();
                    Serial.println("WiFi connected");
                    Serial.print("IP address: ");
                    Serial.println(WiFi.localIP());
                    notConnected = false;
                    cJSON_Delete(json);
                    return true; // Conectado com sucesso
                } else {
                    Serial.println();
                    Serial.println("Não foi possível conectar à WiFi, tentando próxima rede.");
                    notConnected = true;
                }
            }
        }
    }
    cJSON_Delete(json);
    return false; // Isso nunca será executado devido ao ESP.restart()
}

void getWifiData(bool serial, int index) {
    // Parse o JSON
    cJSON *file = cJSON_Parse(readFile(LittleFS, RedeData, false).c_str());
    if (file == NULL) {
        Serial.println("Erro ao parsear JSON!");
        return;
    }

    // Obtenha o vetor de redes
    cJSON *networks = cJSON_GetObjectItemCaseSensitive(file, "networks");
    if (!cJSON_IsArray(networks)) {
        Serial.println("O JSON não contém um vetor 'networks' válido!");
        cJSON_Delete(file);
        return;
    }

    // Obtenha o objeto de rede no índice fornecido
    cJSON *network = cJSON_GetArrayItem(networks, index);
    if (network == NULL) {
        Serial.println("Índice de rede fora do intervalo!");
        cJSON_Delete(file);
        return;
    }

    // Obtenha os campos SSID e PWD
    cJSON *SSID = cJSON_GetObjectItemCaseSensitive(network, "SSID");
    cJSON *PWD = cJSON_GetObjectItemCaseSensitive(network, "PWD");

    if (cJSON_IsString(SSID) && (SSID->valuestring != NULL)) {
        ssid = SSID->valuestring;
    } else {
        Serial.println("SSID inválido!");
    }

    if (cJSON_IsString(PWD) && (PWD->valuestring != NULL)) {
        password = PWD->valuestring;
    } else {
        Serial.println("PWD inválido!");
    }

    // Debugging output
    if (serial) {
        Serial.print("SSID:");
        Serial.print(ssid);
        Serial.print("|| PWD:");
        Serial.println(password);
    }

    // Libere a memória
    cJSON_Delete(file);
}