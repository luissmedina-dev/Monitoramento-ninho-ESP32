// ==== MONITORAMENTO DE NINHOS COM ESP32 ====
// == CÓDIGOS QUE DERAM CERTO ==

// === 1 - HTML ===
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = ""; // NOME DA REDE WIFI
const char* password = "";// SENHA DA REDE

WebServer server(80);

void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:Arial;text-align:center;margin-top:50px;}";
  html += "input[type=text]{padding:10px;font-size:16px;width:80%;max-width:300px;margin-bottom:10px;}";
  html += "input[type=submit]{background-color:#4CAF50;color:white;padding:10px 20px;font-size:16px;border:none;border-radius:5px;cursor:pointer;}";
  html += "</style></head>";
  html += "<body><h1>Enviar Texto para o PC</h1>";
  html += "<form action=\"/enviar\" method=\"POST\">";
  html += "<input type=\"text\" name=\"mensagem\" placeholder=\"Digite algo aqui...\" required><br>";
  html += "<input type=\"submit\" value=\"Enviar Mensagem\">";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleEnviar() {
  if (server.hasArg("mensagem")) {
    String msg = server.arg("mensagem");
    
    // Exibe a mensagem recebida direto na tela do computador
    Serial.print("[Celular diz]: ");
    Serial.println(msg);
  }
  
  // Retorna para a página inicial após o envio
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConectado com sucesso!");
  Serial.print("Acesse no celular: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/enviar", handleEnviar);
  server.begin();
}

void loop() {
  server.handleClient();
}





