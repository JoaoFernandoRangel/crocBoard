#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // Inclua a biblioteca ArduinoJson
#include <Update.h>

#define LED_VOL 12
#define LED_TEMP 13

#define retornaSegundo(x) (1000*(x))


// Configurações WiFi
const char *ssid = "Wokwi-GUEST";
const char *password = "";

// Configurações do ThingsBoard
const char *mqtt_server = "demo.thingsboard.io";
const char *mqtt_token = "vd5NpYwDMOXrtCYZUFfa";

// Variáveis de Tempo
unsigned long t;
float Altura, Temperatura;

void sendData(float v1, float v2);
void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
void setup_wifi();
void fazLeitura(float &Altura, float &Temperatura);

WiFiClient espClient;
PubSubClient client(espClient);

void setup()
{
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (client.connected())
  {
    client.loop();
  }
  else
  {
    reconnect();
  }
  

  if (millis() - t > retornaSegundo(1))
  {
    // fazLeitura(Altura, Temperatura);
    // sendData(Altura, Temperatura);
    t = millis();
  }
  if (Altura > setpoint1)
  {
    digitalWrite(LED_VOL, LOW);
  }
  else
  {
    digitalWrite(LED_VOL, HIGH);
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

int setpoint1, setpoint2;
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
  // Parseie a mensagem JSON
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (message.indexOf("slider2") > -1)
  {
    setpoint2 = doc["params"];
  }
  if (message.indexOf("slider1") > -1)
  {
    setpoint1 = doc["params"];
  }
  Serial.printf("Slider1 = %d\n", setpoint1);
  Serial.printf("Slider2 = %d\n", setpoint2);
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

void sendData(float Altura, float Temperatura)
{
  StaticJsonDocument<200> doc;
  doc["fuelLevel"] = Altura;        // distância
  doc["temperature"] = Temperatura; // Temperatura
  char buffer[256];
  size_t packetsize = serializeJson(doc, buffer);
  client.publish("v1/devices/me/telemetry", buffer, packetsize);
}