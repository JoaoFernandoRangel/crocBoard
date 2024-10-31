#include <ArduinoJson.h>  // Inclua a biblioteca ArduinoJson

#include "Coms.h"
#include "conf.h"

const char *ssid = "Bia 2";
const char *password = "coisafacil";

#define SSID "Bia 2"
#define PWD "coisafacil"

ComWiFi coms(SSID, PWD);

// Configurações WiFi
// const char *ssid = "S23";
// const char *password = "bemvindo";

// Configurações do ThingsBoard
const char *mqtt_server = "demo.thingsboard.io";
const char *mqtt_token = "C14W6ZGOQKxuKucdCFMj";

// Variáveis de Controle
unsigned long t;
uint8_t acc1, cont = 0, contadorMQTT = 0, contadorWiFi = 0;
unsigned long agora, antes0, antes1;
bool flag = false, panic = false;

bool sendData(uint8_t porta1, String timestamp, uint8_t contador, unsigned long TON);
void reconnectMQTT(uint8_t &contadorMQTT);
void callback(char *topic, byte *payload, unsigned int length);
void setup_wifi();
void thingsBoardTask(void *pvParameters);
void autoOpTask(void *pvParameters);

void setup() {
    Serial.begin(9600);
    pinMode(RelePin, OUTPUT);
    pinMode(WiFi_LED, OUTPUT);
    digitalWrite(RelePin, HIGH);
    coms.initWiFi();
    coms.reconnectMQTT();
    xTaskCreatePinnedToCore(thingsBoardTask, "thingsBoardTask", 10000, NULL, 1, NULL, 1);  // Executa no núcleo APP (Core 1)
    xTaskCreatePinnedToCore(autoOpTask, "autoOpTask", 10000, NULL, 1, NULL, 0);            // Executa no núcleo PRO (Core 0)
}

void loop() {
    vTaskSuspend(NULL);  // O loop original está vazio pois a task loopTask está rodando no Core 1
}

unsigned long t0;
uint8_t digitalReadOld;
// Task que executa o conteúdo original do loop() no núcleo APP (Core 1)

// Imbutir etapas de verificação dentro de Coms
// Implementar o transito de dados entre tasks
void thingsBoardTask(void *pvParameters) {
    digitalReadOld = digitalRead(RelePin);
    while (true) {
        coms.comsLoop();
        if (coms.getAcc1()) {
            digitalWrite(RelePin, !coms.getAcc1());  // Liga o relé
            // if (millis() - t0 > retornaSegundo(tOn))
            if (millis() - t0 > retornaMinuto(coms.getTon())) {
                coms.setAcc1(false);
                digitalWrite(RelePin, !coms.getAcc1());  // Desliga o relé
                t0 = millis();
            }
        } else {
            digitalWrite(RelePin, !acc1);  // Desliga o relé
            t0 = millis();
        }
        if (panic) {
            coms.setAcc1(false);  // para o timer
        }
        if (digitalReadOld != digitalRead(RelePin)) {  // Se houve mudança de estado envia para o servidor
            coms.updateServer();
            digitalReadOld = digitalRead(RelePin);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Pequeno delay para não ocupar 100% da CPU
    }
}

// Task que executa a função autoOp no núcleo PRO (Core 0)
// Mudar para buscar os valores de operação automática na classe de wifi.
// Implementar semáforos para busca de dados quando houver mudança.
void autoOpTask(void *pvParameters) {
    while (true) {
        agora = millis();
        // Se a bomba está desligada e o tempo toff passou
        // if (!flag && (agora - antes0 >= retornaSegundo(30))){
        if (!flag && (agora - antes0 >= retornaHora(coms.getToff()))) {
            Serial.printf("Passou %d minuto\n", coms.getToff());
            Serial.printf("Bomba Ligada\n");
            antes0 = agora;  // Atualiza o tempo de referência para o próximo acionamento
            antes1 = agora;  // Atualiza o tempo de referência para o próximo acionamento
            digitalWrite(RelePin, false);
            flag = true;  // Liga a bomba
        }
        // Se a bomba está ligada e o tempo ton passou
        // else if (flag && (agora - antes1 >= retornaSegundo(tOn)))
        else if (flag && (agora - antes1 >= retornaMinuto(coms.getTon()))) {
            Serial.printf("Passou %d segundos\n", coms.getTon());
            Serial.printf("Bomba Desligada\n");
            antes1 = agora;  // Atualiza o tempo de referência para o próximo desligamento
            digitalWrite(RelePin, true);
            flag = false;  // Desliga a bomba
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Define a frequência de execução da autoOp
    }
}

