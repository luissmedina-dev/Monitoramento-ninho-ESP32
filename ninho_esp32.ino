/*
  ==========================================================================
   NINHO INTELIGENTE — ESP32
  ==========================================================================
   Junta os quatro testes em um único programa e mostra tudo numa página web:

   - HC-SR04 (distância): mede a distância do sensor até a comida.
   - Servo SG90: abre a comporta e libera comida quando o prato esvazia
     (automático) ou quando você aperta o botão na página (manual).
   - DHT22: temperatura e umidade do ninho, com gráfico das últimas 6 horas.
   - Célula de carga 1 kg + HX711: peso do filhote, com gráfico das pesagens
     (as pesagens ficam guardadas mesmo se o ESP32 desligar).

   TODOS OS PINOS FICAM DE UM LADO SÓ DA PLACA (o lado do GPIO 12/14, que
   também tem o pino 5V/VIN). Veja o mapa de ligações logo abaixo.

   Bibliotecas (Arduino IDE > Ferramentas > Gerenciar bibliotecas):
     - ESP32Servo                 (Kevin Harrington)
     - DHT sensor library         (Adafruit) + Adafruit Unified Sensor
     - HX711 Arduino Library      (Bogdan Necula)
   Placa: "ESP32 Dev Module" (ou "DOIT ESP32 DEVKIT V1").

   Como usar:
     1. Ajuste o nome e a senha do Wi-Fi na seção 1.
     2. Grave o código e abra o Monitor Serial em 115200.
     3. O endereço da página aparece no Monitor Serial (ex.: http://192.168.0.23).
        Em muitos celulares e computadores também funciona http://ninho.local
     4. Se o Wi-Fi não conectar, o ESP32 cria a rede "Ninho-ESP32"
        (senha ninho1234). Conecte nela e abra http://192.168.4.1

   Comandos pelo Monitor Serial (envie com "Nova linha"):
     t        -> zera (tara) a balança, que deve estar vazia
     c 100    -> calibra com um objeto de 100 g em cima da balança
     p        -> registra uma pesagem agora
     l        -> libera comida agora
  ==========================================================================
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <driver/gpio.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <HX711.h>

// ==========================================================================
// 1. WI-FI
// ==========================================================================
const char* WIFI_NOME  = "Fernanda";    // nome da rede Wi-Fi
const char* WIFI_SENHA = "123456Fer";   // senha da rede

const char* AP_NOME   = "Ninho-ESP32";  // rede própria, criada se o Wi-Fi falhar
const char* AP_SENHA  = "ninho1234";    // mínimo de 8 caracteres
const char* NOME_MDNS = "ninho";        // endereço http://ninho.local

// ==========================================================================
// 2. PINOS — todos do mesmo lado da placa
// ==========================================================================
//  Lado do ESP32 usado (placa de 30 pinos, de cima para baixo):
//    EN  VP  VN  34  35  32  33  25  26  27  14  12  13  GND  VIN
//                     |   |   |   |   |   |           |   |    |
//                  ECHO 3V3 SCK TRIG SERVO DHT        DT GND  5V
//  (Na placa de 38 pinos a ordem é parecida e ainda tem um pino 3V3 em cima.)
//
//  Por que estes pinos:
//   - GPIO 12 NÃO é usado: se ele estiver em nível alto na hora de ligar
//     (o DHT22 tem resistor de pull-up), o ESP32 não inicializa.
//   - GPIO 14 fica livre: ele solta pulsos na inicialização e o servo "treme".
//   - GPIO 35 só funciona como entrada: serve para o ECHO.
//   - Os testes antigos usavam o 12 e o 14 em mais de um sensor ao mesmo
//     tempo; aqui cada sensor tem o seu pino.
#define LADO_DOS_PINOS 1   // 1 = lado do GPIO 12/14 (recomendado); 2 = outro lado

#if LADO_DOS_PINOS == 1
const int PINO_SERVO      = 26;
const int PINO_TRIG       = 25;
const int PINO_ECHO       = 35;  // use o divisor de tensão (o ECHO sai com 5 V)
const int PINO_DHT        = 27;
const int PINO_HX711_DT   = 13;
const int PINO_HX711_SCK  = 33;
// Na placa de 30 pinos este lado NÃO tem o pino 3V3. Este GPIO fica sempre
// ligado e alimenta só o DHT22 e o HX711 (consomem poucos mA).
// Se o seu lado tiver o pino 3V3 de verdade, use-o e coloque -1 aqui.
const int PINO_3V3_AUX    = 32;
#else
// Outro lado da placa. Atenção: este lado não tem 5V, então o servo e o
// HC-SR04 precisariam de uma fonte de 5 V externa (com o GND em comum).
const int PINO_SERVO      = 23;
const int PINO_TRIG       = 22;
const int PINO_ECHO       = 21;  // use o divisor de tensão
const int PINO_DHT        = 19;
const int PINO_HX711_DT   = 18;
const int PINO_HX711_SCK  = 4;
const int PINO_3V3_AUX    = -1;  // a placa de 30 pinos tem 3V3 deste lado
#endif

// ==========================================================================
// 3. AJUSTES
// ==========================================================================
// --- Comedouro (sensor de distância + servo) ---
// O sensor fica em cima do prato, apontado para baixo. Quanto MAIS longe a
// comida, MENOS comida tem no prato.
const float DIST_PRATO_CHEIO_CM = 4.0;   // distância com o prato cheio
const float DIST_PRATO_VAZIO_CM = 12.0;  // a partir daqui: "pouca comida"
const int   ANGULO_FECHADO = 0;          // posição do servo com a comporta fechada
const int   ANGULO_ABERTO  = 90;         // posição com a comporta aberta
const unsigned long TEMPO_ABERTO_MS            = 800;            // quanto tempo a comporta fica aberta
const unsigned long TEMPO_CONFIRMA_VAZIO_MS    = 3000;           // prato precisa ficar "vazio" por 3 s
const unsigned long INTERVALO_MIN_LIBERACAO_MS = 5UL * 60 * 1000; // no mínimo 5 min entre liberações automáticas

// --- Balança (célula de carga 1 kg + HX711) ---
const float FATOR_CALIBRACAO_PADRAO = 420.0;  // valor inicial; calibre pela página
const float PESO_MINIMO_G           = 5.0;    // abaixo disso a balança é considerada vazia
const float TOLERANCIA_ESTAVEL_G    = 2.0;    // variação aceita para considerar o peso estável
const unsigned long TEMPO_ESTAVEL_MS        = 4000;               // peso estável por 4 s = pesagem automática
const unsigned long INTERVALO_REPESAGEM_MS  = 60UL * 60 * 1000;   // filhote parado na balança: nova pesagem a cada 1 h

// --- Ninho (DHT22) ---
const float TEMP_MIN_C = 25.0;   // faixa ideal: AJUSTE para a espécie do filhote
const float TEMP_MAX_C = 35.0;
const unsigned long INTERVALO_HIST_TEMP_MS = 60UL * 1000;  // 1 ponto por minuto no gráfico

// ==========================================================================
// OBJETOS E VARIÁVEIS
// ==========================================================================
WebServer server(80);
Servo servo;
DHT dht(PINO_DHT, DHT22);
HX711 balanca;
Preferences prefs;
bool modoAP = false;

// Leituras atuais
float temperatura = NAN, umidade = NAN;
float distanciaCm = NAN;
float pesoG = NAN;
int falhasDHT = 0;

// --- Distância: mediana das últimas 5 leituras (filtra leituras malucas) ---
const int N_DIST = 5;
float leiturasDist[N_DIST];
int idxDist = 0, nDist = 0;

// --- Comedouro ---
bool alimentacaoAuto = true;
bool comportaAberta = false;
unsigned long tAbriu = 0, tFechou = 0, tUltimaLiberacao = 0, tInicioVazio = 0;
bool jaLiberou = false, contandoVazio = false;
uint32_t totalLiberacoes = 0;
time_t horaUltimaLiberacao = 0;
char origemUltima[16] = "";

// --- Balança: média das últimas 10 leituras ---
const int N_PESO = 10;
long brutos[N_PESO];
int idxPeso = 0, nPeso = 0;
double mediaBruta = 0;
long offsetBalanca = 0;
float fatorBalanca = FATOR_CALIBRACAO_PADRAO;
bool balancaOk = false, tararAoIniciar = false;
unsigned long tUltimaLeituraHX = 0;
// detecção de peso estável
float pesoRef = 0;
unsigned long tPesoRef = 0, tUltimaPesagemAuto = 0;
bool pesadoNestaVez = false;

// --- Pesagens guardadas na memória permanente (NVS) ---
struct Pesagem {
  uint32_t epoch;   // data/hora (segundos desde 1970, UTC); 0 = sem horário
  float gramas;
};
const int MAX_PESAGENS = 120;
Pesagem pesagens[MAX_PESAGENS];
uint32_t msPesagem[MAX_PESAGENS];  // millis() da pesagem (para acertar a hora depois)
int nPesagens = 0;
uint32_t versaoPesagens = 0;       // muda a cada alteração; a página recarrega o gráfico

// --- Histórico de temperatura (memória RAM, últimas 6 horas) ---
struct PontoTemp {
  uint32_t ms;
  float temp;
  float umid;
};
const int MAX_HIST = 360;
PontoTemp hist[MAX_HIST];
int histInicio = 0, histN = 0;
unsigned long tUltimoHist = 0;

// Temporizadores do loop
unsigned long tDist = 0, tDHT = 0, tStatus = 0;
bool horaJaValida = false;

// ==========================================================================
// HORA (NTP pela internet ou pelo relógio do navegador)
// ==========================================================================
bool horaValida() {
  return time(nullptr) > 1700000000;  // depois de nov/2023
}

void corrigirPesagensSemHora() {
  uint32_t agoraEpoch = time(nullptr);
  uint32_t agoraMs = millis();
  bool mudou = false;
  for (int i = 0; i < nPesagens; i++) {
    if (pesagens[i].epoch == 0 && msPesagem[i] != 0) {
      pesagens[i].epoch = agoraEpoch - (agoraMs - msPesagem[i]) / 1000;
      mudou = true;
    }
  }
  if (mudou) {
    prefs.putBytes("pesagens", pesagens, nPesagens * sizeof(Pesagem));
    versaoPesagens++;
  }
}

// ==========================================================================
// SENSOR DE DISTÂNCIA (HC-SR04)
// ==========================================================================
float medirDistanciaCm() {
  digitalWrite(PINO_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PINO_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PINO_TRIG, LOW);
  // Espera no máximo 30 ms (cerca de 5 m). Sem esse limite, um sensor
  // desligado travaria o programa por 1 segundo a cada leitura.
  unsigned long duracao = pulseIn(PINO_ECHO, HIGH, 30000UL);
  if (duracao == 0) return NAN;
  float cm = duracao * 0.0343f / 2.0f;
  if (cm < 2.0f || cm > 400.0f) return NAN;
  return cm;
}

void atualizarDistancia() {
  leiturasDist[idxDist] = medirDistanciaCm();
  idxDist = (idxDist + 1) % N_DIST;
  if (nDist < N_DIST) nDist++;

  float v[N_DIST];
  int n = 0;
  for (int i = 0; i < nDist; i++) {
    if (!isnan(leiturasDist[i])) v[n++] = leiturasDist[i];
  }
  if (n < 3) {           // poucas leituras válidas: sensor com problema
    distanciaCm = NAN;
    return;
  }
  for (int i = 1; i < n; i++) {   // ordena para pegar a mediana
    float x = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
    v[j + 1] = x;
  }
  distanciaCm = v[n / 2];
}

bool pratoVazio() {
  return !isnan(distanciaCm) && distanciaCm >= DIST_PRATO_VAZIO_CM;
}

// ==========================================================================
// COMEDOURO (SERVO)
// ==========================================================================
bool abrirComporta(const char* origem) {
  if (comportaAberta) return false;
  servo.write(ANGULO_ABERTO);
  comportaAberta = true;
  tAbriu = millis();
  tUltimaLiberacao = tAbriu;
  jaLiberou = true;
  totalLiberacoes++;
  horaUltimaLiberacao = horaValida() ? time(nullptr) : 0;
  strlcpy(origemUltima, origem, sizeof(origemUltima));
  Serial.printf("Comida liberada (%s). Distância: %.1f cm\n", origem, distanciaCm);
  return true;
}

// Não usa delay(): a página continua respondendo enquanto a comporta está aberta.
void atualizarComedouro() {
  unsigned long agora = millis();

  if (comportaAberta && agora - tAbriu >= TEMPO_ABERTO_MS) {
    servo.write(ANGULO_FECHADO);
    comportaAberta = false;
    tFechou = agora;
  }

  if (!pratoVazio()) {
    contandoVazio = false;
    return;
  }
  if (!contandoVazio) {
    contandoVazio = true;
    tInicioVazio = agora;
  }
  if (!alimentacaoAuto || comportaAberta) return;
  if (agora - tInicioVazio < TEMPO_CONFIRMA_VAZIO_MS) return;  // confirma que esvaziou mesmo
  if (agora - tFechou < 5000) return;                          // espera a comida assentar
  if (jaLiberou && agora - tUltimaLiberacao < INTERVALO_MIN_LIBERACAO_MS) return;

  abrirComporta("automática");
  contandoVazio = false;
}

// ==========================================================================
// TEMPERATURA (DHT22)
// ==========================================================================
void lerDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    if (++falhasDHT >= 3) {      // 3 falhas seguidas: mostra "sem leitura"
      temperatura = NAN;
      umidade = NAN;
    }
    return;
  }
  falhasDHT = 0;
  temperatura = t;
  umidade = h;
}

void guardarHistoricoTemp() {
  if (isnan(temperatura)) return;
  unsigned long agora = millis();
  if (histN > 0 && agora - tUltimoHist < INTERVALO_HIST_TEMP_MS) return;
  tUltimoHist = agora;
  int pos = (histInicio + histN) % MAX_HIST;
  hist[pos] = { (uint32_t)agora, temperatura, umidade };
  if (histN < MAX_HIST) histN++;
  else histInicio = (histInicio + 1) % MAX_HIST;  // cheio: descarta o mais antigo
}

// ==========================================================================
// BALANÇA (HX711)
// ==========================================================================
void registrarPesagem(float gramas, bool automatica) {
  if (nPesagens == MAX_PESAGENS) {  // cheio: descarta a pesagem mais antiga
    memmove(pesagens, pesagens + 1, (MAX_PESAGENS - 1) * sizeof(Pesagem));
    memmove(msPesagem, msPesagem + 1, (MAX_PESAGENS - 1) * sizeof(uint32_t));
    nPesagens--;
  }
  pesagens[nPesagens].epoch = horaValida() ? (uint32_t)time(nullptr) : 0;
  pesagens[nPesagens].gramas = roundf(gramas * 10.0f) / 10.0f;
  msPesagem[nPesagens] = millis();
  if (msPesagem[nPesagens] == 0) msPesagem[nPesagens] = 1;
  nPesagens++;
  prefs.putBytes("pesagens", pesagens, nPesagens * sizeof(Pesagem));
  versaoPesagens++;
  Serial.printf("Pesagem %s registrada: %.1f g\n", automatica ? "automática" : "manual", gramas);
}

void verificarPesagemAutomatica() {
  if (nPeso < N_PESO || isnan(pesoG)) return;
  unsigned long agora = millis();

  if (pesoG < PESO_MINIMO_G) {          // balança vazia: pronto para a próxima pesagem
    pesadoNestaVez = false;
    pesoRef = pesoG;
    tPesoRef = agora;
    return;
  }
  if (fabsf(pesoG - pesoRef) > TOLERANCIA_ESTAVEL_G) {  // ainda mexendo
    pesoRef = pesoG;
    tPesoRef = agora;
    return;
  }
  if (agora - tPesoRef < TEMPO_ESTAVEL_MS) return;

  bool repesar = pesadoNestaVez && (agora - tUltimaPesagemAuto >= INTERVALO_REPESAGEM_MS);
  if (pesadoNestaVez && !repesar) return;

  registrarPesagem(pesoG, true);
  pesadoNestaVez = true;
  tUltimaPesagemAuto = agora;
}

// Lê o HX711 só quando ele tem um valor pronto (não trava o programa).
void atualizarBalanca() {
  if (!balanca.is_ready()) {
    if (millis() - tUltimaLeituraHX > 2000) {
      balancaOk = false;
      pesoG = NAN;
    }
    return;
  }
  long bruto = balanca.read();
  if (bruto == 0x7FFFFF || bruto == -0x800000) return;  // valor saturado: ignora
  tUltimaLeituraHX = millis();
  balancaOk = true;

  brutos[idxPeso] = bruto;
  idxPeso = (idxPeso + 1) % N_PESO;
  if (nPeso < N_PESO) nPeso++;
  double soma = 0;
  for (int i = 0; i < nPeso; i++) soma += brutos[i];
  mediaBruta = soma / nPeso;

  if (tararAoIniciar && nPeso == N_PESO) {  // primeira vez: zera com a balança vazia
    offsetBalanca = lround(mediaBruta);
    prefs.putLong("offset", offsetBalanca);
    tararAoIniciar = false;
    Serial.println("Balança zerada automaticamente (primeira vez).");
  }

  float p = (mediaBruta - offsetBalanca) / fatorBalanca;
  if (fabsf(p) < 0.5f) p = 0;   // esconde o "ruído" perto de zero
  pesoG = p;
  verificarPesagemAutomatica();
}

bool tararBalanca() {
  if (!balancaOk || nPeso == 0) return false;
  offsetBalanca = lround(mediaBruta);
  prefs.putLong("offset", offsetBalanca);
  pesadoNestaVez = false;
  Serial.println("Balança zerada.");
  return true;
}

// Retorna "" se deu certo ou a mensagem de erro.
String calibrarBalanca(float massaG) {
  if (massaG <= 0) return "Informe o peso do objeto em gramas";
  if (!balancaOk || nPeso < N_PESO) return "Balança sem leitura";
  double diferenca = mediaBruta - offsetBalanca;
  if (fabs(diferenca) < 500) return "Coloque o objeto na balança antes de calibrar";
  fatorBalanca = diferenca / massaG;
  prefs.putFloat("fator", fatorBalanca);
  Serial.printf("Balança calibrada. Novo fator: %.2f\n", fatorBalanca);
  return "";
}

// ==========================================================================
// PÁGINA WEB (fica guardada na memória flash do ESP32)
// ==========================================================================
const char PAGINA[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Ninho do filhote</title>
<style>
:root{--fundo:#f6f1ea;--cartao:#fff;--texto:#2b2420;--suave:#7a6e66;--linha:#e9e0d6;--marca:#c4622d;--ok:#3f8a5c;--alerta:#c4412a;--frio:#3d6fa8;--okf:#e3f1e8;--alertaf:#fbe4dd;--friof:#e3ecf7}
@media (prefers-color-scheme:dark){:root{--fundo:#1b1714;--cartao:#26201c;--texto:#f1e9e2;--suave:#a89c93;--linha:#3a312b;--okf:#1f3a2a;--alertaf:#4a2219;--friof:#1d2f45;--ok:#7fc79a;--alerta:#f08a72;--frio:#8ab4e8}}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--fundo);color:var(--texto)}
header,main{max-width:980px;margin:0 auto;padding-left:16px;padding-right:16px}
header{padding-top:20px;padding-bottom:6px}
h1{font-size:1.5rem;margin:0}
#estado{color:var(--suave);font-size:.85rem;margin-top:4px}
main{padding-top:8px;padding-bottom:32px;display:grid;gap:14px;grid-template-columns:repeat(auto-fit,minmax(270px,1fr))}
.cartao{background:var(--cartao);border:1px solid var(--linha);border-radius:14px;padding:16px;min-width:0}
.largo{grid-column:1/-1}
.rotulo{font-size:.76rem;font-weight:700;letter-spacing:.06em;text-transform:uppercase;color:var(--suave)}
.valor{font-size:2.4rem;font-weight:700;margin:6px 0 2px;font-variant-numeric:tabular-nums}
.valor small{font-size:1rem;font-weight:500;color:var(--suave)}
.info{font-size:.9rem;color:var(--suave);margin:4px 0}
.info b{color:var(--texto)}
.selo{display:inline-block;margin-top:6px;padding:3px 10px;border-radius:99px;font-size:.8rem;font-weight:600;background:var(--linha)}
.selo.ok{background:var(--okf);color:var(--ok)}.selo.alerta{background:var(--alertaf);color:var(--alerta)}.selo.frio{background:var(--friof);color:var(--frio)}
.botoes{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}
button{font:inherit;font-size:.9rem;padding:9px 14px;border-radius:10px;border:1px solid var(--linha);background:var(--cartao);color:var(--texto);cursor:pointer}
button.principal{background:var(--marca);border-color:var(--marca);color:#fff;font-weight:600}
button:disabled{opacity:.5;cursor:default}
.barra{height:10px;background:var(--linha);border-radius:99px;overflow:hidden;margin:10px 0 2px}
.barra div{height:100%;width:0;background:var(--ok);transition:width .4s}
.chave{display:flex;align-items:center;gap:8px;margin-top:12px;font-size:.9rem}
.chave input{width:18px;height:18px;accent-color:var(--marca)}
details{margin-top:12px;font-size:.9rem}summary{cursor:pointer;color:var(--suave)}
input[type=number]{font:inherit;width:100px;padding:8px;border-radius:8px;border:1px solid var(--linha);background:var(--fundo);color:var(--texto)}
.cabeca{display:flex;justify-content:space-between;align-items:baseline;gap:8px;flex-wrap:wrap}
.links{display:flex;gap:14px}.links a{color:var(--marca);font-size:.85rem;text-decoration:none}
.grafico{margin-top:10px;min-height:190px}
.grafico svg{display:block;overflow:visible}
.grade{stroke:var(--linha);stroke-width:1}
.eixo{fill:var(--suave);font-size:11px}
.linha{fill:none;stroke-width:2.2;stroke-linejoin:round;stroke-linecap:round}
.faixa{fill:var(--ok);opacity:.1}
.vazio{color:var(--suave);font-size:.9rem;padding:70px 0;text-align:center}
#aviso{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);max-width:90%;background:var(--texto);color:var(--fundo);padding:10px 16px;border-radius:10px;font-size:.9rem;opacity:0;transition:opacity .3s;pointer-events:none}
#aviso.ver{opacity:1}
</style>
</head>
<body>
<header>
  <h1>Ninho do filhote</h1>
  <div id="estado">Conectando ao ESP32…</div>
</header>
<main>
  <section class="cartao">
    <div class="rotulo">Temperatura do ninho</div>
    <div class="valor" id="temp">--<small> °C</small></div>
    <div class="info">Umidade: <b id="umid">--</b> %</div>
    <div class="info">Faixa ideal: <span id="faixa">--</span></div>
    <span class="selo" id="seloTemp">--</span>
  </section>

  <section class="cartao">
    <div class="rotulo">Peso do filhote</div>
    <div class="valor" id="peso">--<small> g</small></div>
    <div class="info" id="infoPeso">Coloque o filhote na balança: o peso é registrado sozinho quando fica estável.</div>
    <div class="botoes">
      <button class="principal" onclick="acao('/pesar')">Registrar pesagem</button>
      <button onclick="acao('/tarar')">Zerar balança</button>
    </div>
    <details>
      <summary>Calibrar a balança</summary>
      <p class="info">1. Com a balança vazia, toque em <b>Zerar balança</b>.<br>2. Coloque um objeto de peso conhecido, informe o peso e toque em <b>Calibrar</b>.</p>
      <input type="number" id="massa" value="100" min="1" step="0.1"> g
      <button onclick="calibrar()">Calibrar</button>
    </details>
  </section>

  <section class="cartao">
    <div class="rotulo">Comida</div>
    <div class="valor" id="dist">--<small> cm</small></div>
    <div class="info">Distância do sensor até a comida</div>
    <div class="barra"><div id="nivel"></div></div>
    <span class="selo" id="seloComida">--</span>
    <label class="chave"><input type="checkbox" id="auto" onchange="acao('/auto?on='+(this.checked?1:0))"> Liberar comida automaticamente</label>
    <div class="info" id="infoLib">--</div>
    <div class="botoes"><button class="principal" id="btnLiberar" onclick="acao('/liberar')">Liberar comida agora</button></div>
  </section>

  <section class="cartao largo">
    <div class="cabeca">
      <div class="rotulo">Crescimento do filhote (pesagens)</div>
      <div class="links"><a href="/pesagens.csv">Baixar planilha</a><a href="#" onclick="limpar();return false">Apagar pesagens</a></div>
    </div>
    <div class="grafico" id="gPeso"></div>
    <div class="info" id="resumoPeso"></div>
  </section>

  <section class="cartao largo">
    <div class="rotulo">Temperatura do ninho: últimas 6 horas</div>
    <div class="grafico" id="gTemp"></div>
  </section>
</main>
<div id="aviso"></div>

<script>
const $=id=>document.getElementById(id);
let dados=null,pesagens=[],histTemp=[],versaoPesagens=-1;
const fmt=(v,c)=>v==null||isNaN(v)?'--':Number(v).toFixed(c).replace('.',',');
const hhmm=ms=>new Date(ms).toLocaleTimeString('pt-BR',{hour:'2-digit',minute:'2-digit'});
const ddmm=ms=>new Date(ms).toLocaleDateString('pt-BR',{day:'2-digit',month:'2-digit'});
const dataHora=ms=>ddmm(ms)+' '+hhmm(ms);

function aviso(t){const a=$('aviso');a.textContent=t;a.classList.add('ver');clearTimeout(a.t);a.t=setTimeout(()=>a.classList.remove('ver'),2800);}
async function pegar(url){const r=await fetch(url,{cache:'no-store'});if(!r.ok)throw new Error(r.status);return r.json();}
async function acao(url){
  try{const r=await fetch(url,{method:'POST'});const j=await r.json();aviso(j.msg);atualizar();}
  catch(e){aviso('Não consegui falar com o ESP32');}
}
function calibrar(){
  const g=parseFloat(String($('massa').value).replace(',','.'));
  if(!(g>0)){aviso('Informe o peso do objeto em gramas');return;}
  acao('/calibrar?g='+g);
}
function limpar(){if(confirm('Apagar todas as pesagens registradas?'))acao('/limpar');}
function haQuanto(s){if(s<60)return 'há '+s+' s';if(s<3600)return 'há '+Math.round(s/60)+' min';return 'há '+fmt(s/3600,1)+' h';}
function selo(el,classe,texto){el.className='selo '+classe;el.textContent=texto;}

async function atualizar(){
  let d;
  try{d=await pegar('/dados');}catch(e){$('estado').textContent='Sem conexão com o ESP32. Tentando de novo…';return;}
  dados=d;
  // Ninho
  $('temp').innerHTML=fmt(d.temp,1)+'<small> °C</small>';
  $('umid').textContent=fmt(d.umid,0);
  $('faixa').textContent=fmt(d.temp_min,0)+' a '+fmt(d.temp_max,0)+' °C';
  if(d.temp==null)selo($('seloTemp'),'alerta','Sensor sem leitura');
  else if(d.temp<d.temp_min)selo($('seloTemp'),'frio','Ninho frio');
  else if(d.temp>d.temp_max)selo($('seloTemp'),'alerta','Ninho quente');
  else selo($('seloTemp'),'ok','Temperatura boa');
  // Balança
  $('peso').innerHTML=(d.balanca_ok?fmt(d.peso,1):'--')+'<small> g</small>';
  $('infoPeso').textContent=d.balanca_ok?'Coloque o filhote na balança: o peso é registrado sozinho quando fica estável.':'Balança sem resposta: confira os fios do HX711.';
  // Comida
  $('dist').innerHTML=fmt(d.dist,1)+'<small> cm</small>';
  let nivel=0;
  if(d.dist!=null)nivel=Math.min(1,Math.max(0,(d.dist_vazio-d.dist)/(d.dist_vazio-d.dist_cheio)));
  $('nivel').style.width=(nivel*100)+'%';
  $('nivel').style.background=d.vazio?'var(--alerta)':'var(--ok)';
  if(d.dist==null)selo($('seloComida'),'alerta','Sensor sem leitura');
  else if(d.vazio)selo($('seloComida'),'alerta','Pouca comida');
  else selo($('seloComida'),'ok','Tem comida');
  if(document.activeElement!==$('auto'))$('auto').checked=d.auto;
  let txt='Nenhuma liberação desde que o ESP32 ligou.';
  if(d.liberacoes>0){
    txt='Última liberação: '+(d.ultima_lib?dataHora(d.ultima_lib*1000):haQuanto(d.ultima_lib_ha_s))+' ('+d.origem+'). Total: '+d.liberacoes+'.';
  }
  if(d.auto&&d.espera_s>0)txt+=' Próxima automática possível em '+Math.ceil(d.espera_s/60)+' min.';
  $('infoLib').textContent=txt;
  $('btnLiberar').disabled=d.aberta;
  // Rodapé
  $('estado').textContent=(d.modo_ap?'Rede própria do ESP32':'Wi-Fi conectado ('+d.rssi+' dBm)')+' · atualizado às '+new Date().toLocaleTimeString('pt-BR');
  if(d.pesagens_ver!==versaoPesagens){versaoPesagens=d.pesagens_ver;carregarPesagens();}
}

// Gráfico de linha simples em SVG (funciona sem internet)
function grafico(el,pts,o){
  const W=Math.max(el.clientWidth,260),H=190,m={l:46,r:14,t:12,b:26};
  if(!pts.length){el.innerHTML='<div class="vazio">'+o.vazio+'</div>';return;}
  let x0=pts[0].x,x1=x0,y0=pts[0].y,y1=y0;
  for(const p of pts){x0=Math.min(x0,p.x);x1=Math.max(x1,p.x);y0=Math.min(y0,p.y);y1=Math.max(y1,p.y);}
  if(x1===x0){x0-=o.xpad;x1+=o.xpad;}
  const folga=Math.max((y1-y0)*0.1,o.folgaMin);y0-=folga;y1+=folga;
  // marcas "redondas" no eixo vertical (ex.: 20, 40, 60...)
  const bruto=(y1-y0)/4,mag=Math.pow(10,Math.floor(Math.log10(bruto))),r=bruto/mag;
  const passo=(r<=1?1:r<=2?2:r<=2.5?2.5:r<=5?5:10)*mag,casasEixo=passo>=1?0:passo>=0.1?1:2;
  y0=Math.floor(y0/passo)*passo;y1=Math.ceil(y1/passo)*passo;
  const X=v=>m.l+(v-x0)/(x1-x0)*(W-m.l-m.r),Y=v=>H-m.b-(v-y0)/(y1-y0)*(H-m.t-m.b);
  let s='<svg viewBox="0 0 '+W+' '+H+'" width="100%" height="'+H+'">';
  if(o.faixa){const a=Math.max(o.faixa[0],y0),b=Math.min(o.faixa[1],y1);
    if(b>a)s+='<rect class="faixa" x="'+m.l+'" width="'+(W-m.l-m.r)+'" y="'+Y(b)+'" height="'+(Y(a)-Y(b))+'"/>';}
  for(let v=y0;v<=y1+passo/1000;v+=passo){const y=Y(v);
    s+='<line class="grade" x1="'+m.l+'" x2="'+(W-m.r)+'" y1="'+y+'" y2="'+y+'"/>';
    s+='<text class="eixo" x="'+(m.l-6)+'" y="'+(y+4)+'" text-anchor="end">'+fmt(v,casasEixo)+'</text>';}
  [[x0,'start'],[(x0+x1)/2,'middle'],[x1,'end']].forEach(([v,a])=>{
    s+='<text class="eixo" x="'+X(v)+'" y="'+(H-6)+'" text-anchor="'+a+'">'+o.rotuloX(v)+'</text>';});
  s+='<path class="linha" stroke="'+o.cor+'" d="'+pts.map((p,i)=>(i?'L':'M')+X(p.x).toFixed(1)+' '+Y(p.y).toFixed(1)).join(' ')+'"/>';
  if(o.pontos)for(const p of pts)
    s+='<circle cx="'+X(p.x).toFixed(1)+'" cy="'+Y(p.y).toFixed(1)+'" r="4" fill="'+o.cor+'"><title>'+o.dica(p.x)+': '+fmt(p.y,1)+' '+o.un+'</title></circle>';
  el.innerHTML=s+'</svg>';
}

async function carregarPesagens(){
  try{pesagens=(await pegar('/pesagens')).pontos;desenharPeso();}catch(e){}
}
function desenharPeso(){
  const semHora=pesagens.some(p=>!p[0]);  // pesagens feitas sem relógio: usa a ordem
  const pts=pesagens.map((p,i)=>({x:semHora?i:p[0]*1000,y:p[1]}));
  const dias=pts.length>1&&!semHora&&pts[pts.length-1].x-pts[0].x>86400000;
  grafico($('gPeso'),pts,{cor:'#c4622d',un:'g',pontos:true,folgaMin:5,
    xpad:semHora?1:60000,vazio:'Nenhuma pesagem ainda. Coloque o filhote na balança.',
    rotuloX:semHora?(v=>v<0||v>pts.length-1?'':'#'+(Math.round(v)+1)):(v=>dias?ddmm(v):hhmm(v)),
    dica:semHora?(v=>'Pesagem #'+(v+1)):dataHora});
  if(!pts.length){$('resumoPeso').textContent='';return;}
  const a=pts[0].y,b=pts[pts.length-1].y,dif=b-a;
  $('resumoPeso').textContent=pts.length+(pts.length>1?' pesagens':' pesagem')+' · última: '+fmt(b,1)+' g'+
    (pts.length>1?' · variação desde a primeira: '+(dif>=0?'+':'')+fmt(dif,1)+' g':'');
}

async function carregarTemp(){
  try{const j=await pegar('/historico');const agora=Date.now();
    histTemp=j.pontos.map(p=>({x:agora-(j.agora-p[0]),y:p[1]}));desenharTemp();}catch(e){}
}
function desenharTemp(){
  const d=dados||{};
  grafico($('gTemp'),histTemp,{cor:'#d08a1e',un:'°C',folgaMin:1,xpad:60000,
    faixa:d.temp_min!=null?[d.temp_min,d.temp_max]:null,
    vazio:'Aguardando as primeiras leituras (1 ponto por minuto).',rotuloX:hhmm,dica:dataHora});
}

(async()=>{
  try{await fetch('/hora?ts='+Math.floor(Date.now()/1000));}catch(e){}  // acerta o relógio do ESP32 se ele não tiver internet
  await atualizar();
  carregarTemp();
  setInterval(atualizar,2000);
  setInterval(carregarTemp,60000);
  window.addEventListener('resize',()=>{desenharPeso();desenharTemp();});
})();
</script>
</body>
</html>
)rawliteral";

// ==========================================================================
// SERVIDOR WEB
// ==========================================================================
String num(float v, int casas) {
  if (isnan(v) || isinf(v)) return "null";
  return String(v, casas);
}

String comVirgula(float v, int casas) {   // 124.6 -> "124,6" (para as mensagens)
  String s = String(v, casas);
  s.replace(".", ",");
  return s;
}

void addCampo(String& j, const char* nome, const String& valor) {
  j += (j.length() > 1) ? ",\"" : "\"";
  j += nome;
  j += "\":";
  j += valor;
}

void responder(bool ok, const String& msg) {
  String j = "{\"ok\":";
  j += ok ? "true" : "false";
  j += ",\"msg\":\"";
  j += msg;
  j += "\"}";
  server.send(ok ? 200 : 400, "application/json", j);
}

void handlePagina() {
  server.send_P(200, "text/html; charset=utf-8", PAGINA);
}

void handleDados() {
  unsigned long agora = millis();
  long espera = 0;
  if (jaLiberou) {
    long falta = (long)INTERVALO_MIN_LIBERACAO_MS - (long)(agora - tUltimaLiberacao);
    if (falta > 0) espera = falta / 1000;
  }
  String j = "{";
  j.reserve(700);
  addCampo(j, "temp", num(temperatura, 1));
  addCampo(j, "umid", num(umidade, 0));
  addCampo(j, "temp_min", String(TEMP_MIN_C, 1));
  addCampo(j, "temp_max", String(TEMP_MAX_C, 1));
  addCampo(j, "peso", num(pesoG, 1));
  addCampo(j, "balanca_ok", balancaOk ? "true" : "false");
  addCampo(j, "dist", num(distanciaCm, 1));
  addCampo(j, "dist_cheio", String(DIST_PRATO_CHEIO_CM, 1));
  addCampo(j, "dist_vazio", String(DIST_PRATO_VAZIO_CM, 1));
  addCampo(j, "vazio", pratoVazio() ? "true" : "false");
  addCampo(j, "auto", alimentacaoAuto ? "true" : "false");
  addCampo(j, "aberta", comportaAberta ? "true" : "false");
  addCampo(j, "liberacoes", String(totalLiberacoes));
  addCampo(j, "ultima_lib", String((uint32_t)horaUltimaLiberacao));
  addCampo(j, "ultima_lib_ha_s", String(jaLiberou ? (long)((agora - tUltimaLiberacao) / 1000) : -1L));
  addCampo(j, "origem", String("\"") + origemUltima + "\"");
  addCampo(j, "espera_s", String(espera));
  addCampo(j, "pesagens_ver", String(versaoPesagens));
  addCampo(j, "modo_ap", modoAP ? "true" : "false");
  addCampo(j, "rssi", String(modoAP ? 0 : WiFi.RSSI()));
  addCampo(j, "hora_ok", horaValida() ? "true" : "false");
  j += "}";
  server.send(200, "application/json", j);
}

void handleHistorico() {
  String j;
  j.reserve(histN * 22 + 40);
  j += "{\"agora\":";
  j += String((uint32_t)millis());
  j += ",\"pontos\":[";
  for (int i = 0; i < histN; i++) {
    const PontoTemp& p = hist[(histInicio + i) % MAX_HIST];
    if (i) j += ',';
    j += '[';
    j += String(p.ms);
    j += ',';
    j += num(p.temp, 1);
    j += ',';
    j += num(p.umid, 0);
    j += ']';
  }
  j += "]}";
  server.send(200, "application/json", j);
}

void handlePesagens() {
  String j;
  j.reserve(nPesagens * 20 + 20);
  j += "{\"pontos\":[";
  for (int i = 0; i < nPesagens; i++) {
    if (i) j += ',';
    j += '[';
    j += String(pesagens[i].epoch);
    j += ',';
    j += String(pesagens[i].gramas, 1);
    j += ']';
  }
  j += "]}";
  server.send(200, "application/json", j);
}

// Planilha no formato do Excel brasileiro (separador ";" e vírgula decimal)
void handleCsv() {
  String c = "data;hora;peso_g\n";
  for (int i = 0; i < nPesagens; i++) {
    char data[12] = "", hora[8] = "";
    if (pesagens[i].epoch) {
      time_t t = pesagens[i].epoch;
      struct tm tmv;
      localtime_r(&t, &tmv);
      strftime(data, sizeof(data), "%d/%m/%Y", &tmv);
      strftime(hora, sizeof(hora), "%H:%M", &tmv);
    }
    String g = String(pesagens[i].gramas, 1);
    g.replace(".", ",");
    c += data; c += ';'; c += hora; c += ';'; c += g; c += '\n';
  }
  server.sendHeader("Content-Disposition", "attachment; filename=pesagens.csv");
  server.send(200, "text/csv; charset=utf-8", c);
}

void handleHora() {
  if (!horaValida() && server.hasArg("ts")) {
    long ts = server.arg("ts").toInt();
    if (ts > 1700000000L) {
      struct timeval tv = { (time_t)ts, 0 };
      settimeofday(&tv, nullptr);
      Serial.println("Relógio acertado pelo navegador.");
    }
  }
  server.send(200, "application/json", horaValida() ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleLiberar() {
  if (abrirComporta("manual")) responder(true, "Comida liberada");
  else responder(false, "A comporta já está aberta");
}

void handleAuto() {
  alimentacaoAuto = server.arg("on") == "1";
  prefs.putBool("auto", alimentacaoAuto);
  responder(true, alimentacaoAuto ? "Alimentação automática ligada" : "Alimentação automática desligada");
}

void handleTarar() {
  if (tararBalanca()) responder(true, "Balança zerada");
  else responder(false, "Balança sem leitura: confira os fios do HX711");
}

void handleCalibrar() {
  String erro = calibrarBalanca(server.arg("g").toFloat());
  if (erro.length()) responder(false, erro);
  else responder(true, "Balança calibrada (fator " + comVirgula(fatorBalanca, 1) + ")");
}

void handlePesar() {
  if (!balancaOk || isnan(pesoG)) { responder(false, "Balança sem leitura"); return; }
  if (pesoG < PESO_MINIMO_G) { responder(false, "Peso muito baixo. O filhote está na balança?"); return; }
  registrarPesagem(pesoG, false);
  responder(true, "Pesagem registrada: " + comVirgula(pesoG, 1) + " g");
}

void handleLimpar() {
  nPesagens = 0;
  prefs.remove("pesagens");
  versaoPesagens++;
  responder(true, "Pesagens apagadas");
}

void configurarServidor() {
  server.on("/", HTTP_GET, handlePagina);
  server.on("/dados", HTTP_GET, handleDados);
  server.on("/historico", HTTP_GET, handleHistorico);
  server.on("/pesagens", HTTP_GET, handlePesagens);
  server.on("/pesagens.csv", HTTP_GET, handleCsv);
  server.on("/hora", HTTP_GET, handleHora);
  server.on("/liberar", HTTP_POST, handleLiberar);
  server.on("/auto", HTTP_POST, handleAuto);
  server.on("/tarar", HTTP_POST, handleTarar);
  server.on("/calibrar", HTTP_POST, handleCalibrar);
  server.on("/pesar", HTTP_POST, handlePesar);
  server.on("/limpar", HTTP_POST, handleLimpar);
  server.onNotFound([]() { server.send(404, "text/plain", "Pagina nao encontrada"); });
  server.begin();
}

// ==========================================================================
// WI-FI
// ==========================================================================
void conectarWiFi() {
  WiFi.setHostname(NOME_MDNS);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
  Serial.printf("Conectando ao Wi-Fi \"%s\"", WIFI_NOME);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    modoAP = false;
    WiFi.setAutoReconnect(true);
    configTzTime("<-03>3", "a.st1.ntp.br", "pool.ntp.org");  // horário de Brasília
    Serial.printf("Conectado! Abra no navegador: http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    modoAP = true;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_NOME, AP_SENHA);
    Serial.printf("Não conectou. Criei a rede \"%s\" (senha %s).\n", AP_NOME, AP_SENHA);
    Serial.printf("Conecte nela e abra: http://%s\n", WiFi.softAPIP().toString().c_str());
  }
  if (MDNS.begin(NOME_MDNS)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("Também pode tentar: http://%s.local\n", NOME_MDNS);
  }
}

// ==========================================================================
// COMANDOS PELO MONITOR SERIAL
// ==========================================================================
void lerSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd == "t") {
    Serial.println(tararBalanca() ? "OK: balança zerada" : "Erro: balança sem leitura");
  } else if (cmd.startsWith("c")) {
    String erro = calibrarBalanca(cmd.substring(1).toFloat());
    Serial.println(erro.length() ? "Erro: " + erro : "OK: balança calibrada");
  } else if (cmd == "p") {
    if (balancaOk && !isnan(pesoG)) registrarPesagem(pesoG, false);
    else Serial.println("Erro: balança sem leitura");
  } else if (cmd == "l") {
    abrirComporta("manual");
  } else if (cmd.length()) {
    Serial.println("Comandos: t (zerar), c 100 (calibrar com 100 g), p (pesar), l (liberar comida)");
  }
}

void imprimirStatus() {
  Serial.printf("Temp: %.1f C | Umid: %.0f %% | Peso: %.1f g | Comida: %.1f cm (%s) | ",
                temperatura, umidade, pesoG, distanciaCm, pratoVazio() ? "pouca" : "ok");
  if (modoAP) Serial.printf("Página: http://%s\n", WiFi.softAPIP().toString().c_str());
  else Serial.printf("Página: http://%s\n", WiFi.localIP().toString().c_str());
}

// ==========================================================================
// SETUP E LOOP
// ==========================================================================
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(300);
  Serial.println("\n=== Ninho inteligente: iniciando ===");

  // Alimentação 3,3 V auxiliar para o DHT22 e o HX711 (ver seção 2)
  if (PINO_3V3_AUX >= 0) {
    pinMode(PINO_3V3_AUX, OUTPUT);
    gpio_set_drive_capability((gpio_num_t)PINO_3V3_AUX, GPIO_DRIVE_CAP_3);
    digitalWrite(PINO_3V3_AUX, HIGH);
    delay(300);  // tempo para os sensores ligarem
  }

  setenv("TZ", "<-03>3", 1);  // horário de Brasília (sem horário de verão)
  tzset();

  // Configurações guardadas
  prefs.begin("ninho", false);
  alimentacaoAuto = prefs.getBool("auto", true);
  fatorBalanca = prefs.getFloat("fator", FATOR_CALIBRACAO_PADRAO);
  if (fabsf(fatorBalanca) < 0.001f) fatorBalanca = FATOR_CALIBRACAO_PADRAO;
  if (prefs.isKey("offset")) offsetBalanca = prefs.getLong("offset", 0);
  else tararAoIniciar = true;   // primeira vez: a balança deve estar vazia
  size_t tamanho = prefs.getBytesLength("pesagens");
  if (tamanho > 0 && tamanho <= sizeof(pesagens) && tamanho % sizeof(Pesagem) == 0) {
    prefs.getBytes("pesagens", pesagens, tamanho);
    nPesagens = tamanho / sizeof(Pesagem);
  }
  Serial.printf("%d pesagens guardadas. Fator da balança: %.2f\n", nPesagens, fatorBalanca);

  // Sensor de distância
  pinMode(PINO_TRIG, OUTPUT);
  pinMode(PINO_ECHO, INPUT);
  digitalWrite(PINO_TRIG, LOW);

  // Servo (comporta fechada)
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);
  servo.attach(PINO_SERVO, 500, 2400);
  servo.write(ANGULO_FECHADO);

  // Temperatura e balança
  dht.begin();
  balanca.begin(PINO_HX711_DT, PINO_HX711_SCK);
  // Com o pull-up, um HX711 desconectado aparece como "sem resposta"
  // na página, em vez de mostrar pesos aleatórios.
  pinMode(PINO_HX711_DT, INPUT_PULLUP);

  conectarWiFi();
  configurarServidor();
  Serial.println("Comandos: t (zerar), c 100 (calibrar com 100 g), p (pesar), l (liberar comida)");
}

void loop() {
  server.handleClient();
  unsigned long agora = millis();

  atualizarBalanca();                      // sempre que o HX711 tiver leitura nova

  if (agora - tDist >= 200) {              // distância: 5 vezes por segundo
    tDist = agora;
    atualizarDistancia();
  }
  atualizarComedouro();

  if (agora - tDHT >= 2500) {              // DHT22: no máximo 1 leitura a cada 2 s
    tDHT = agora;
    lerDHT();
  }
  guardarHistoricoTemp();

  if (!horaJaValida && horaValida()) {     // o relógio acabou de ser acertado
    horaJaValida = true;
    corrigirPesagensSemHora();
  }

  if (agora - tStatus >= 5000) {
    tStatus = agora;
    imprimirStatus();
  }
  lerSerial();
}
