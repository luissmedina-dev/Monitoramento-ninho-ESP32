/* ESP32 WiFi Scanning example */

#include "DHT.h"

#define DHTPIN 12     // Pino do ESP32 conectado ao pino de dados do DHT22
#define DHTTYPE DHT22 // Modelo do sensor

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  Serial.println(F("Iniciando leitura do sensor DHT22..."));

  dht.begin();
}

void loop() {
  // Aguarda 2 segundos entre as leituras
  delay(2000);

  // Leitura da umidade e temperatura
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature(); // Em graus Celsius

  // Verifica se alguma leitura falhou
  if (isnan(umidade) || isnan(temperatura)) {
    Serial.println(F("Falha ao ler do sensor DHT!"));
    return;
  }

  // Exibe os valores no Monitor Serial
  Serial.print(F("Umidade: "));
  Serial.print(umidade);
  Serial.print(F("%  |  Temperatura: "));
  Serial.print(temperatura);
  Serial.println(F("°C"));
}