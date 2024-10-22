#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // Inclua a biblioteca ArduinoJson
#include <Update.h>

//teste

// Configurações WiFi
const char* ssid = "adivinha";
const char* password = "ALTernat1v0";

int k = 0;
int vetorTeste1[] = {
    12, 45, 67, 23, 89, 34, 56, 78, 90, 11,
    22, 33, 44, 55, 66, 77, 88, 99, 10, 21,
    32, 43, 54, 65, 76, 87, 98, 13, 24, 35,
    46, 57, 68, 79, 80, 14, 15, 16, 17, 18,
    19, 20, 31, 42, 53, 64, 75, 86, 97, 29,
    30, 41, 52, 63, 74, 85, 96, 27, 28, 39,
    40, 51, 62, 73, 84, 95, 26, 25, 36, 37,
    38, 49, 50, 61, 72, 83, 94, 23, 92, 93,
    9, 14, 15, 21, 22, 33, 34, 35, 45, 46,
    47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 61, 62, 63, 64, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
    77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
    87, 88, 89, 90, 91, 92, 93, 94, 95, 96,
    97, 98, 99, 0, 1, 2, 3, 4, 5, 6,
    7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
    47, 48, 49, 50, 51, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 61, 62, 63, 64, 65, 66,
    67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
    77, 78, 79, 80, 81, 82, 83, 84, 85, 86,
    87, 88, 89, 90, 91, 92, 93, 94, 95, 96
};
int vetorTeste2[] = {
    57, 92, 36, 19, 45, 88, 66, 73, 29, 54,
    11, 98, 83, 25, 12, 70, 60, 1, 5, 81,
    91, 3, 30, 77, 84, 44, 6, 15, 49, 35,
    23, 50, 78, 72, 8, 9, 2, 69, 27, 17,
    86, 24, 37, 65, 4, 38, 68, 99, 16, 64,
    41, 14, 10, 79, 87, 34, 74, 32, 90, 20,
    39, 67, 76, 48, 80, 59, 55, 75, 61, 47,
    18, 13, 7, 53, 33, 52, 58, 42, 26, 100,
    63, 93, 46, 40, 62, 82, 97, 71, 94, 95,
    39, 14, 36, 44, 21, 88, 84, 13, 28, 66,
    1, 5, 90, 59, 8, 26, 47, 72, 19, 32,
    74, 85, 9, 100, 17, 29, 57, 77, 41, 12,
    75, 52, 31, 60, 67, 24, 2, 10, 70, 68,
    6, 20, 33, 11, 16, 39, 43, 25, 86, 69,
    94, 22, 53, 54, 14, 99, 30, 83, 23, 76,
    73, 4, 87, 58, 80, 55, 45, 92, 71, 64,
    3, 18, 63, 95, 65, 79, 88, 42, 35, 15,
    26, 82, 56, 1, 78, 46, 90, 12, 62, 77,
    20, 14, 7, 93, 38, 66, 99, 8, 31, 65,
    37, 22, 32, 21, 72, 45, 64, 81, 36, 13,
    40, 44, 50, 11, 2, 17, 98, 84, 57, 23
};

// Configurações do ThingsBoard
const char* mqtt_server = "demo.thingsboard.io";
const char* mqtt_token = "vd5NpYwDMOXrtCYZUFfa";

// Pinos dos LEDs

// Pinos Conversor AD

// Variaáveis de Tempo
unsigned long lastSendTime = 0;
const long interval = 2000; // Intervalo de envio em milissegundos (2 segundos)


WiFiClient espClient;
PubSubClient client(espClient);



void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}



void callback(char* topic, byte* payload, unsigned int length) {
  // Converta o payload para uma string
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // Parseie a mensagem JSON
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Extraia os campos method e params
  const char* method = doc["method"];
  int params = doc["params"];

  Serial.print("Método: ");
  Serial.println(method);
  Serial.print("Parâmetros: ");
  Serial.println(params);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    if (client.connect("ESP32Client", mqtt_token, NULL)) {
      Serial.println("Conectado");
      client.subscribe("v1/devices/me/rpc/request/+");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tente novamente em 5 segundos");
      delay(5000);
    }
  }
}

void sendData(int v1, int v2) {

  StaticJsonDocument<200> doc;
  doc["Valor1"] = v1;
  doc["fuelLevel"] = v1;
  doc["temperature"] = v2;

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish("v1/devices/me/telemetry", buffer, n);
  // client.publish("v2/devices/me/telemetry", feedbackConstructor.c_str(), n);

}

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);


}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastSendTime > 500) {
    lastSendTime = now;
    
    sendData(vetorTeste1[k],vetorTeste2[k]);

    k = k+1;
    if (k>5) {k=2;}
  }

}