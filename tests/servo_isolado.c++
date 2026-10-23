#include <ESP32Servo.h>

Servo meuServo;

// Define o pino de controlo do servo (GPIO 18)
const int pinoServo = 14;

void setup() {
  Serial.begin(115200);
  
  // Aloca temporizadores para o ESP32 controlar o servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  // Define a frequência padrão para servos (50Hz)
  meuServo.setPeriodHertz(50);
  
  // Associa o servo ao pino especificado com os limites de pulso típicos do SG90 (500us a 2400us)
  meuServo.attach(pinoServo, 500, 2400);
  
  Serial.println("Iniciando teste do servo SG90...");
}

void loop() {
  // Move de 0 a 180 graus
  for (int pos = 0; pos <= 180; pos += 1) {
    meuServo.write(pos);
    delay(15);
  }
  
  delay(1000); // Aguarda 1 segundo a 180 graus
  
  // Move de 180 a 0 graus
  for (int pos = 180; pos >= 0; pos -= 1) {
    meuServo.write(pos);
    delay(15);
  }
  
  delay(1000); // Aguarda 1 segundo a 0 graus
}