#include <ArduinoJson.h>  // Inclua a biblioteca ArduinoJson
#include <NTPClient.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WiFi.h>

#include "cJSON.h"
#include "myFS.h"

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP _ntpUDP;
NTPClient _timeClient(_ntpUDP, "pool.ntp.org", -10800);

#define retornaSegundo(x) (1000 * (x))
#define retornaMinuto(x) (60 * 1000 * (x))
#define retornaHora(x) (60 * 60 * 1000 * (x))
#define RelePin 13
#define WiFi_LED 19
#define MAX_ATTEMPTS 10
// Arquivos de configuração
#define RedeData "/wifiData.JSON"

unsigned long tOn = 15, tOff = 4;
unsigned long flagT, t0;
bool ligado = false;

// Configurações WiFi
const char *ssid = "Bia 2";
const char *password = "coisafacil";
// const char *ssid = "S23";
// const char *password = "bemvindo";

// Configurações do ThingsBoard
const char *mqtt_server = "demo.thingsboard.io";
#define BombaCaju "C14W6ZGOQKxuKucdCFMj"
#define BombaGalinheiro "nvyrYVfkx4D99FG7fSIz"

// Variáveis de Controle
unsigned long t;
uint8_t acc1, cont = 0, contadorMQTT = 0, contadorWiFi = 0;
unsigned long agora, antes0, antes1;
bool flag = false, panic = false, religaWifi;

bool sendData(uint8_t porta1, String timestamp, uint8_t contador, unsigned long TON);
void reconnectMQTT(uint8_t &contadorMQTT);
void callback(char *topic, byte *payload, unsigned int length);
void thingsBoardTask(void *pvParameters);
void autoOpTask(void *pvParameters);
void getWifiData(bool serial, int index);
bool connectToWifi();

void setup() {
    Serial.begin(115200);
    pinMode(RelePin, OUTPUT);
    pinMode(WiFi_LED, OUTPUT);
    digitalWrite(RelePin, HIGH);
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    xTaskCreatePinnedToCore(thingsBoardTask, "thingsBoardTask", 10000, NULL, 1, NULL, 1);  // Executa no núcleo APP (Core 1)
    xTaskCreatePinnedToCore(autoOpTask, "autoOpTask", 10000, NULL, 1, NULL, 0);            // Executa no núcleo PRO (Core 0)
}

void loop() {
    vTaskSuspend(NULL);  // O loop original está vazio pois a task loopTask está rodando no Core 1
}

uint8_t digitalReadOld, autoOn;
// Task que executa o conteúdo original do loop() no núcleo APP (Core 1)
void thingsBoardTask(void *pvParameters) {
    digitalReadOld = digitalRead(RelePin);
    connectToWifi();
    _timeClient.begin();
    _timeClient.update();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    reconnectMQTT(contadorMQTT);
    while (true) {
        if (WiFi.status() != WL_CONNECTED) {
            connectToWifi();
            digitalWrite(WiFi_LED, LOW);  // Acende Led WiFi
        } else {
            digitalWrite(WiFi_LED, HIGH);  // Acende Led WiFi
        }
        if (!client.connected()) {
            client.setServer(mqtt_server, 1883);
            client.setCallback(callback);
            reconnectMQTT(contadorMQTT);
        } else {
            client.loop();
            _timeClient.update();
            if (millis() - t > retornaSegundo(30)) {
                if (sendData(!digitalRead(RelePin), _timeClient.getFormattedTime(), cont, tOn)) {
                    cont++;
                    _timeClient.update();
                } else {
                    cont = 1;
                }
                t = millis();
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Pequeno delay para não ocupar 100% da CPU
        if (acc1) {
            digitalWrite(RelePin, !acc1);  // Liga o relé
            if (millis() - t0 > retornaMinuto(tOn)) {
                acc1 = false;
                digitalWrite(RelePin, !acc1);  // Desliga o relé
                t0 = millis();
            }
        } else {
            if (!autoOn) {
                digitalWrite(RelePin, !acc1);  // Desliga o relé
            }
            t0 = millis();
        }
        if (panic) {
            digitalWrite(RelePin, panic);  // desliga o relé
            acc1 = false;                  // para o timer
        }
        if (digitalReadOld != digitalRead(RelePin)) {  // Se houve mudança de estado envia para o servidor
            sendData(!digitalRead(RelePin), _timeClient.getFormattedTime(), cont, tOn);
            digitalReadOld = digitalRead(RelePin);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Pequeno delay para não ocupar 100% da CPU
    }
}

// Task que executa a função autoOp no núcleo PRO (Core 0)
void autoOpTask(void *pvParameters) {
    while (true) {
        agora = millis();
        // Se a bomba está desligada e o tempo toff passou
        // if (!flag && (agora - antes0 >= retornaSegundo(30))){
        if (!flag && (agora - antes0 >= retornaHora(tOff))) {
            Serial.printf("Passou %d minuto\n", tOff);
            Serial.printf("Bomba Ligada\n");
            antes0 = agora;  // Atualiza o tempo de referência para o próximo acionamento
            antes1 = agora;  // Atualiza o tempo de referência para o próximo acionamento
            digitalWrite(RelePin, false);
            autoOn = true;
            flag = true;  // Liga a bomba
        }
        // Se a bomba está ligada e o tempo ton passou
        // else if (flag && (agora - antes1 >= retornaSegundo(5))){
        else if (flag && (agora - antes1 >= retornaMinuto(tOn))) {
            Serial.printf("Passou %d segundos\n", tOn);
            Serial.printf("Bomba Desligada\n");
            antes1 = agora;  // Atualiza o tempo de referência para o próximo desligamento
            digitalWrite(RelePin, true);
            autoOn = false;
            flag = false;  // Desliga a bomba
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Define a frequência de execução da autoOp
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
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (message.indexOf("acc1") > -1) {
        acc1 = doc["params"];
    }
    if (message.indexOf("tOn") > -1) {
        tOn = doc["params"];
        sendData(!digitalRead(RelePin), _timeClient.getFormattedTime(), cont, tOn);
    }
    if (message.indexOf("panic") > -1) {
        panic = doc["params"];
    }
    if (message.indexOf("restart") > -1) {
        ESP.restart();
    }
}

void reconnectMQTT(uint8_t &contadorMQTT) {
    while (!client.connected() && contadorMQTT < 15) {
        Serial.print("Tentando conectar ao MQTT...");
        if (client.connect("ESP32Client", BombaGalinheiro, NULL)) {
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

bool sendData(uint8_t porta1, String timestamp, uint8_t contador, unsigned long TON) {
    StaticJsonDocument<200> doc;
    doc["porta1"] = porta1;  // distância
    doc["timestamp"] = timestamp;
    doc["contador"] = contador;
    doc["tOn"] = TON;
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
    const char *jsonString = readFile(LittleFS, RedeData, false).c_str();
    delay(2000);
    Serial.println("jsonString:");
    Serial.println(jsonString);
    cJSON *json = cJSON_Parse(jsonString);
    if (json == NULL) {
        Serial.println("Erro ao analisar JSON.");
        return;
    }
    // Verifica se o campo 'networks' existe e é um array
    cJSON *networks = cJSON_GetObjectItemCaseSensitive(json, "networks");
    if (!cJSON_IsArray(networks)) {
        Serial.println("Campo 'networks' não encontrado ou não é um array.");
        cJSON_Delete(json);
        return;
    }
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
                ssid_str = ssid->valuestring;  // Atribui o valor do JSON
                pwd_str = pwd->valuestring;    // Atribui o valor do JSON

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
                    return true;  // Conectado com sucesso
                } else {
                    Serial.println();
                    Serial.println("Não foi possível conectar à WiFi, tentando próxima rede.");
                    notConnected = true;
                }
            }
        }
    }
    cJSON_Delete(json);
    return false;  // Isso nunca será executado devido ao ESP.restart()
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