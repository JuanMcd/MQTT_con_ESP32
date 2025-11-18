#include <WiFi.h>          // Librería para conectar la ESP32 a WiFi
#include <PubSubClient.h>  // Librería para MQTT
#include "DHT.h"           // Librería del sensor DHT

// ============================
// 1. CONFIGURACIÓN WiFi Y MQTT
// ============================

const char* ssid = "A55 de Juan";                // Nombre de la red WiFi
const char* password = "12345678";       // Contraseña de la red WiFi
const char* mqtt_server = "test.mosquitto.org";  // Broker MQTT público

/*Broker públicos gratuitos:
mosquitto: test.mosquitto.org
hivemq: broker.hivemq.com
emqx: broker.emqx.io*/

WiFiClient espClient;          // Cliente WiFi
PubSubClient client(espClient); // Cliente MQTT usando la conexión WiFi

// Variable para controlar cada cuánto se envían los datos
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

// ============================
// 2. CONFIGURACIÓN DEL SENSOR DHT11
// ============================

#define DHTPIN 4       // Pin digital de la ESP32 conectado al DHT11
#define DHTTYPE DHT11  // Tipo de sensor

DHT dht(DHTPIN, DHTTYPE);  // Se crea el objeto del sensor


// ============================
// 3. FUNCIÓN PARA CONECTAR AL WiFi
// ============================

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);          // Modo estación (se conecta a un router)
  WiFi.begin(ssid, password);   // Inicia la conexión WiFi

  // Espera hasta que la conexión sea exitosa
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());  // Se usa más adelante para generar un clientId aleatorio

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
}


// ============================
// 4. FUNCIÓN DE CALLBACK MQTT
//    (RECIBE MENSAJES DEL BROKER)
// ============================

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");

  // Imprime el mensaje recibido carácter por carácter
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Ejemplo: encender/apagar el LED según el mensaje
  if ((char)payload[0] == '1') {
    digitalWrite(2, LOW);   // LED encendido (activo en bajo en algunas placas)
  } else {
    digitalWrite(2, HIGH);  // LED apagado
  }
}


// ============================
// 5. FUNCIÓN PARA RECONEXIÓN MQTT
// ============================

void reconnect() {
  // Bucle hasta que logremos reconectar
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    
    // Se crea un ID de cliente aleatorio
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    // Intento de conexión al broker
    if (client.connect(clientId.c_str())) {
      Serial.println("conectado");
      // Publica un mensaje de bienvenida
      client.publish("outTopic", "ESP32 conectado");
      // Se suscribe a un tópico para recibir órdenes (opcional)
      client.subscribe("inTopic");
    } else {
      Serial.print("falló, rc=");
      Serial.print(client.state());
      Serial.println("  Reintentando en 5 segundos...");
      delay(5000);
    }
  }
}


// ============================
// 6. SETUP: INICIALIZACIÓN
// ============================

void setup() {
  pinMode(2, OUTPUT);      // LED integrado de la ESP32 (para pruebas)
  Serial.begin(115200);    // Velocidad del puerto serial
  dht.begin();             // Inicializa el sensor DHT11

  setup_wifi();            // Conexión a la red WiFi
  client.setServer(mqtt_server, 1883); // Dirección y puerto del broker MQTT
  client.setCallback(callback);        // Función que manejará los mensajes entrantes
}


// ============================
// 7. LOOP PRINCIPAL
// ============================
// AQUÍ ES DONDE SE LEE EL DHT11 Y SE PUBLICAN LOS TÓPICOS temp Y hum
//    

void loop() {
  // Verifica conexión MQTT y se reconecta si hace falta
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantiene activa la conexión MQTT

  unsigned long now = millis();
  // Enviar datos cada 2000 ms (2 segundos)
  if (now - lastMsg > 2000) {
    lastMsg = now;

    // ---- LECTURA DEL SENSOR DHT11 ----
    float h = dht.readHumidity();     // Humedad relativa (%)
    float t = dht.readTemperature();  // Temperatura en °C

    // Verificación de lectura correcta
    if (isnan(h) || isnan(t)) {
      Serial.println("Error al leer el sensor DHT11");
      return; // Si la lectura falla, salimos del loop y lo intentamos en la próxima iteración
    }

    // Imprimir en el monitor serial (útil para depuración y demostración)
    Serial.print("Temperatura: ");
    Serial.print(t);
    Serial.print(" °C  |  Humedad: ");
    Serial.print(h);
    Serial.println(" %");

    // Convertir a String para poder publicar con client.publish()
    String tempStr = String(t, 2); // 2 decimales
    String humStr  = String(h, 2);

    // ---- PUBLICACIÓN EN LOS TÓPICOS MQTT ----
    // AQUÍ ES DONDE REALMENTE ENVIAMOS A "temp" Y "hum"

    client.publish("/temp", tempStr.c_str());  // Tópico de temperatura
    client.publish("/hum", humStr.c_str());    // Tópico de humedad
  }
}
