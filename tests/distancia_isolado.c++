#define TRIG_PIN 12  // Pino de disparo (Trigger)
#define ECHO_PIN 14 // Pino de recepção (Echo)

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("Iniciando leitura do sensor HC-SR04...");
}

void loop() {
  // Limpa o pino TRIG
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Envia um pulso de 10 microsegundos no pino TRIG
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Leitura do tempo de resposta do pino ECHO (em microsegundos)
  long duracao = pulseIn(ECHO_PIN, HIGH);

  // Calcula a distância em centímetros
  // A velocidade do som é ~0.034 cm/us (distância = (tempo * velocidade) / 2)
  float distancia_cm = duracao * 0.0343 / 2;

  // Exibe a distância no Monitor Serial
  Serial.print("Distancia: ");
  Serial.print(distancia_cm);
  Serial.println(" cm");

  // Aguarda 500 ms antes da próxima leitura
  delay(500);
}