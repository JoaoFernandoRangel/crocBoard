#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // Inclua a biblioteca ArduinoJson
#include <Update.h>
#include <NTPClient.h>

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP _ntpUDP;
NTPClient _timeClient(_ntpUDP, "pool.ntp.org", -10800);

#define retornaSegundo(x) (1000 * (x))
#define retornaMinuto(x) (60 * 1000 * (x))
#define retornaHora(x) (60 * 60 * 1000 * (x))
#define RelePin 27
unsigned long tOn = 15, tOff = 4;
unsigned long flagT, t0;
bool ligado = false;

// Configurações WiFi
const char *ssid = "Bia 2";
const char *password = "coisafacil";

// Configurações do ThingsBoard
const char *mqtt_server = "demo.thingsboard.io";
const char *mqtt_token = "C14W6ZGOQKxuKucdCFMj";

// Variáveis de Tempo
unsigned long t;
uint8_t acc1, cont = 0;

bool sendData(uint8_t porta1, String timestamp, uint8_t contador, unsigned long TON);
void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
void setup_wifi();
void thingsBoardTask(void *pvParameters);
void autoOpTask(void *pvParameters);

void setup()
{
  Serial.begin(9600);
  pinMode(RelePin, OUTPUT);
  digitalWrite(RelePin, HIGH);
  setup_wifi();
  _timeClient.begin();
  _timeClient.update();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  xTaskCreatePinnedToCore(thingsBoardTask, "thingsBoardTask", 10000, NULL, 1, NULL, 1); // Executa no núcleo APP (Core 1)
  xTaskCreatePinnedToCore(autoOpTask, "autoOpTask", 10000, NULL, 1, NULL, 0);           // Executa no núcleo PRO (Core 0)
}

unsigned long agora, antes0, antes1;
bool flag = false, panic = false;
void loop()
{
  vTaskSuspend(NULL); // O loop original está vazio pois a task loopTask está rodando no Core 1
}

// Task que executa o conteúdo original do loop() no núcleo APP (Core 1)
void thingsBoardTask(void *pvParameters)
{
  while (true)
  {
    if (client.connected())
    {
      client.loop();
      if (millis() - t > retornaSegundo(1))
      {
        if (sendData(!digitalRead(RelePin), _timeClient.getFormattedTime(), cont, tOn))
        {
          cont++;
          _timeClient.update();
        }
        else
        {
          cont = 1;
        }
        t = millis();
      }
    }
    else
    {
      reconnect();
    }
    if (acc1)
    {
      digitalWrite(RelePin, !acc1); // Liga o relé
      // if (millis() - t0 > retornaSegundo(tOn))
      if (millis() - t0 > retornaMinuto(tOn))
      {
        acc1 = false;
        digitalWrite(RelePin, !acc1); // Desliga o relé
        t0 = millis();
      }
    }
    else
    {
      t0 = millis();
    }
    if (panic)
    {
      digitalWrite(RelePin, panic);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Pequeno delay para não ocupar 100% da CPU
  }
}

// Task que executa a função autoOp no núcleo PRO (Core 0)
void autoOpTask(void *pvParameters)
{
  while (true)
  {
    agora = millis();
    // Se a bomba está desligada e o tempo toff passou
    // if (!flag && (agora - antes0 >= retornaMinuto(tOff)))
    if (!flag && (agora - antes0 >= retornaHora(tOff)))
    {
      Serial.printf("Passou %d minuto\n", tOff);
      Serial.printf("Bomba Ligada\n");
      antes0 = agora; // Atualiza o tempo de referência para o próximo acionamento
      antes1 = agora; // Atualiza o tempo de referência para o próximo acionamento
      digitalWrite(RelePin, false);
      flag = true; // Liga a bomba
    }
    // Se a bomba está ligada e o tempo ton passou
    // else if (flag && (agora - antes1 >= retornaSegundo(tOn)))
    else if (flag && (agora - antes1 >= retornaMinuto(tOn)))
    {
      Serial.printf("Passou %d segundos\n", tOn);
      Serial.printf("Bomba Desligada\n");
      antes1 = agora; // Atualiza o tempo de referência para o próximo desligamento
      digitalWrite(RelePin, true);
      flag = false; // Desliga a bomba
    }
    vTaskDelay(100 / portTICK_PERIOD_MS); // Define a frequência de execução da autoOp
  }
}

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  /*
  Mensagem recebida [v1/devices/me/rpc/request/25]: {"method":"slider2","params":80}
  Mensagem recebida [v1/devices/me/rpc/request/26]: {"method":"slider1","params":60}
  */

  // Converta o payload para uma string
  String message;
  for (unsigned int i = 0; i < length; i++)
  {
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
  if (message.indexOf("acc1") > -1)
  {
    acc1 = doc["params"];
  }
  if (message.indexOf("tOn") > -1)
  {
    tOn = doc["params"];
  }
  if (message.indexOf("panic") > -1)
  {
    panic = doc["params"];
  }
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Tentando conectar ao MQTT...");
    if (client.connect("ESP32Client", mqtt_token, NULL))
    {
      Serial.println("Conectado");
      client.subscribe("v1/devices/me/rpc/request/+");
    }
    else
    {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tente novamente em 5 segundos");
      delay(5000);
    }
  }
}

bool sendData(uint8_t porta1, String timestamp, uint8_t contador, unsigned long TON)
{
  StaticJsonDocument<200> doc;
  doc["porta1"] = porta1; // distância
  doc["timestamp"] = timestamp;
  doc["contador"] = contador;
  doc["tOn"] = TON;
  char buffer[256];
  size_t packetsize = serializeJson(doc, buffer);
  if (client.publish("v1/devices/me/telemetry", buffer, packetsize))
  {
    Serial.println(buffer);
    return true;
  }
  else
  {
    return false;
  }
}