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
        Ele continua tentando o Wi-Fi a cada 15 s; quando conectar, a rede
        "Ninho-ESP32" é desligada e a página passa a usar o IP do roteador.
     5. O log do sistema fica em http://<ip>/logs (ver API.md).

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
#include <LittleFS.h>
#include <esp_system.h>
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
const unsigned long INTERVALO_TENTATIVA_WIFI_MS = 15000;  // sem Wi-Fi: tenta de novo a cada 15 s

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
const float HISTERESE_TEMP_C = 0.5;  // para "voltar à faixa" precisa entrar 0,5 °C além do limite

// --- Log dos sensores ---
const unsigned long TEMPO_CONFIRMA_SENSOR_MS = 5000;  // falha (ou volta) precisa durar 5 s para ir ao log

// ==========================================================================
// OBJETOS E VARIÁVEIS
// ==========================================================================
const int PORTA_HTTP = 80;
WebServer server(PORTA_HTTP);
Servo servo;
DHT dht(PINO_DHT, DHT22);
HX711 balanca;
Preferences prefs;
bool modoAP = false;

// --- Wi-Fi: estado da conexão, conferido a cada volta do loop ---
bool wifiConectado = false;       // estado na última verificação
bool jaConectouAntes = false;     // separa "conectado" (1ª vez) de "reconectado"
unsigned long tPerdeuWiFi = 0, tTentativaWiFi = 0;
uint32_t reconexoes = 0;

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

// --- Log do sistema (memória flash, LittleFS) ---
// Cada ocorrência vira um registro de tamanho fixo (80 bytes). Como todos têm
// o mesmo tamanho, o registro número i fica na posição i * 80 do arquivo:
// dá para ler de trás para frente só com seek(), sem interpretar texto.
enum NivelLog : uint8_t { NIVEL_INFO, NIVEL_AVISO, NIVEL_ERRO };
const char* NOMES_NIVEL[] = { "INFO", "AVISO", "ERRO" };
struct RegistroLog {
  uint32_t id;      // número sequencial do evento
  uint32_t boot;    // em qual inicialização do ESP32 o evento aconteceu
  uint32_t t;       // data/hora (segundos desde 1970); 0 = hora ainda não acertada
  uint32_t ms;      // millis() no momento do evento
  uint8_t nivel;    // NIVEL_INFO, NIVEL_AVISO ou NIVEL_ERRO
  char msg[63];     // texto, terminado em '\0'
};                  // 4 * 4 + 1 + 63 = 80 bytes
// Retenção: dois arquivos de até 75 registros. Quando o atual enche, o antigo
// é apagado e o atual passa a ser o antigo. Ficam guardados entre 75 e 150
// eventos (no máximo 12 KB) e nunca é preciso reescrever um arquivo inteiro.
const int LOG_POR_ARQUIVO = 75;
const char* ARQ_LOG        = "/log.bin";
const char* ARQ_LOG_ANTIGO = "/log_antigo.bin";
bool logOk = false;          // o LittleFS foi montado?
uint32_t bootAtual = 0;      // contador de inicializações (guardado na NVS)
uint32_t proximoIdLog = 1;
int nLogAtual = 0;           // registros no arquivo atual

// --- Estado dos sensores já registrado no log (para gravar só as mudanças) ---
bool dhtComErro = false, balancaComErro = false, distComErro = false;
unsigned long tDhtEstado = 0, tBalancaEstado = 0, tDistEstado = 0;
bool tempForaFaixa = false;

// Temporizadores do loop
unsigned long tDist = 0, tDHT = 0, tStatus = 0;
bool horaJaValida = false;
bool horaPeloNavegador = false;   // para o log saber quem acertou a hora

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
// LOG DO SISTEMA (gravado na memória flash com o LittleFS)
// ==========================================================================
// Toda gravação acontece na task do loop(): no setup(), no próprio loop() ou
// nos handlers da página, que também rodam dentro do loop (server.handleClient).
void registrarLog(NivelLog nivel, const String& msg) {
  RegistroLog r = {};
  r.id = proximoIdLog++;
  r.boot = bootAtual;
  r.t = horaValida() ? (uint32_t)time(nullptr) : 0;
  r.ms = millis();
  r.nivel = nivel;
  strlcpy(r.msg, msg.c_str(), sizeof(r.msg));
  if (msg.length() >= sizeof(r.msg)) {
    // O texto foi cortado. Tira os bytes não ASCII do fim para não deixar
    // uma letra acentuada pela metade (em UTF-8 ela ocupa 2 bytes).
    size_t n = strlen(r.msg);
    while (n > 0 && ((uint8_t)r.msg[n - 1] & 0x80)) n--;
    r.msg[n] = '\0';
  }
  Serial.printf("[%s] %s\n", NOMES_NIVEL[nivel], r.msg);
  if (!logOk) return;

  if (nLogAtual >= LOG_POR_ARQUIVO) {   // arquivo cheio: ele vira o antigo
    LittleFS.remove(ARQ_LOG_ANTIGO);
    LittleFS.rename(ARQ_LOG, ARQ_LOG_ANTIGO);
    nLogAtual = 0;
  }
  File f = LittleFS.open(ARQ_LOG, FILE_APPEND);
  if (!f) return;
  f.write((const uint8_t*)&r, sizeof(r));
  f.close();
  nLogAtual++;
}

// Lê o id do último registro de um arquivo (0 se ele estiver vazio).
uint32_t ultimoIdLog(const char* caminho) {
  if (!LittleFS.exists(caminho)) return 0;
  File f = LittleFS.open(caminho, FILE_READ);
  int n = f.size() / sizeof(RegistroLog);
  RegistroLog r = {};
  if (n > 0) {
    f.seek((n - 1) * sizeof(RegistroLog));
    f.read((uint8_t*)&r, sizeof(r));
  }
  f.close();
  return r.id;
}

void iniciarLog() {
  bootAtual = prefs.getUInt("boot", 0) + 1;   // conta as inicializações
  prefs.putUInt("boot", bootAtual);

  // true = formata a partição na primeira vez (ou se ela estiver corrompida).
  // O LittleFS usa a partição "spiffs" do esquema padrão da Arduino IDE.
  logOk = LittleFS.begin(true);
  if (!logOk) {
    Serial.println("Erro: não consegui montar o LittleFS. O log fica só no Serial.");
    return;
  }
  if (LittleFS.exists(ARQ_LOG)) {
    File f = LittleFS.open(ARQ_LOG, FILE_READ);
    nLogAtual = f.size() / sizeof(RegistroLog);
    f.close();
  }
  // Continua a numeração de onde parou (o arquivo atual pode estar vazio
  // logo depois de uma troca, então olha também o antigo).
  uint32_t ultimo = ultimoIdLog(ARQ_LOG);
  if (ultimo == 0) ultimo = ultimoIdLog(ARQ_LOG_ANTIGO);
  proximoIdLog = ultimo + 1;
}

// Primeiro evento de cada boot, com o motivo do último reset.
void registrarInicio() {
  const char* motivo = "desconhecido";
  bool grave = false;   // reset causado por falha: também vira ERRO
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   motivo = "energia ligada ou botão EN"; break;
    case ESP_RST_EXT:       motivo = "pino de reset externo"; break;
    case ESP_RST_SW:        motivo = "reinício por software"; break;
    case ESP_RST_DEEPSLEEP: motivo = "saída do deep sleep"; break;
    case ESP_RST_PANIC:     motivo = "travamento (panic)"; grave = true; break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       motivo = "watchdog"; grave = true; break;
    case ESP_RST_BROWNOUT:  motivo = "queda de tensão (brownout)"; grave = true; break;
    default: break;
  }
  registrarLog(NIVEL_INFO, String("ESP32 iniciado (") + motivo + ")");
  if (grave) registrarLog(NIVEL_ERRO, String("Reset por ") + motivo);
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
  registrarLog(NIVEL_INFO, String("Comida liberada (") + origem + ")");
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
  registrarLog(NIVEL_INFO, String("Pesagem ") + (automatica ? "automática" : "manual") +
                           " registrada: " + comVirgula(gramas, 1) + " g");
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
    registrarLog(NIVEL_INFO, "Balança tarada automaticamente (primeira vez)");
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
  registrarLog(NIVEL_INFO, "Balança tarada");
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
  registrarLog(NIVEL_INFO, "Balança calibrada (fator " + comVirgula(fatorBalanca, 2) + ")");
  return "";
}

// ==========================================================================
// EVENTOS DOS SENSORES (vão para o log só quando o estado muda)
// ==========================================================================
// Aceita a mudança (ok -> erro ou erro -> ok) só depois que ela dura 5 s
// seguidos, como o "prato vazio" que precisa durar 3 s. Assim um sensor que
// falha e volta várias vezes seguidas não enche o log.
// Retorna true quando o estado registrado mudou.
bool mudouEstado(bool erroAgora, bool& comErro, unsigned long& tEstado) {
  unsigned long agora = millis();
  if (erroAgora == comErro) {   // nada mudou: reinicia a contagem
    tEstado = agora;
    return false;
  }
  if (agora - tEstado < TEMPO_CONFIRMA_SENSOR_MS) return false;
  comErro = erroAgora;
  tEstado = agora;
  return true;
}

// Só lê o estado que as funções dos sensores já calcularam; não mexe nelas.
void verificarSensores() {
  if (mudouEstado(isnan(temperatura), dhtComErro, tDhtEstado))
    registrarLog(dhtComErro ? NIVEL_ERRO : NIVEL_INFO,
                 dhtComErro ? "DHT22 sem leitura" : "DHT22 voltou a responder");
  if (mudouEstado(!balancaOk, balancaComErro, tBalancaEstado))
    registrarLog(balancaComErro ? NIVEL_ERRO : NIVEL_INFO,
                 balancaComErro ? "HX711 sem leitura" : "HX711 voltou a responder");
  if (mudouEstado(isnan(distanciaCm), distComErro, tDistEstado))
    registrarLog(distComErro ? NIVEL_ERRO : NIVEL_INFO,
                 distComErro ? "HC-SR04 sem leitura" : "HC-SR04 voltou a responder");

  // Temperatura fora da faixa, com histerese: sai da faixa ao passar do
  // limite, mas só "volta" quando fica 0,5 °C para dentro dele. Sem isso,
  // uma leitura oscilando em cima do limite (35,0 / 35,1 / 35,0...)
  // geraria um evento a cada leitura.
  if (isnan(temperatura)) return;
  if (!tempForaFaixa && (temperatura < TEMP_MIN_C || temperatura > TEMP_MAX_C)) {
    tempForaFaixa = true;
    registrarLog(NIVEL_AVISO, "Temperatura fora da faixa: " + comVirgula(temperatura, 1) + " °C");
  } else if (tempForaFaixa && temperatura >= TEMP_MIN_C + HISTERESE_TEMP_C &&
             temperatura <= TEMP_MAX_C - HISTERESE_TEMP_C) {
    tempForaFaixa = false;
    registrarLog(NIVEL_INFO, "Temperatura voltou à faixa: " + comVirgula(temperatura, 1) + " °C");
  }
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
/* Cores do design. --suave2, --ok-txt e --botao existem por contraste: texto
   pequeno precisa de 4,5:1, e --suave sobre --fundo dá só 4,37:1; o branco
   sobre #c4622d dá 4,0:1, sobre #b0561f passa. */
:root{--fundo:#f6f1ea;--cartao:#fff;--texto:#2b2420;--suave:#7a6e66;--suave2:#6b5f57;--linha:#e9e0d6;--marca:#c4622d;--botao:#b0561f;
--ok:#3f8a5c;--ok-txt:#2f7049;--alerta:#c4412a;--alerta-txt:#9e321f;--alerta-bg:#fbe9e5;--frio:#3d6fa8;--frio-txt:#2c5584;--frio-bg:#e6eef7;
--marca-txt:#9a4a1f;--marca-bg:#f8ebe2;--neutro-bg:#efe9e3;--info-txt:#5a4f48}
@media (prefers-color-scheme:dark){:root{color-scheme:dark;--fundo:#1b1714;--cartao:#26201c;--texto:#f1e9e2;--suave:#a89c93;--suave2:#a89c93;--linha:#3a312b;--marca:#e0874f;
--ok:#7fc79a;--ok-txt:#7fc79a;--alerta:#f08a72;--alerta-txt:#f08a72;--alerta-bg:#3a2620;--frio:#8ab4e8;--frio-txt:#8ab4e8;--frio-bg:#1d2f45;
--marca-txt:#e0874f;--marca-bg:#3a2a20;--neutro-bg:#322a25;--info-txt:#c9bdb4}}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--fundo);color:var(--texto);font-variant-numeric:tabular-nums}
[hidden]{display:none!important}
header{position:sticky;top:0;z-index:5;background:var(--fundo);border-bottom:1px solid var(--linha)}
.topo{max-width:980px;margin:0 auto;padding:12px 16px;display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:12px}
h1{margin:0;font-size:18px}
.sub{font-size:12px;color:var(--suave2)}
.vivo{display:flex;align-items:center;gap:8px;padding:7px 12px;border-radius:99px;background:var(--cartao);border:1px solid var(--linha);font-size:12px;font-weight:650}
.ponto{width:8px;height:8px;border-radius:50%;background:var(--suave);flex:none}
.on .ponto{background:var(--ok)}
.pisca{animation:pisca .7s ease-out}
@keyframes pisca{from{box-shadow:0 0 0 0 var(--ok)}to{box-shadow:0 0 0 7px transparent}}
.abas{order:3;flex:1 1 100%;display:grid;grid-template-columns:1fr 1fr;gap:4px;padding:4px;background:var(--linha);border-radius:12px}
.abas button{min-height:44px;border:0;border-radius:9px;background:transparent;color:var(--texto);font:inherit;font-size:14px;font-weight:600;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:8px}
.abas button[aria-selected=true]{background:var(--cartao);font-weight:700}
.cont{min-width:20px;height:20px;padding:0 6px;border-radius:99px;background:var(--botao);color:#fff;font-size:11px;font-weight:700;display:inline-flex;align-items:center;justify-content:center}
main,#log{max-width:980px;margin:0 auto;padding:12px 16px 28px;display:grid;gap:12px}
main>*{min-width:0}
/* No celular a ordem dos cartões segue o design; no desktop vale a ordem do HTML */
@media (max-width:759px){.o0{order:0}.o1{order:1}.o2{order:2}.o3{order:3}.o4{order:4}.o5{order:5}.o6{order:6}.o7{order:7}}
.so-desk{display:none}
@media (min-width:760px){
 .abas{order:0;flex:0 0 auto;grid-template-columns:auto auto}.abas button{padding:0 22px}
 main{grid-template-columns:repeat(12,minmax(0,1fr));gap:16px}
 .c7{grid-column:span 7}.c5{grid-column:span 5}.c12{grid-column:1/-1}.so-desk{display:flex}.cartao{padding:20px}}
.cartao{background:var(--cartao);border:1px solid var(--linha);border-radius:14px;padding:16px;display:flex;flex-direction:column;gap:12px}
.cab{display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap}
.rot{margin:0;font-size:11px;font-weight:650;letter-spacing:.08em;text-transform:uppercase;color:var(--suave)}
.tres{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}
.tres>div,.pilha{display:flex;flex-direction:column;gap:3px}
.big{font-size:26px;font-weight:700;line-height:1.1}.big small{font-size:15px;font-weight:600}
.med{font-size:16px;font-weight:650}
.nota{font-size:12px;color:var(--suave)}
.q{color:var(--alerta-txt)}.f{color:var(--frio-txt)}.b{color:var(--ok-txt)}
.estado{display:flex;align-items:center;gap:8px;font-size:13px;font-weight:600}
.estado .ponto{background:currentColor}
.chip{padding:4px 10px;border-radius:99px;font-size:12px;font-weight:650;background:var(--neutro-bg)}
.chip.frio{background:var(--frio-bg);color:var(--frio-txt)}.chip.marca{background:var(--marca-bg);color:var(--marca-txt)}
.mono{font-family:ui-monospace,"SF Mono",Menlo,Consolas,monospace}
.ender{font-size:22px;font-weight:700;overflow-wrap:anywhere}
dl{margin:0;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px 12px}
dd{margin:2px 0 0;font-size:14px;font-weight:600;overflow-wrap:anywhere}
.sep{border-top:1px solid var(--linha);padding-top:12px;display:flex;flex-direction:column;gap:8px}
button{font:inherit}
.btn{min-height:44px;padding:0 14px;border-radius:10px;border:1px solid var(--linha);background:var(--cartao);color:var(--texto);font-size:14px;font-weight:600;cursor:pointer}
.btn.prim{min-height:48px;border:0;background:var(--botao);color:#fff;font-size:15px;font-weight:700}
.btn:disabled{background:var(--linha);color:var(--suave2);cursor:default}
.botoes{display:flex;flex-wrap:wrap;gap:8px}
details{font-size:14px}summary{cursor:pointer;color:var(--suave);min-height:32px}
input[type=number]{font:inherit;width:100px;min-height:44px;padding:0 8px;border-radius:10px;border:1px solid var(--linha);background:var(--fundo);color:var(--texto)}
.chave{display:flex;align-items:center;gap:10px;font-size:14px;min-height:44px}
.chave input{width:20px;height:20px;accent-color:var(--botao)}
#avisos:empty{display:none}
.av{display:flex;gap:10px;align-items:flex-start;padding:12px 14px;border-radius:12px;font-size:12px;margin-bottom:12px}
.av b{display:block;font-size:14px;margin-bottom:2px}.av svg{flex:none}
.av.q{background:var(--alerta-bg)}.av.f{background:var(--frio-bg)}.av.m{background:var(--marca-bg);color:var(--marca-txt)}
.av.perdido{background:#c4412a;color:#fff;flex-wrap:wrap}.av.perdido .btn{border:1.5px solid #fff;background:transparent;color:#fff}
.parado main .cartao{opacity:.6}
.g{display:block;overflow:visible}
.g-grade{stroke:var(--linha);stroke-width:1}.g-eixo{fill:var(--suave);font-size:10px}
.g-faixa{fill:var(--ok);opacity:.13}.g-faixa-l{stroke:var(--ok);stroke-dasharray:3 3;opacity:.6}
.g-linha{fill:none;stroke-width:2;stroke-linejoin:round;stroke-linecap:round}
.temp{stroke:var(--marca);fill:var(--marca)}.umid{stroke:var(--frio);fill:var(--frio)}
.g-x{stroke:var(--alerta);stroke-width:2;stroke-linecap:round}
.vistas{display:flex;align-items:center;gap:2px;padding:3px;background:var(--fundo);border-radius:10px}
.vistas .nota{padding:0 8px}
.vistas button{min-height:44px;padding:0 14px;border:0;border-radius:8px;background:transparent;color:var(--suave2);font-size:13px;font-weight:600;cursor:pointer}
.vistas button[aria-pressed=true]{background:var(--cartao);color:var(--texto);font-weight:700;box-shadow:0 1px 2px rgba(43,36,32,.12)}
.vistas button:disabled{opacity:.45;cursor:default}
#ninhoSvg svg{max-width:440px;margin:0 auto}
#gPeso{position:relative;touch-action:pan-y}
.sobre{position:absolute;left:12%;right:12%;top:22%;display:flex;flex-direction:column;align-items:center;gap:4px;padding:14px 12px;border-radius:12px;background:var(--cartao);border:1px solid var(--linha);text-align:center;font-size:12px;color:var(--suave)}
.sobre b{font-size:14px;color:var(--texto)}
.cartao .av{margin:0}
.av.neutro{background:var(--neutro-bg);font-size:13px;align-items:center}
.bola{width:10px;height:10px;border-radius:50%;flex:none;margin-top:4px;background:var(--alerta)}
.bola.anel{background:none;border:2px solid var(--texto);margin-top:0}
.leg{display:flex;flex-wrap:wrap;gap:6px 16px;font-size:12px}.leg span{display:flex;align-items:center;gap:6px}
.form-ecl{display:flex;flex-wrap:wrap;align-items:flex-end;gap:8px;padding:12px;border-radius:12px;background:var(--fundo);margin-top:8px}
.form-ecl label{display:flex;flex-direction:column;gap:4px;flex:1 1 150px;font-size:12px;font-weight:600;color:var(--suave)}
input[type=date]{min-height:44px;padding:0 10px;border:1px solid var(--linha);border-radius:10px;background:var(--cartao);color:var(--texto);font:inherit;font-size:14px}
.legenda{display:flex;align-items:center;gap:8px;margin:0}
#corLeg{width:12px;height:12px;border-radius:3px;flex:none}
.lado{display:flex;gap:14px;align-items:flex-start}
.fig{width:150px;flex:none}
@media (min-width:760px){.fig{width:170px}}
/* Textos dentro das ilustrações seguem o tema claro/escuro */
.il-t{fill:var(--suave);font-size:11px}.il-c{fill:var(--cartao);stroke:var(--suave);stroke-width:1.5}
.vazio{color:var(--suave);font-size:13px;text-align:center;padding:30px 0;margin:0}
.ev{display:flex;flex-direction:column;gap:4px;padding:10px 0;border-top:1px solid var(--linha);font-size:14px}
.nv{padding:2px 7px;border-radius:5px;font-size:10px;font-weight:750;letter-spacing:.06em}
.nv.ERRO{background:var(--alerta);color:var(--cartao)}.nv.AVISO{padding:1px 6px;border:1.5px solid var(--marca);color:var(--marca-txt)}.nv.INFO{background:var(--neutro-bg);color:var(--info-txt)}
.linha-ev{display:flex;align-items:center;gap:8px}
.lk{background:none;border:0;padding:0;color:var(--marca-txt);font-weight:650;font-size:13px;cursor:pointer;text-decoration:underline}
.rodape{margin:4px 0 0;text-align:center;font-size:12px;color:var(--suave2)}
#log{grid-template-columns:minmax(0,1fr)}
.cab-log{display:flex;align-items:baseline;justify-content:space-between;gap:8px;flex-wrap:wrap}.cab-log h2{margin:0;font-size:20px}
.filtros{display:flex;flex-wrap:wrap;gap:8px}
.filtros button{min-height:44px;padding:0 14px;border-radius:99px;border:1px solid var(--linha);background:var(--cartao);color:var(--texto);font-size:13px;font-weight:600;cursor:pointer}
.filtros button[aria-pressed=true]{background:var(--texto);color:var(--fundo);border-color:var(--texto);font-weight:700}
.boot{background:var(--cartao);border:1px solid var(--linha);border-radius:14px;overflow:hidden}
.boot>div{padding:12px 14px;display:flex;justify-content:space-between;gap:8px;background:var(--neutro-bg)}
.boot ul{list-style:none;margin:0;padding:0}
.boot li{padding:12px 14px;display:flex;flex-direction:column;gap:6px;border-top:1px solid var(--linha)}
.boot p{margin:0;font-size:14px;line-height:1.4;overflow-wrap:anywhere}
/* Evento novo: entra deslizando (250 ms) e fica destacado por ~3 s */
.novo{animation:entra .25s ease-out,destaque 3s ease-out}
@keyframes entra{from{opacity:0;transform:translateY(-6px)}}
@keyframes destaque{from{background:var(--marca-bg)}}
@media (prefers-reduced-motion:reduce){*{animation:none!important}}
#toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);max-width:90%;background:var(--texto);color:var(--fundo);padding:10px 16px;border-radius:10px;font-size:14px;opacity:0;transition:opacity .3s;pointer-events:none}
#toast.ver{opacity:1}
</style>
</head>
<body>
<header>
<div class="topo">
  <div><h1>Ninho do filhote</h1><div class="sub" id="ender">ESP32 · conectando…</div></div>
  <div class="abas" role="tablist" aria-label="Seções">
    <button role="tab" id="abaPainel" aria-selected="true" aria-controls="painel" onclick="mostrar('painel')">Painel</button>
    <button role="tab" id="abaLog" aria-selected="false" aria-controls="log" onclick="mostrar('log')">Log <span class="cont" id="naoVistos" hidden></span></button>
  </div>
  <div class="vivo" id="vivo"><span class="ponto" id="ponto"></span><span id="vivoTxt">Conectando…</span></div>
</div>
</header>

<main id="painel" role="tabpanel" aria-labelledby="abaPainel">
  <div id="avisos" class="c12 o0"></div>

  <section class="cartao c7 o1">
    <div class="cab">
      <h2 class="rot">Ninho · filhote</h2>
      <div class="vistas" role="group" aria-label="Vista do ninho">
        <span class="nota" id="semTermica" hidden>vista térmica indisponível</span>
        <button aria-pressed="true" id="vNormal" onclick="vista(false)">Normal</button>
        <button aria-pressed="false" id="vTermica" onclick="vista(true)">Térmica</button>
      </div>
    </div>
    <div id="ninhoSvg"></div>
    <p class="nota legenda" id="legenda" hidden><span id="corLeg"></span>Uma cor só: temperatura no ponto do sensor (DHT22). Não é mapa de calor.</p>
    <div class="tres" style="border-top:1px solid var(--linha);padding-top:12px">
      <div><span class="rot">Temperatura</span><span class="big" id="temp">—</span><span class="nota" id="faixa"></span></div>
      <div><span class="rot">Umidade</span><span class="big" id="umid">—</span><span class="nota">relativa · DHT22</span></div>
      <div><span class="rot">Peso</span><span class="big" id="peso">—</span><span class="nota" id="pesoNota"></span></div>
    </div>
    <div class="estado" id="estTemp"></div>
    <details>
      <summary>Balança: pesar, zerar e calibrar</summary>
      <p class="nota">O peso é registrado sozinho quando fica estável. Para calibrar: zere com a balança vazia, coloque um objeto de peso conhecido, informe o peso e toque em Calibrar.</p>
      <div class="botoes">
        <button class="btn" onclick="acao('/pesar')">Registrar pesagem</button>
        <button class="btn" onclick="acao('/tarar')">Zerar balança</button>
      </div>
      <p class="botoes"><label class="chave"><input type="number" id="massa" value="100" min="1" step="0.1"> g</label><button class="btn" onclick="calibrar()">Calibrar</button></p>
    </details>
  </section>

  <section class="cartao c5 o3">
    <div class="cab"><h2 class="rot">Conexão</h2><span class="chip" id="iface">—</span></div>
    <div class="cab" style="align-items:flex-end">
      <div class="pilha"><span class="rot">Endereço</span><span class="ender mono" id="ip">—</span></div>
      <div class="pilha" style="align-items:flex-end"><span id="barras"></span><span class="nota" id="rssi"></span></div>
    </div>
    <dl>
      <div><dt class="rot">Rede (SSID)</dt><dd id="ssid">—</dd></div>
      <div><dt class="rot">MAC</dt><dd class="mono" id="mac">—</dd></div>
      <div><dt class="rot">Ligado há</dt><dd id="uptime">—</dd></div>
      <div><dt class="rot">Reconexões</dt><dd id="recon">—</dd></div>
      <div><dt class="rot">Relógio</dt><dd id="relogio">—</dd></div>
      <div><dt class="rot">Status</dt><dd id="status">—</dd></div>
    </dl>
    <div class="sep">
      <div class="cab"><span class="rot">Rede medida pelo navegador</span><span class="estado" id="vivo2"></span></div>
      <div id="gLat"></div>
      <div class="tres">
        <div><span class="rot">Latência</span><span class="med" id="lat">—</span></div>
        <div><span class="rot">Média</span><span class="med" id="media">—</span></div>
        <div><span class="rot">Requisições</span><span class="med" id="nReq">0</span></div>
      </div>
      <span class="nota" id="redeNota"></span>
    </div>
  </section>

  <section class="cartao c5 o2">
    <div class="cab"><h2 class="rot">Comedouro dos pais</h2><span class="chip marca" id="modo">—</span></div>
    <div class="lado">
      <div class="fig" id="comSvg"></div>
      <div class="pilha" style="gap:12px">
        <div class="pilha"><span class="rot">Comida no prato</span><span class="big" id="nivel">—</span><span class="nota" id="dist"></span></div>
        <div class="pilha"><span class="rot">Comporta</span><span class="med" id="comporta">—</span></div>
        <div class="pilha"><span class="rot">Próxima liberação automática</span><span class="med" id="proxima"></span></div>
      </div>
    </div>
    <span class="nota" id="ultima"></span>
    <label class="chave"><input type="checkbox" id="auto" onchange="acao('/auto?on='+(this.checked?1:0))"> Liberar comida automaticamente</label>
    <button class="btn prim" id="btnLiberar" onclick="acao('/liberar')">Liberar comida</button>
  </section>

  <section class="cartao c7 o4">
    <h2 class="rot" id="tTemp">Temperatura · desde que a página abriu</h2>
    <div id="gTemp"></div>
    <span class="nota" id="notaFaixa"></span>
    <h2 class="rot" id="tUmid">Umidade · desde que a página abriu</h2>
    <div id="gUmid"></div>
  </section>

  <section class="cartao c7 o5">
    <div class="cab"><h2 class="rot">Crescimento do filhote</h2><span class="med" id="resumoPeso"></span></div>
    <div id="seloPeso" class="pilha" style="gap:8px"></div>
    <div id="gPeso"></div>
    <span class="nota" id="eixoPeso"></span>
    <div class="leg" id="legPeso"></div>
    <details id="ecl">
      <summary id="eclTit">Informe a data de eclosão</summary>
      <form class="form-ecl" onsubmit="salvarEclosao(event)">
        <p class="nota" style="flex:1 1 100%;margin:0">Com a data de eclosão dá para ver a idade do filhote e comparar o peso com a faixa de referência.</p>
        <label>Data de eclosão<input type="date" id="dataEcl" required></label>
        <button class="btn prim">Salvar</button>
        <span class="nota" id="eclMsg" role="status" style="flex:1 1 100%"></span>
      </form>
    </details>
    <span class="nota" id="fontePeso"></span>
    <span class="nota" id="notaPeso"></span>
    <div class="botoes"><a class="lk" href="/pesagens.csv">Baixar planilha</a><button class="lk" onclick="limpar()">Apagar pesagens</button></div>
  </section>

  <section class="cartao c5 o6 so-desk">
    <div class="cab"><h2 class="rot">Últimos eventos</h2><button class="lk" onclick="mostrar('log')">Ver log completo</button></div>
    <div id="ultimos"></div>
  </section>

  <p class="rodape c12 o7" id="rodape"></p>
</main>

<section id="log" role="tabpanel" aria-labelledby="abaLog" hidden>
  <div class="cab-log"><h2>Log do sistema</h2><span class="sub">GET /logs a cada 10 s</span></div>
  <div class="filtros" role="group" aria-label="Filtrar por nível">
    <button aria-pressed="true" data-n="" onclick="filtrar(this)">Todos</button>
    <button aria-pressed="false" data-n="ERRO" onclick="filtrar(this)">Erro</button>
    <button aria-pressed="false" data-n="AVISO" onclick="filtrar(this)">Aviso</button>
    <button aria-pressed="false" data-n="INFO" onclick="filtrar(this)">Info</button>
  </div>
  <div id="listaLog" style="display:grid;gap:14px"><p class="vazio">Carregando o log…</p></div>
  <p class="rodape" id="rodapeLog"></p>
</section>
<div id="toast" role="status" aria-live="polite"></div>

<script>
const $ = id => document.getElementById(id);
// Texto vindo do ESP32 (mensagens do log, SSID) é escapado antes de ir
// para o HTML: um "<" na mensagem não pode virar uma tag.
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'})[c]);
const fmt = (v, c) => v == null || isNaN(v) ? '—' : Number(v).toFixed(c).replace('.', ',');
const dois = n => String(n).padStart(2, '0');
const hms = d => d.toLocaleTimeString('pt-BR');
const ddmm = d => dois(d.getDate()) + '/' + dois(d.getMonth() + 1);
function duracao(s) {   // segundos -> "2,1 s", "42 s", "12 min", "3 h 05 min"
  if (s < 10) return fmt(s, 1) + ' s';
  if (s < 60) return Math.round(s) + ' s';
  if (s < 3600) return Math.floor(s / 60) + ' min';
  return Math.floor(s / 3600) + ' h ' + dois(Math.floor(s % 3600 / 60)) + ' min';
}

// ---------------------------------------------------------------- estado
let dados = null;                 // última resposta de /dados
let logs = [], bootAtual = 0;     // /logs (mais recente primeiro)
let conectado = false, tUltimaResposta = 0;
let versaoPesagens = -1, pesagens = [];
// Histórico de temperatura e umidade: 1 ponto por minuto (a mesma taxa do
// ESP32), janela de 6 h. Começa com o /historico do ESP32 e recebe um ponto
// novo do /dados a cada minuto.
const hist = [], JANELA = 6 * 3600 * 1000;
let tUltimoPonto = 0, histDoEsp = false;
const rede = { total: 0, falhas: 0, lat: [] };   // lat: últimas 60 (ms, ou null = sem resposta)
let filtro = '', novos = new Set();
let vistoAte = 0;                 // id do último ERRO/AVISO que a pessoa já viu
try { vistoAte = +localStorage.getItem('ninhoVisto') || 0; } catch (e) {}

// ---------------------------------------------------------------- rede
// Fila: cada requisição só sai depois que a anterior terminou. O WebServer
// do ESP32 atende um cliente por vez; requisições em paralelo ficariam
// esperando lá dentro e estourariam o tempo.
let fila = Promise.resolve();
function pedir(url, metodo) {
  const p = fila.then(() => buscar(url, metodo));
  fila = p.catch(() => {});
  return p;
}

// Uma requisição com limite de 3 s (AbortController cancela o fetch) e
// medição da latência com performance.now().
async function buscar(url, metodo) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 3000);
  const t0 = performance.now();
  rede.total++;
  try {
    const r = await fetch(url, { method: metodo || 'GET', cache: 'no-store', signal: ctrl.signal });
    // Só lê JSON quando a resposta é JSON: um 404 do ESP32 vem em texto
    const j = (r.headers.get('content-type') || '').includes('json') ? await r.json() : null;
    guardarLatencia(performance.now() - t0);
    return { ok: r.ok, status: r.status, j };
  } catch (e) {
    rede.falhas++;
    guardarLatencia(null);
    throw e;
  } finally {
    clearTimeout(timer);
  }
}
function guardarLatencia(ms) {
  rede.lat.push(ms == null ? null : Math.round(ms));
  if (rede.lat.length > 60) rede.lat.shift();
}

// Laço de atualização SEM setInterval: a próxima volta só é agendada
// (2 s depois) quando esta terminou, com resposta ou com timeout.
let tLogs = 0, proxima = null, rodando = false;
async function ciclo() {
  if (rodando) return;            // "Tentar agora" durante uma volta: ignora
  rodando = true;
  clearTimeout(proxima);
  try {
    const r = await pedir('/dados');
    if (!r.ok) throw new Error(r.status);
    receberDados(r.j);
    if (r.j.pesagens_ver !== versaoPesagens) {
      const p = await pedir('/pesagens');
      if (p.ok) { pesagens = p.j.pontos; versaoPesagens = r.j.pesagens_ver; desenharPeso(); }
    }
    if (Date.now() - tLogs >= 10000) {          // /logs a cada 10 s, na mesma fila
      tLogs = Date.now();
      const l = await pedir('/logs');
      if (l.ok) receberLogs(l.j);
    }
  } catch (e) {
    semResposta();                // timeout, rede fora ou erro HTTP
  }
  rodando = false;
  proxima = setTimeout(ciclo, 2000);
}

async function acao(url) {
  try {
    const r = await pedir(url, 'POST');
    toast(r.j ? r.j.msg : 'O ESP32 respondeu com erro ' + r.status);   // inclusive o erro, ex.: "A comporta já está aberta"
  } catch (e) {
    toast('Não consegui falar com o ESP32');
  }
  ciclo();
}
function calibrar() {
  const g = parseFloat(String($('massa').value).replace(',', '.'));
  if (!(g > 0)) { toast('Informe o peso do objeto em gramas'); return; }
  acao('/calibrar?g=' + g);
}
function limpar() { if (confirm('Apagar todas as pesagens registradas?')) acao('/limpar'); }
function toast(t) {
  const a = $('toast');
  a.textContent = t; a.classList.add('ver');
  clearTimeout(a.t); a.t = setTimeout(() => a.classList.remove('ver'), 3000);
}

// ---------------------------------------------------------------- respostas
function receberDados(d) {
  dados = d; conectado = true; tUltimaResposta = Date.now();
  if (Date.now() - tUltimoPonto >= 60000) guardarPonto(Date.now(), d.temp, d.umid);
  const p = $('ponto');            // "ao vivo": o ponto pisca a cada resposta
  p.classList.remove('pisca'); void p.offsetWidth; p.classList.add('pisca');
  desenharPainel();
}
function guardarPonto(t, temp, umid) {
  hist.push({ t, temp, umid });
  tUltimoPonto = t;
  while (hist.length && hist[0].t < Date.now() - JANELA) hist.shift();   // mantém só as últimas 6 h
}
// /historico manda o millis() de cada ponto e o millis() de agora ("agora");
// a diferença entre os dois é há quanto tempo o ponto foi medido.
async function carregarHistorico() {
  try {
    const r = await pedir('/historico');
    if (!r.ok) return;
    const agora = Date.now();
    for (const p of r.j.pontos) guardarPonto(agora - (r.j.agora - p[0]), p[1], p[2]);
    histDoEsp = true;
  } catch (e) {}                  // falhou: segue só com o que chegar pelo /dados
}
function semResposta() {
  conectado = false;
  desenharPainel();
}
function receberLogs(j) {
  const antes = new Set(logs.map(e => e.id));
  // Eventos novos (só depois da primeira carga) entram com animação
  novos = logs.length ? new Set(j.logs.filter(e => !antes.has(e.id)).map(e => e.id)) : new Set();
  const mudou = j.logs.length !== logs.length || (j.logs[0] && j.logs[0].id) !== (logs[0] && logs[0].id);
  logs = j.logs; bootAtual = j.boot_atual;
  if (!$('log').hidden) marcarVistos();
  contarNaoVistos();
  if (mudou) { desenharLog(); desenharUltimos(); }
  novos = new Set();
}

// ---------------------------------------------------------------- painel
function desenharPainel() {
  document.body.classList.toggle('parado', !conectado);
  $('vivo').classList.toggle('on', conectado);
  $('vivoTxt').textContent = conectado ? 'Ao vivo · ' + ultimaLatencia() + ' ms' : 'Parado';
  desenharAvisos();
  desenharRede();
  const d = dados;
  if (!d) { $('rodape').textContent = 'Ainda sem resposta do ESP32.'; return; }
  $('ender').textContent = 'ESP32 · ' + d.ip;

  // Ninho
  const quente = d.temp != null && d.temp > d.temp_max, frio = d.temp != null && d.temp < d.temp_min;
  $('temp').innerHTML = d.temp == null ? '—' : fmt(d.temp, 1) + ' <small>°C</small>';
  $('temp').className = 'big' + (quente ? ' q' : frio ? ' f' : '');
  $('faixa').textContent = 'ideal ' + fmt(d.temp_min, 0) + '–' + fmt(d.temp_max, 0) + ' °C';
  $('umid').innerHTML = d.umid == null ? '—' : fmt(d.umid, 0) + ' <small>%</small>';
  const pesando = d.balanca_ok && d.peso != null && d.peso >= 5;
  $('peso').innerHTML = pesando ? fmt(d.peso, 1) + ' <small>g</small>' : '—';
  $('pesoNota').textContent = !d.balanca_ok ? 'balança sem leitura' : pesando ? 'na balança' : 'balança vazia';
  $('pesoNota').className = 'nota' + (d.balanca_ok ? '' : ' q');
  const est = d.temp == null ? ['q', 'DHT22 sem leitura'] : quente ? ['q', 'Acima da faixa ideal'] :
              frio ? ['f', 'Abaixo da faixa ideal'] : ['b', 'Dentro da faixa ideal'];
  $('estTemp').className = 'estado ' + est[0];
  $('estTemp').innerHTML = '<span class="ponto"></span>' + est[1];

  // Comedouro: nível = onde a distância medida fica entre "prato cheio" e "prato vazio"
  let nivel = null;
  if (d.dist != null) nivel = Math.min(1, Math.max(0, (d.dist_vazio - d.dist) / (d.dist_vazio - d.dist_cheio)));
  $('modo').textContent = d.auto ? 'Automático' : 'Manual';
  $('nivel').textContent = nivel == null ? '—' : Math.round(nivel * 100) + ' %';
  $('nivel').className = 'big' + (d.vazio ? ' q' : '');
  $('dist').textContent = d.dist == null ? 'sensor sem leitura' : 'sensor a ' + fmt(d.dist, 1) + ' cm da comida';
  $('comporta').textContent = d.aberta ? 'Aberta' : 'Fechada';
  // espera_s = quanto falta para a próxima liberação AUTOMÁTICA ser permitida.
  // Não bloqueia a liberação manual.
  $('proxima').textContent = !d.auto ? 'desligada' :
    d.espera_s > 0 ? 'em ' + duracao(d.espera_s) : 'quando o prato esvaziar';
  $('ultima').textContent = d.liberacoes > 0 ?
    'Última liberação há ' + duracao(d.ultima_lib_ha_s) + ' (' + d.origem + ') · ' + d.liberacoes + ' desde que o ESP32 ligou' :
    'Nenhuma liberação desde que o ESP32 ligou';
  if (document.activeElement !== $('auto')) $('auto').checked = d.auto;
  desenharNinho();
  desenharComedouro(nivel);
  if ((d.eclosao || 0) !== eclAntes) desenharPeso();   // a data de eclosão mudou

  // Conexão
  const ap = d.interface === 'AP';
  $('iface').textContent = ap ? 'AP · rede própria' : 'STA · no roteador';
  $('iface').className = 'chip ' + (ap ? 'marca' : 'frio');
  $('ip').innerHTML = esc(d.ip) + '<span class="nota" style="font-size:inherit">:' + d.porta + '</span>';
  $('ssid').textContent = d.ssid;
  $('mac').textContent = d.mac;
  $('uptime').textContent = duracao(d.uptime_s);
  $('recon').textContent = d.reconexoes;
  $('relogio').textContent = d.hora_ok ? 'acertado' : 'sem hora';
  $('relogio').className = d.hora_ok ? '' : 'q';
  $('status').textContent = ap ? 'rede própria ativa' : d.conectado ? 'conectado' : 'desconectado';
  // Sinal: em modo AP o ESP32 é o ponto de acesso, então não existe RSSI
  const barras = ap || !d.rssi ? 0 : d.rssi >= -55 ? 4 : d.rssi >= -65 ? 3 : d.rssi >= -75 ? 2 : 1;
  let s = '<svg width="26" height="20" viewBox="0 0 26 20" role="img" aria-label="' + (ap ? 'Sinal não se aplica' : 'Sinal: ' + barras + ' de 4 barras') + '">';
  for (let i = 0; i < 4; i++) s += '<rect x="' + i * 7 + '" y="' + (14 - i * 4.4) + '" width="4" height="' + (5 + i * 4.4) + '" rx="1" style="fill:var(' + (i < barras ? '--texto' : '--linha') + ')"/>';
  $('barras').innerHTML = s + '</svg>';
  $('rssi').textContent = ap ? 'RSSI não se aplica' : d.rssi + ' dBm';

  desenharGraficos();
  $('rodape').textContent = (conectado ? 'Atualizado às ' : 'Última leitura às ') + hms(new Date(tUltimaResposta)) + (bootAtual ? ' · boot ' + bootAtual : '');
}

// Faixas de aviso no topo do painel. Só reescreve quando o texto muda,
// para o leitor de tela não repetir o aviso a cada 2 s.
const ICONES = {
  term: 'M14 14.8V5a2 2 0 1 0-4 0v9.8a4 4 0 1 0 4 0z',
  tri: 'M12 3 2 20h20L12 3zM12 10v4M12 17v.5',
  wifi: 'M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0M12 19.5v.5',
  semWifi: 'M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0M12 19.5v.5M4 4l16 16'
};
let avisosAntes = '';
function faixa(classe, icone, titulo, texto, extra) {
  return '<div class="av ' + classe + '" role="' + (classe === 'm' || classe === 'f' ? 'status' : 'alert') + '">' +
    '<svg width="20" height="20" viewBox="0 0 24 24" aria-hidden="true"><path d="' + ICONES[icone] + '" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/></svg>' +
    '<div><b>' + titulo + '</b>' + texto + '</div>' + (extra || '') + '</div>';
}
function desenharAvisos() {
  const d = dados;
  let h = '';
  if (!conectado) {
    const ha = tUltimaResposta ? 'há ' + duracao((Date.now() - tUltimaResposta) / 1000) : '';
    h += faixa('perdido', 'semWifi', 'Sem resposta do ESP32 ' + ha,
      'Pode ser o ESP32 desligado, o Wi-Fi caído ou este aparelho fora da rede. Tentando de novo a cada 2 s.' +
      (tUltimaResposta ? ' Última leitura às ' + hms(new Date(tUltimaResposta)) + '.' : ''),
      '<button class="btn" onclick="ciclo()">Tentar agora</button>');
  }
  if (d) {
    const fx = fmt(d.temp_min, 0) + '–' + fmt(d.temp_max, 0) + ' °C';
    if (d.temp == null) h += faixa('m', 'tri', 'DHT22 sem leitura', 'O ESP32 respondeu, mas sem temperatura e umidade. Confira o cabo do sensor.');
    else if (d.temp > d.temp_max) h += faixa('q', 'term', 'Temperatura alta: ' + fmt(d.temp, 1) + ' °C', 'Acima da faixa ideal (' + fx + ') no ponto do sensor.');
    else if (d.temp < d.temp_min) h += faixa('f', 'term', 'Temperatura baixa: ' + fmt(d.temp, 1) + ' °C', 'Abaixo da faixa ideal (' + fx + ') no ponto do sensor.');
    if (d.vazio) h += faixa('q', 'tri', 'Pouca comida no prato', 'O sensor está a ' + fmt(d.dist, 1) + ' cm da comida (limite: ' + fmt(d.dist_vazio, 0) + ' cm).');
    if (d.dist == null) h += faixa('m', 'tri', 'Sensor de distância sem leitura', 'Confira os fios do HC-SR04.');
    if (!d.balanca_ok) h += faixa('m', 'tri', 'Balança sem resposta', 'Confira os fios do HX711.');
    if (d.modo_ap) h += faixa('f', 'wifi', 'Modo AP: rede própria do ESP32', 'O ESP32 não conectou ao roteador e continua tentando a cada 15 s.' + (d.hora_ok ? '' : ' O relógio está sem hora.'));
  }
  if (h !== avisosAntes) { $('avisos').innerHTML = h; avisosAntes = h; }
  const btn = $('btnLiberar');      // só a falta de conexão bloqueia a liberação manual
  btn.disabled = !conectado;
  btn.textContent = conectado ? 'Liberar comida' : 'Liberar comida · sem conexão';
}

function ultimaLatencia() {
  for (let i = rede.lat.length - 1; i >= 0; i--) if (rede.lat[i] != null) return rede.lat[i];
  return '—';
}
function desenharRede() {
  const ok = rede.lat.filter(v => v != null);
  $('lat').textContent = ok.length ? ultimaLatencia() + ' ms' : '—';
  $('media').textContent = ok.length ? Math.round(ok.reduce((a, b) => a + b, 0) / ok.length) + ' ms' : '—';
  $('nReq').textContent = rede.total.toLocaleString('pt-BR');
  $('redeNota').textContent = 'GET /dados a cada 2 s · /logs a cada 10 s · ' + rede.falhas + ' sem resposta';
  $('redeNota').className = 'nota' + (rede.falhas ? ' q' : '');
  $('vivo2').className = 'estado ' + (conectado ? 'b' : 'q');
  $('vivo2').innerHTML = '<span class="ponto"></span>' + (conectado ? 'ao vivo' : 'sem resposta');
  // Latência das últimas 60 requisições; um X vermelho = sem resposta
  const el = $('gLat'), W = Math.max(el.clientWidth, 240), H = 64, max = Math.max(200, ...ok);
  const X = i => (2 + (i + 60 - rede.lat.length) / 59 * (W - 4)).toFixed(1);
  const Y = v => (6 + (1 - v / max) * (H - 12)).toFixed(1);
  let d = '', x = '', ant = false;
  rede.lat.forEach((v, i) => {
    if (v == null) { x += 'M' + (X(i) - 3) + ' 2l6 6M' + (+X(i) + 3) + ' 2l-6 6'; ant = false; }
    else { d += (ant ? 'L' : 'M') + X(i) + ' ' + Y(v); ant = true; }
  });
  el.innerHTML = '<svg class="g" viewBox="0 0 ' + W + ' ' + H + '" width="100%" height="' + H + '" role="img" aria-label="Latência das últimas ' + rede.lat.length + ' requisições">' +
    '<path class="g-grade" stroke-dasharray="3 3" d="M0 ' + Y(100) + 'H' + W + '"/><text class="g-eixo" x="0" y="' + (Y(100) - 4) + '">100 ms</text>' +
    '<path class="g-linha umid" style="fill:none;stroke-width:1.8" d="' + d + '"/><path class="g-x" d="' + x + '"/></svg>';
}

// Gráfico de linha em SVG. pts = [{x, y}]; y null = sem leitura, vira uma
// lacuna na linha (em vez de ligar os dois lados do buraco). Com o.lacuna,
// pontos mais distantes que isso no tempo também abrem lacuna: o ESP32 não
// guarda ponto no histórico quando o DHT22 falha.
function grafico(el, pts, o) {
  const W = Math.max(el.clientWidth, 260), H = o.H, m = { l: 36, r: 12, t: 10, b: 22 };
  const ys = pts.filter(p => p.y != null).map(p => p.y);
  if (!ys.length) { el.innerHTML = '<p class="vazio">' + o.vazio + '</p>'; return; }
  let lo = Math.min(...ys), hi = Math.max(...ys);
  if (o.faixa) { lo = Math.min(lo, o.faixa[0]); hi = Math.max(hi, o.faixa[1]); }
  const folga = Math.max((hi - lo) * 0.1, o.folga); lo -= folga; hi += folga;
  let x0 = pts[0].x, x1 = pts[pts.length - 1].x;
  if (x1 === x0) { x0 -= 60000; x1 += 60000; }
  const X = v => +(m.l + (v - x0) / (x1 - x0) * (W - m.l - m.r)).toFixed(1);
  const Y = v => +(m.t + (1 - (v - lo) / (hi - lo)) * (H - m.t - m.b)).toFixed(1);
  let s = '<svg class="g" viewBox="0 0 ' + W + ' ' + H + '" width="100%" height="' + H + '" role="img" aria-label="' + o.aria + '">';
  if (o.faixa) s += '<rect class="g-faixa" x="' + m.l + '" y="' + Y(o.faixa[1]) + '" width="' + (W - m.l - m.r) + '" height="' + (Y(o.faixa[0]) - Y(o.faixa[1])) + '"/>';
  // Marcas do eixo vertical: os limites da faixa ideal, ou o menor e o maior valor
  for (const v of new Set(o.faixa || [Math.min(...ys), Math.max(...ys)]))
    s += '<path class="' + (o.faixa ? 'g-faixa-l' : 'g-grade') + '" d="M' + m.l + ' ' + Y(v) + 'H' + (W - m.r) + '"/>' +
         '<text class="g-eixo" x="' + (m.l - 6) + '" y="' + (Y(v) + 4) + '" text-anchor="end">' + fmt(v, o.casas) + '</text>';
  let d = '', ant = false, ult = null;
  for (const p of pts) {
    if (p.y == null) { ant = false; continue; }
    if (ant && o.lacuna && p.x - ult.x > o.lacuna) ant = false;
    d += (ant ? 'L' : 'M') + X(p.x) + ' ' + Y(p.y);
    ant = true; ult = p;
    if (o.pontos) s += '<circle class="' + o.cor + '" cx="' + X(p.x) + '" cy="' + Y(p.y) + '" r="3.5"/>';
  }
  s += '<path class="g-linha ' + o.cor + '" style="fill:none" d="' + d + '"/>';
  if (!o.pontos) s += '<circle class="' + o.cor + '" cx="' + X(ult.x) + '" cy="' + Y(ult.y) + '" r="4" style="stroke:var(--cartao);stroke-width:2"/>';
  s += '<text class="g-eixo" x="' + m.l + '" y="' + (H - 6) + '">' + o.rotX(x0) + '</text>' +
       '<text class="g-eixo" x="' + (W - m.r) + '" y="' + (H - 6) + '" text-anchor="end">' + o.rotFim(x1, ult) + '</text>';
  el.innerHTML = s + '</svg>';
}

// ---------------------------------------------------------------- ilustrações
// Vista térmica: o ninho inteiro com UMA cor, porque há um sensor só (DHT22).
// Verde dentro da faixa; fora dela, a cor anda até azul (frio) ou vermelho
// (quente) puros a 3 °C do limite.
// Por que HSL: misturando em RGB, o meio do caminho entre verde e vermelho
// é um marrom igual ao do ninho na vista normal, e a vista térmica parece
// desligada justamente quando a temperatura começa a sair da faixa.
// Interpolando o MATIZ (o "H" do HSL), a cor gira pela roda de cores:
// verde -> amarelo -> laranja -> vermelho quando esquenta, e
// verde -> ciano -> azul quando esfria.
// Cada cor: [matiz em graus, saturação %, luminosidade %] (as mesmas
// #3f8a5c, #3d6fa8 e #c4412a do design, convertidas para HSL)
const VERDE = [143, 37, 39], AZUL = [212, 47, 45], VERM = [9, 65, 47];
const hsl = c => 'hsl(' + c[0] + ',' + c[1] + '%,' + c[2] + '%)';
function misturar(a, b, k) {      // k = 0 -> cor a; k = 1 -> cor b
  return hsl(a.map((v, i) => Math.round(v + (b[i] - v) * k)));
}
function corTermica(t, tmin, tmax) {
  if (t < tmin) return misturar(VERDE, AZUL, Math.min(1, (tmin - t) / 3));
  if (t > tmax) return misturar(VERDE, VERM, Math.min(1, (t - tmax) / 3));
  return hsl(VERDE);
}
let termica = false;
function vista(t) {
  termica = t;
  $('vNormal').setAttribute('aria-pressed', !t);
  $('vTermica').setAttribute('aria-pressed', t);
  desenharNinho();
}

function desenharNinho() {
  const d = dados;
  if (!d) return;
  const t = d.temp, semT = t == null;
  const term = termica && !semT;              // sem leitura não há cor para pintar
  $('vTermica').disabled = semT;
  $('semTermica').hidden = !semT;
  const cor = semT ? '' : corTermica(t, d.temp_min, d.temp_max);
  // Cor de cada parte: a ilustração na vista normal; na térmica, tudo da cor da temperatura
  const c = term ? { borda: cor, oco: cor, penas: cor, asa: cor, crista: cor, bochecha: cor, bico: cor, corpo: cor, traco: '#fff', brilho: '#fff' }
    : { borda: '#a87a4f', oco: '#6e4b30', penas: '#f2dc9b', asa: '#e3c47a', crista: '#d9b65e', bochecha: '#f0a46a', bico: '#d9a58e', corpo: '#c08f5f', traco: 'none', brilho: '#d6ab7d' };
  const tr = ' stroke="' + c.traco + '" stroke-width="2"';
  const sens = semT ? 'var(--alerta-txt)' : 'var(--suave)';
  let s = '<svg class="g" viewBox="0 0 400 214" width="100%" role="img" aria-label="Ninho com o filhote' +
    (semT ? ', temperatura sem leitura' : ', ' + fmt(t, 1) + ' graus') + '">' +
    '<g transform="translate(' + (term ? -12 : 30) + ',0)">' +
    '<rect x="76" y="174" width="188" height="10" rx="4" style="fill:var(--linha)"/>' +
    '<rect x="92" y="184" width="16" height="8" rx="2" fill="#d8ccbf"/><rect x="232" y="184" width="16" height="8" rx="2" fill="#d8ccbf"/>' +
    '<text class="il-t" x="170" y="208" text-anchor="middle">balança (HX711)</text>' +
    '<ellipse cx="170" cy="112" rx="114" ry="24" fill="' + c.borda + '"' + tr + '/>' +
    '<ellipse cx="170" cy="110" rx="96" ry="16" fill="' + c.oco + '"' + tr + '/>' +
    '<ellipse cx="162" cy="102" rx="44" ry="30" fill="' + c.penas + '"' + tr + '/>' +
    '<path d="M138 94C148 80 178 82 186 102C170 110 150 110 138 94Z" fill="' + c.asa + '"' + tr + '/>' +
    '<path d="M196 52C190 38 180 32 170 32M203 50C201 36 195 28 186 24" fill="none" stroke="' + c.crista + '" stroke-width="4" stroke-linecap="round"/>' +
    '<circle cx="202" cy="72" r="22" fill="' + c.penas + '"' + tr + '/>' +
    '<circle cx="211" cy="78" r="6" fill="' + c.bochecha + '"/><circle cx="209" cy="66" r="3.2" fill="#2b2420"/>' +
    '<path d="M222 69C233 71 233 81 222 83Z" fill="' + c.bico + '"' + tr + '/>' +
    '<path d="M56 112A114 24 0 0 0 284 112C278 170 62 170 56 112Z" fill="' + c.corpo + '"' + tr + '/>';
  if (!term) s += '<path d="M66 120C110 138 230 140 276 120M72 132C120 150 210 152 266 132M88 148C140 160 200 160 252 146" fill="none" stroke="#8a6240" stroke-width="2.5" stroke-linecap="round"/>';
  s += '<path d="M56 112A114 24 0 0 0 284 112" fill="none" stroke="' + c.brilho + '" stroke-width="3"/>' +
    // o DHT22 preso na borda do ninho: a temperatura é medida só neste ponto
    '<path d="M62 104L36 62H14" style="fill:none;stroke:' + sens + ';stroke-width:1.2"/>' +
    '<rect x="56" y="98" width="15" height="19" rx="2.5" style="fill:#fff;stroke:' + sens + ';stroke-width:1.8"/>' +
    '<path d="M59 103H68M59 107H68M59 111H68" stroke="#a89c93"/>' +
    '<text x="14" y="56" font-size="11" font-weight="600" style="fill:' + sens + '">' + (semT ? 'DHT22 · sem leitura' : 'sensor DHT22') + '</text>';
  // Etiqueta do peso só na vista normal: na térmica ela ficaria embaixo da
  // seta da régua (o peso continua nos números logo abaixo do desenho)
  if (!term && !d.balanca_ok)
    s += '<rect x="250" y="26" width="92" height="30" rx="9" stroke-dasharray="4 3" style="fill:var(--cartao);stroke:var(--alerta);stroke-width:1.5"/>' +
         '<text x="296" y="45" text-anchor="middle" font-size="11" font-weight="600" style="fill:var(--alerta-txt)">balança sem leitura</text>';
  else if (!term && d.peso >= 5)
    s += '<path d="M224 62L262 46" style="stroke:var(--suave)"/><rect x="262" y="22" width="70" height="42" rx="9" style="fill:var(--cartao);stroke:var(--linha);stroke-width:1.5"/>' +
         '<text x="297" y="45" text-anchor="middle" font-size="18" font-weight="700" style="fill:var(--texto)">' + fmt(d.peso, 1) + ' g</text>' +
         '<text class="il-t" x="297" y="57" text-anchor="middle">na balança</text>';
  s += '</g>';
  if (term) {
    // Régua de 22 a 40 °C com a faixa ideal marcada e a seta na temperatura atual
    const y = v => 190 - (Math.max(22, Math.min(40, v)) - 22) / 18 * 170;
    const o = v => ((y(v) - 20) / 170).toFixed(3);   // posição na régua, de 0 (topo) a 1
    const yMax = y(d.temp_max), yMin = y(d.temp_min), yv = y(t);
    // A régua usa a mesma corTermica() do ninho, de 0,5 em 0,5 °C: o
    // degradê do SVG misturaria em RGB entre paradas distantes
    let paradas = '';
    for (let v = 40; v >= 22; v -= 0.5) paradas += '<stop offset="' + o(v) + '" stop-color="' + corTermica(v, d.temp_min, d.temp_max) + '"/>';
    s += '<defs><linearGradient id="escala" x1="0" y1="0" x2="0" y2="1">' + paradas + '</linearGradient></defs>' +
      '<text class="il-t" x="359" y="12" text-anchor="middle">°C</text>' +
      '<rect x="352" y="20" width="14" height="170" rx="7" fill="url(#escala)"/>' +
      '<path d="M368 ' + yMax + 'H372V' + yMin + 'H368" style="fill:none;stroke:var(--texto)"/>' +
      '<text x="376" y="' + (yMax + 4) + '" font-size="11" style="fill:var(--texto)">' + fmt(d.temp_max, 0) + '</text>' +
      '<text x="376" y="' + (yMin + 4) + '" font-size="11" style="fill:var(--texto)">' + fmt(d.temp_min, 0) + '</text>' +
      '<path d="M338 ' + (yv - 6) + 'L349 ' + yv + 'L338 ' + (yv + 6) + 'Z" style="fill:var(--texto)"/>' +
      '<text x="334" y="' + (yv + 4) + '" text-anchor="end" font-size="13" font-weight="700" style="fill:var(--texto)">' + fmt(t, 1) + '</text>';
  }
  $('ninhoSvg').innerHTML = s + '</svg>';
  $('legenda').hidden = !term;
  $('corLeg').style.background = cor;
}

// Comedouro: o nível desenhado é o do PRATO, que é onde o HC-SR04 mede.
// O reservatório aparece, mas sem nível, porque nenhum sensor mede quanto
// tem nele.
function desenharComedouro(nivel) {
  const d = dados, sem = d.dist == null;
  const h = Math.round((nivel || 0) * 40);   // o prato tem 40 de altura útil no desenho
  const yS = 214 - h;                         // superfície da comida
  let s = '<svg class="g" viewBox="0 0 220 240" width="100%" role="img" aria-label="Comedouro: ' +
    (sem ? 'sensor de distância sem leitura' : Math.round(nivel * 100) + '% de comida no prato') + ', comporta ' + (d.aberta ? 'aberta' : 'fechada') + '">' +
    '<defs><clipPath id="prato"><path d="M40 170H180L168 214H52Z"/></clipPath>' +
    '<pattern id="sementes" width="8" height="8" patternUnits="userSpaceOnUse"><circle cx="2" cy="2" r="1.3" fill="#b9832f"/><circle cx="6" cy="6" r="1.3" fill="#b9832f"/></pattern></defs>' +
    '<rect x="60" y="6" width="80" height="64" rx="8" class="il-c" stroke-dasharray="4 3"/>' +
    '<text class="il-t" x="100" y="34" text-anchor="middle" style="font-size:10px">reservatório</text>' +
    '<text class="il-t" x="100" y="48" text-anchor="middle" style="font-size:9px">nível não medido</text>' +
    '<path d="M64 70L94 90H106L136 70Z" class="il-c"/><rect x="94" y="90" width="12" height="10" class="il-c"/>' +
    // servo (SG90) e a comporta, que gira 70° quando abre
    '<rect x="62" y="98" width="22" height="14" rx="2" fill="#3a312b"/><circle cx="86" cy="105" r="3" style="fill:var(--suave)"/>' +
    '<rect x="86" y="101" width="26" height="4" rx="1.5" style="fill:var(--botao)" transform="rotate(' + (d.aberta ? 70 : 0) + ' 86 105)"/>';
  if (d.aberta) s += '<g fill="#d6a24a"><circle cx="100" cy="120" r="2"/><circle cx="97" cy="132" r="2"/><circle cx="103" cy="144" r="2"/></g>';
  if (h > 0) s += '<g clip-path="url(#prato)"><rect x="40" y="' + yS + '" width="140" height="' + h + '" fill="#d6a24a"/>' +
    '<rect x="40" y="' + yS + '" width="140" height="' + h + '" fill="url(#sementes)"/></g>';
  s += '<path d="M40 170H180L168 214H52Z" style="fill:none;stroke:var(--suave);stroke-width:1.5"/>' +
    '<path d="M182 174H188M182 214H188" style="stroke:var(--suave)"/>' +
    '<text class="il-t" x="191" y="177">cheio</text>' +
    '<text x="191" y="217" font-size="11"' + (d.vazio ? ' font-weight="700" style="fill:var(--alerta-txt)"' : ' class="il-t"') + '>vazio</text>' +
    // HC-SR04 em cima do prato, medindo a distância até a comida
    '<text class="il-t" x="150" y="127" text-anchor="middle" font-size="9">HC-SR04</text>' +
    '<rect x="128" y="132" width="44" height="14" rx="3" style="fill:var(--frio)"/>' +
    '<circle cx="139" cy="139" r="5" fill="#dfe5ea"/><circle cx="161" cy="139" r="5" fill="#dfe5ea"/>';
  if (sem) s += '<text x="150" y="162" text-anchor="middle" font-size="11" font-weight="600" style="fill:var(--alerta-txt)">sem leitura</text>';
  else s += '<path d="M150 147V' + (yS - 1) + '" stroke-width="1.5" stroke-dasharray="3 3" style="stroke:var(--frio)"/>' +
    '<path d="M146 ' + (yS - 6) + 'L150 ' + (yS - 1) + 'L154 ' + (yS - 6) + '" stroke-width="1.5" style="fill:none;stroke:var(--frio)"/>' +
    '<text x="145" y="' + Math.round((147 + yS) / 2 + 4) + '" text-anchor="end" font-size="11" font-weight="600" style="fill:var(--frio-txt)">' + fmt(d.dist, 1) + ' cm</text>';
  $('comSvg').innerHTML = s + '</svg>';
}

function desenharGraficos() {
  if (!dados || $('painel').hidden) return;
  const periodo = histDoEsp ? 'últimas 6 h' : 'desde que a página abriu';
  $('tTemp').textContent = 'Temperatura · ' + periodo;
  $('tUmid').textContent = 'Umidade · ' + periodo;
  const hora = ms => hms(new Date(ms)).slice(0, 5);
  // Os pontos guardados (1 por minuto) mais a leitura mais recente no fim
  const pts = hist.concat({ t: tUltimaResposta, temp: dados.temp, umid: dados.umid });
  grafico($('gTemp'), pts.map(h => ({ x: h.t, y: h.temp })), {
    H: 130, cor: 'temp', folga: 1, casas: 0, faixa: [dados.temp_min, dados.temp_max], lacuna: 150000,
    vazio: 'Aguardando leituras do DHT22.', aria: 'Temperatura do ninho ' + periodo,
    rotX: hora, rotFim: (x, u) => 'agora · ' + fmt(u.y, 1) + ' °C' });
  grafico($('gUmid'), pts.map(h => ({ x: h.t, y: h.umid })), {
    H: 110, cor: 'umid', folga: 2, casas: 0, lacuna: 150000,
    vazio: 'Aguardando leituras do DHT22.', aria: 'Umidade do ninho ' + periodo,
    rotX: hora, rotFim: (x, u) => 'agora · ' + fmt(u.y, 0) + ' %' });
  $('notaFaixa').textContent = 'Faixa verde = ideal (' + fmt(dados.temp_min, 0) + '–' + fmt(dados.temp_max, 0) + ' °C), informada pelo ESP32. Lacuna na linha = sem leitura.';
}

// ---------------------------------------------------------------- crescimento
// Faixa de referência de peso de calopsita por idade: Ritchie, Harrison e
// Harrison, "Avian Medicine" (via Cockatiel Cottage). A tabela dá intervalos
// de dias; aqui entra o ponto médio de cada intervalo (dias) com o peso
// mínimo e o máximo (g).
const REF_DIAS = [4.5, 10.5, 17.5, 24.5, 31.5, 38.5, 45.5, 52.5];
const REF_MIN = [5, 12, 45, 72, 80, 80, 80, 90];
const REF_MAX = [12, 45, 72, 108, 120, 90, 95, 110];

// Interpolação monotônica (Fritsch–Carlson): curva suave que passa pelos
// pontos da tabela e nunca passa do maior nem do menor valor entre dois
// pontos vizinhos (uma spline comum poderia "inventar" um pico que a tabela
// não tem). Antes de 4,5 dias não há referência; depois de 52,5 segue reta.
function monotonica(xs, ys) {
  const n = xs.length, m = [], t = [];
  for (let i = 0; i < n - 1; i++) m.push((ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]));   // inclinação de cada trecho
  t[0] = m[0]; t[n - 1] = m[n - 2];
  for (let i = 1; i < n - 1; i++) t[i] = m[i - 1] * m[i] <= 0 ? 0 : (m[i - 1] + m[i]) / 2;   // pico ou vale: tangente zero
  for (let i = 0; i < n - 1; i++) {      // limita as tangentes para a curva não passar dos pontos
    if (m[i] === 0) { t[i] = t[i + 1] = 0; continue; }
    const a = t[i] / m[i], b = t[i + 1] / m[i], s = a * a + b * b;
    if (s > 9) { const k = 3 / Math.sqrt(s); t[i] = k * a * m[i]; t[i + 1] = k * b * m[i]; }
  }
  return x => {
    if (x < xs[0]) return null;
    if (x >= xs[n - 1]) return ys[n - 1];
    let i = 0; while (x > xs[i + 1]) i++;
    const h = xs[i + 1] - xs[i], u = (x - xs[i]) / h, u2 = u * u, u3 = u2 * u;   // polinômio de Hermite cúbico
    return (2 * u3 - 3 * u2 + 1) * ys[i] + (u3 - 2 * u2 + u) * h * t[i] + (-2 * u3 + 3 * u2) * ys[i + 1] + (u3 - u2) * h * t[i + 1];
  };
}
const refMin = monotonica(REF_DIAS, REF_MIN), refMax = monotonica(REF_DIAS, REF_MAX);
// Classifica com a MESMA curva que é desenhada: o ponto nunca parece dentro
// da faixa no gráfico e sai classificado como fora (ou o contrário)
function classificar(idade, g) {
  const lo = refMin(idade);
  if (lo == null) return 'sem';          // antes de 4,5 dias: sem classificação
  return g < lo ? 'abaixo' : g > refMax(idade) ? 'acima' : 'ok';
}

let selPeso = -1, geoPeso = null, eclAntes = -1;
function desenharPeso() {
  const ecl = (dados && dados.eclosao) || 0;   // o firmware atual não manda o campo: "não informada"
  // Epoch 0 = pesagem feita antes de o ESP32 saber a hora: sem data, não dá para plotar
  const comData = pesagens.filter(p => p[0] > 0), sem = pesagens.length - comData.length;
  const pts = comData.map(p => {
    const idade = ecl ? (p[0] - ecl) / 86400 : null;
    return { t: p[0], g: p[1], idade, cls: ecl ? classificar(idade, p[1]) : 'sem' };
  });
  const el = $('gPeso'), W = Math.max(el.clientWidth, 260), H = W > 500 ? 270 : 230;
  const m = { l: 34, r: 10, t: 12, b: 22 };
  // Eixo X: idade em dias (com a data de eclosão) ou data da pesagem (sem ela)
  const vx = p => ecl ? p.idade : p.t;
  let x0 = 0, x1 = 56;
  if (ecl) { x0 = Math.min(0, ...pts.map(p => Math.floor(p.idade))); x1 = Math.max(56, ...pts.map(p => Math.ceil(p.idade / 7) * 7)); }
  else if (pts.length) { x0 = pts[0].t; x1 = pts[pts.length - 1].t; if (x1 === x0) { x0 -= 86400; x1 += 86400; } }
  const X = v => +(m.l + (v - x0) / (x1 - x0) * (W - m.l - m.r)).toFixed(1);
  const topo = Math.max(130, Math.ceil((Math.max(0, ...pts.map(p => p.g)) + 10) / 30) * 30);
  const Y = g => +(m.t + (1 - g / topo) * (H - m.t - m.b)).toFixed(1);
  const txt = (x, y, t, a) => '<text class="g-eixo" x="' + x + '" y="' + y + '" text-anchor="' + (a || 'middle') + '">' + t + '</text>';

  let s = '<svg class="g" viewBox="0 0 ' + W + ' ' + H + '" width="100%" height="' + H + '" role="img" aria-label="' +
    (pts.length ? 'Peso do filhote, ' + pts.length + ' pesagens' + (ecl ? ', com faixa de referência por idade' : '') : 'Gráfico de peso vazio') + '">';
  for (let g = 0; g <= topo - 10; g += 30)
    s += '<path class="g-grade" d="M' + m.l + ' ' + Y(g) + 'H' + (W - m.r) + '"/>' + txt(m.l - 6, Y(g) + 4, g + (g + 30 > topo - 10 ? ' g' : ''), 'end');
  if (ecl) {
    // Faixa de referência, de 4,5 dias até o fim do eixo, de meio em meio dia
    const cima = [], baixo = [];
    for (let d = 4.5; d <= x1; d += 0.5) { cima.push(X(d) + ' ' + Y(refMax(d))); baixo.push(X(d) + ' ' + Y(refMin(d))); }
    s += '<path class="g-faixa" d="M' + cima.join('L') + 'L' + baixo.reverse().join('L') + 'Z"/>';
    // A própria tabela tem uma queda de peso perto da 5ª semana. A nota fica
    // ACIMA da faixa, onde não cai nenhum ponto "abaixo da faixa".
    const a = X(38.5), b = X(49), y = Y(102), mx = ((a + b) / 2).toFixed(1);
    s += '<path d="M' + a + ' ' + (y + 4) + 'V' + y + 'H' + b + 'V' + (y + 4) + '" style="fill:none;stroke:var(--suave)"/>' +
         '<text class="g-eixo" x="' + mx + '" y="' + (y - 18) + '" text-anchor="middle" font-style="italic">perda normal ao</text>' +
         '<text class="g-eixo" x="' + mx + '" y="' + (y - 6) + '" text-anchor="middle" font-style="italic">começar a voar</text>';
    const passo = W < 400 && x1 - x0 > 56 ? 14 : 7;
    for (let d = Math.ceil(x0 / 7) * 7; d <= x1; d += passo) s += txt(X(d), H - 6, d);
  } else if (pts.length) {
    for (let i = 0; i < 4; i++) { const t = x0 + i * (x1 - x0) / 3; s += txt(X(t), H - 6, ddmm(new Date(t * 1000)), ['start', 'middle', 'middle', 'end'][i]); }
  }
  // Linha e pontos: abaixo da faixa em --alerta; acima com destaque neutro (anel)
  s += '<path class="g-linha temp" style="fill:none;stroke-width:2.5" d="' + pts.map((p, i) => (i ? 'L' : 'M') + X(vx(p)) + ' ' + Y(p.g)).join('') + '"/>';
  for (const p of pts) {
    const c = ' cx="' + X(vx(p)) + '" cy="' + Y(p.g) + '"';
    if (p.cls === 'abaixo') s += '<circle' + c + ' r="9" style="fill:var(--alerta);opacity:.22"/><circle' + c + ' r="5" style="fill:var(--alerta);stroke:var(--cartao);stroke-width:1.5"/>';
    else if (p.cls === 'acima') s += '<circle' + c + ' r="4" style="fill:var(--cartao);stroke:var(--texto);stroke-width:2"/>';
    else s += '<circle' + c + ' r="3.5" style="fill:var(--marca);stroke:var(--cartao);stroke-width:1.5"/>';
  }
  // Pesagem selecionada (mouse ou toque): guia, anel e caixa com os detalhes
  const sel = pts[selPeso];
  if (sel) {
    const px = X(vx(sel)), py = Y(sel.g), tw = 196;
    const bx = Math.max(m.l, Math.min(W - m.r - tw, px - tw / 2));
    const by = py - 76 < m.t ? py + 14 : py - 76;
    const dt = new Date(sel.t * 1000);
    const l1 = ddmm(dt) + '/' + dt.getFullYear() + (ecl ? ' · ' + Math.floor(sel.idade) + ' dias' : '');
    const l2 = fmt(sel.g, 1) + ' g' + ({ ok: ' · dentro da faixa', abaixo: ' · abaixo da faixa', acima: ' · acima da faixa' }[sel.cls] || '');
    const l3 = sel.cls === 'sem' ? '' : 'esperado: ' + Math.round(refMin(sel.idade)) + ' a ' + Math.round(refMax(sel.idade)) + ' g';
    s += '<path d="M' + px + ' ' + m.t + 'V' + (H - m.b) + '" stroke-dasharray="3 3" style="stroke:var(--suave)"/>' +
      '<circle cx="' + px + '" cy="' + py + '" r="7" style="fill:none;stroke:var(--marca);stroke-width:2"/>' +
      '<rect x="' + bx + '" y="' + by + '" width="' + tw + '" height="62" rx="8" style="fill:var(--texto)"/>' +
      '<text x="' + (bx + 10) + '" y="' + (by + 17) + '" font-size="11" style="fill:var(--cartao)">' + l1 + '</text>' +
      '<text x="' + (bx + 10) + '" y="' + (by + 36) + '" font-size="14" font-weight="700" style="fill:var(--cartao)">' + l2 + '</text>' +
      '<text x="' + (bx + 10) + '" y="' + (by + 53) + '" font-size="11" style="fill:var(--cartao)">' + l3 + '</text>';
  }
  el.innerHTML = s + '</svg>' + (pts.length ? '' :
    '<div class="sobre"><b>Nenhuma pesagem ainda</b>Quando o filhote for pesado na balança, o primeiro ponto aparece aqui.</div>');
  geoPeso = { W, xs: pts.map(p => X(vx(p))) };

  // Selo: "abaixo do esperado" só se a ÚLTIMA pesagem estiver abaixo
  const ult = pts[pts.length - 1], nAcima = pts.filter(p => p.cls === 'acima').length;
  let selo = '';
  if (ult && ult.cls === 'abaixo')
    selo += '<div class="av q" role="status"><span class="bola"></span><div><b>Abaixo do esperado para ' + Math.floor(ult.idade) + ' dias</b>' +
      fmt(ult.g, 1) + ' g na última pesagem; a referência nessa idade vai de ' + Math.round(refMin(ult.idade)) + ' a ' + Math.round(refMax(ult.idade)) + ' g.</div></div>';
  if (nAcima)
    selo += '<div class="av neutro" role="status"><span class="bola anel"></span><span><b>' + nAcima + (nAcima > 1 ? ' pesagens acima' : ' pesagem acima') +
      ' da faixa de referência.</b> Não indica problema por si só.</span></div>';
  $('seloPeso').innerHTML = selo;
  $('resumoPeso').textContent = !ult ? '' : (ecl ? Math.floor(ult.idade) + ' dias · ' : 'última: ' + ddmm(new Date(ult.t * 1000)) + ' · ') + fmt(ult.g, 1) + ' g';
  $('eixoPeso').textContent = ecl ? 'Idade em dias desde a eclosão · marcações por semana' : 'Data da pesagem (sem a data de eclosão não dá para calcular a idade)';
  $('legPeso').innerHTML = '<span><svg width="22" height="10" aria-hidden="true"><path d="M1 5H21" style="stroke:var(--marca);stroke-width:2.5"/><circle cx="11" cy="5" r="3.5" style="fill:var(--marca)"/></svg>Peso do filhote</span>' +
    (ecl ? '<span><svg width="22" height="10" aria-hidden="true"><rect width="22" height="10" rx="2" class="g-faixa" style="opacity:.3"/></svg>Faixa de referência (Ritchie et al.)</span>' : '') +
    (pts.some(p => p.cls === 'abaixo') ? '<span><span class="bola" style="margin:0"></span>Abaixo da faixa</span>' : '') +
    (nAcima ? '<span><span class="bola anel"></span>Acima da faixa</span>' : '');
  $('fontePeso').textContent = ecl ? 'Faixa: Ritchie, Harrison e Harrison, Avian Medicine (via Cockatiel Cottage), interpolada entre os pontos médios de cada intervalo.' : '';
  $('notaPeso').textContent = sem ? sem + (sem > 1 ? ' pesagens sem data (epoch 0) não aparecem' : ' pesagem sem data (epoch 0) não aparece') + ' no gráfico.' : '';

  // Formulário da eclosão: aberto se a data não foi informada; só mexe
  // nele quando a data muda, para não fechar enquanto a pessoa digita
  if (ecl !== eclAntes) {
    eclAntes = ecl;
    $('ecl').open = !ecl;
    if (ecl) {
      const d = new Date(ecl * 1000);
      $('eclTit').textContent = 'Eclosão em ' + ddmm(d) + '/' + d.getFullYear() + ' · alterar';
      $('dataEcl').value = d.getFullYear() + '-' + dois(d.getMonth() + 1) + '-' + dois(d.getDate());
    } else $('eclTit').textContent = 'Informe a data de eclosão';
  }
}

async function salvarEclosao(ev) {
  ev.preventDefault();                       // o formulário não navega: quem envia é o fetch
  const [a, m, d] = $('dataEcl').value.split('-').map(Number);
  const t = Math.floor(new Date(a, m - 1, d).getTime() / 1000);   // meia-noite, horário local
  if (!(t > 0)) return;
  if (t > Date.now() / 1000) { $('eclMsg').textContent = 'A data de eclosão não pode estar no futuro.'; return; }
  try {
    const r = await pedir('/eclosao?t=' + t, 'POST');
    // 404 = o firmware ainda não tem essa rota: avisa sem quebrar o resto
    $('eclMsg').textContent = r.status === 404 ? 'Data de eclosão ainda não suportada pelo firmware.' :
      r.j ? r.j.msg : 'O ESP32 respondeu com erro ' + r.status + '.';
  } catch (e) {
    $('eclMsg').textContent = 'Não consegui falar com o ESP32.';
  }
  ciclo();
}

// Mouse ou toque em qualquer parte do gráfico seleciona a pesagem mais
// próxima no eixo X (no celular não dá para acertar um ponto de 7 px)
function escolherPeso(e) {
  if (!geoPeso || !geoPeso.xs.length) return;
  const r = $('gPeso').getBoundingClientRect();
  const x = (e.clientX - r.left) * geoPeso.W / r.width;
  let melhor = 0;
  geoPeso.xs.forEach((v, i) => { if (Math.abs(v - x) < Math.abs(geoPeso.xs[melhor] - x)) melhor = i; });
  if (melhor !== selPeso) { selPeso = melhor; desenharPeso(); }
}

// ---------------------------------------------------------------- log
function quando(e) {
  if (e.t) { const d = new Date(e.t * 1000); return ddmm(d) + ' · ' + hms(d); }
  return 'sem hora · boot ' + e.boot + ' · ' + duracao(e.ms / 1000) + ' após ligar';
}
function itemLog(e) {
  return '<div class="linha-ev"><span class="nv ' + e.nivel + '">' + e.nivel + '</span><span class="nota">' + quando(e) + '</span></div>';
}
function desenharLog() {
  const n = { '': logs.length, ERRO: 0, AVISO: 0, INFO: 0 };
  logs.forEach(e => n[e.nivel]++);
  for (const b of document.querySelectorAll('.filtros button'))
    b.textContent = (b.dataset.n ? b.dataset.n[0] + b.dataset.n.slice(1).toLowerCase() : 'Todos') + ' · ' + n[b.dataset.n];
  // Agrupa por boot, mantendo a ordem (mais recente primeiro)
  const grupos = [];
  for (const e of logs) {
    if (!grupos.length || grupos[grupos.length - 1].boot !== e.boot) grupos.push({ boot: e.boot, ev: [] });
    grupos[grupos.length - 1].ev.push(e);
  }
  let h = '';
  for (const g of grupos) {
    const vis = g.ev.filter(e => !filtro || e.nivel === filtro);
    if (!vis.length) continue;
    const atual = g.boot === bootAtual;
    const direita = atual && dados ? 'ligado há ' + duracao(dados.uptime_s) : g.ev.every(e => !e.t) ? 'sem relógio' : '';
    h += '<section class="boot"><div><h3 class="rot">Boot ' + g.boot + (atual ? ' · atual' : '') + '</h3><span class="nota">' + direita + '</span></div><ul>';
    for (const e of vis) h += '<li' + (novos.has(e.id) ? ' class="novo"' : '') + '>' + itemLog(e) + '<p>' + esc(e.msg) + '</p></li>';
    h += '</ul></section>';
  }
  $('listaLog').innerHTML = h || '<p class="vazio">' + (logs.length ? 'Nenhum evento deste nível.' : 'Nenhum evento registrado ainda.') + '</p>';
  $('rodapeLog').textContent = 'Mostrando ' + logs.length + ' eventos guardados no ESP32';
}
function desenharUltimos() {
  $('ultimos').innerHTML = logs.slice(0, 4).map(e => '<div class="ev">' + itemLog(e) + '<span>' + esc(e.msg) + '</span></div>').join('') ||
    '<p class="vazio">Nenhum evento ainda.</p>';
}
function filtrar(b) {
  filtro = b.dataset.n;
  for (const x of document.querySelectorAll('.filtros button')) x.setAttribute('aria-pressed', x === b);
  desenharLog();
}
// Contador da aba Log: ERRO e AVISO com id maior que o último já visto
function contarNaoVistos() {
  if (logs.length && vistoAte > logs[0].id) vistoAte = 0;   // log apagado: os ids recomeçaram
  const n = logs.filter(e => e.nivel !== 'INFO' && e.id > vistoAte).length;
  $('naoVistos').hidden = !n;
  $('naoVistos').textContent = n;
  $('abaLog').setAttribute('aria-label', n ? 'Log, ' + n + ' erros ou avisos não vistos' : 'Log');
}
function marcarVistos() {
  if (!logs.length) return;
  vistoAte = logs[0].id;
  try { localStorage.setItem('ninhoVisto', vistoAte); } catch (e) {}
  contarNaoVistos();
}

function mostrar(aba) {
  const log = aba === 'log';
  $('painel').hidden = log; $('log').hidden = !log;
  $('abaPainel').setAttribute('aria-selected', !log);
  $('abaLog').setAttribute('aria-selected', log);
  if (log) marcarVistos();
  else { desenharGraficos(); desenharRede(); desenharPeso(); }
  window.scrollTo(0, 0);
}

window.addEventListener('resize', () => { desenharGraficos(); desenharRede(); desenharPeso(); });
$('gPeso').addEventListener('pointermove', escolherPeso);
$('gPeso').addEventListener('pointerdown', escolherPeso);
// O mouse saiu: tira a seleção. No toque não (o pointerleave vem logo
// depois de levantar o dedo e apagaria o que a pessoa acabou de tocar).
$('gPeso').addEventListener('pointerleave', e => { if (e.pointerType === 'mouse') { selPeso = -1; desenharPeso(); } });
(async () => {
  // Acerta o relógio do ESP32 com o do navegador, caso ele esteja sem internet
  try { await pedir('/hora?ts=' + Math.floor(Date.now() / 1000)); } catch (e) {}
  await carregarHistorico();       // uma vez só, na mesma fila
  ciclo();
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

// Coloca "\" antes de aspas e barras invertidas e troca caracteres de
// controle por espaço, para o texto não quebrar o JSON.
String escaparJson(const char* s) {
  String r;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { r += '\\'; r += *s; }
    else if ((uint8_t)*s < 0x20) r += ' ';
    else r += *s;
  }
  return r;
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
  // Rede (RF4). Em modo AP mostra a interface da rede própria do ESP32;
  // senão, a interface STA (cliente do roteador). Cada uma tem o seu MAC.
  String ip   = modoAP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String mac  = modoAP ? WiFi.softAPmacAddress() : WiFi.macAddress();
  String ssid = modoAP ? AP_NOME : WIFI_NOME;

  String j = "{";
  j.reserve(950);
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
  addCampo(j, "interface", modoAP ? "\"AP\"" : "\"STA\"");
  addCampo(j, "ip", "\"" + ip + "\"");
  addCampo(j, "mac", "\"" + mac + "\"");
  addCampo(j, "ssid", "\"" + escaparJson(ssid.c_str()) + "\"");
  addCampo(j, "porta", String(PORTA_HTTP));
  addCampo(j, "conectado", WiFi.status() == WL_CONNECTED ? "true" : "false");
  addCampo(j, "uptime_s", String(millis() / 1000));
  addCampo(j, "reconexoes", String(reconexoes));
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

// Log do mais recente para o mais antigo. A resposta vai em pedaços
// ("chunked"), um registro por vez: o JSON inteiro nunca fica na RAM.
void handleLogs() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("{\"boot_atual\":" + String(bootAtual) + ",\"logs\":[");

  // Eventos sem hora: usa como referência um evento mais novo do mesmo boot
  // que tem hora. Lendo de trás para frente, a última referência guardada
  // antes de chegar neles é a mais antiga com hora: o "Hora acertada".
  // Como millis() conta desde o boot, a diferença de ms entre os dois é o
  // tempo que passou entre eles. Nada é regravado na flash.
  uint32_t bootRef = 0, tRef = 0, msRef = 0;
  bool primeiro = true;
  const char* arquivos[] = { ARQ_LOG, ARQ_LOG_ANTIGO };   // o atual tem os mais novos
  for (const char* caminho : arquivos) {
    if (!logOk || !LittleFS.exists(caminho)) continue;
    File f = LittleFS.open(caminho, FILE_READ);
    int n = f.size() / sizeof(RegistroLog);
    for (int i = n - 1; i >= 0; i--) {
      RegistroLog r;
      f.seek(i * sizeof(RegistroLog));
      if (f.read((uint8_t*)&r, sizeof(r)) != sizeof(r)) break;
      r.msg[sizeof(r.msg) - 1] = '\0';

      if (r.t != 0) { bootRef = r.boot; tRef = r.t; msRef = r.ms; }
      else if (tRef != 0 && r.boot == bootRef) r.t = tRef - (msRef - r.ms) / 1000;

      String j = primeiro ? "{" : ",{";
      j += "\"id\":" + String(r.id);
      j += ",\"boot\":" + String(r.boot);
      j += ",\"t\":" + String(r.t);
      j += ",\"ms\":" + String(r.ms);
      j += ",\"nivel\":\"";
      j += NOMES_NIVEL[r.nivel <= NIVEL_ERRO ? r.nivel : NIVEL_INFO];
      j += "\",\"msg\":\"" + escaparJson(r.msg) + "\"}";
      server.sendContent(j);
      primeiro = false;
    }
    f.close();
  }
  server.sendContent("]}");
  server.sendContent("");   // pedaço vazio = fim da resposta
}

void handleHora() {
  if (!horaValida() && server.hasArg("ts")) {
    long ts = server.arg("ts").toInt();
    if (ts > 1700000000L) {
      struct timeval tv = { (time_t)ts, 0 };
      settimeofday(&tv, nullptr);
      horaPeloNavegador = true;   // o evento "Hora acertada" é gravado no loop
    }
  }
  server.send(200, "application/json", horaValida() ? "{\"ok\":true}" : "{\"ok\":false}");
}

void handleLiberar() {
  if (abrirComporta("manual")) responder(true, "Comida liberada");
  else responder(false, "A comporta já está aberta");
}

void handleAuto() {
  bool antes = alimentacaoAuto;
  alimentacaoAuto = server.arg("on") == "1";
  prefs.putBool("auto", alimentacaoAuto);
  if (alimentacaoAuto != antes)   // só registra se realmente mudou
    registrarLog(NIVEL_INFO, alimentacaoAuto ? "Alimentação automática ligada" : "Alimentação automática desligada");
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
  registrarLog(NIVEL_AVISO, "Pesagens apagadas (" + String(nPesagens) + ")");
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
  server.on("/logs", HTTP_GET, handleLogs);
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
  // As novas tentativas ficam por conta de verificarWiFi(), no loop, em um
  // intervalo fixo. Assim é o nosso código que decide quando tentar de novo.
  WiFi.setAutoReconnect(false);
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
  Serial.printf("Conectando ao Wi-Fi \"%s\"", WIFI_NOME);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  // Hora pela internet (horário de Brasília). Se ainda não houver rede, o
  // ESP32 sincroniza sozinho quando conectar.
  configTzTime("<-03>3", "a.st1.ntp.br", "pool.ntp.org");

  if (WiFi.status() == WL_CONNECTED) {
    modoAP = false;
    Serial.printf("Conectado! Abra no navegador: http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    // Modo AP+STA: cria a rede própria (AP) e, ao mesmo tempo, continua
    // tentando a rede configurada (STA) pelo verificarWiFi().
    modoAP = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_NOME, AP_SENHA);
    registrarLog(NIVEL_AVISO, String("Entrou em modo AP (rede ") + AP_NOME + ")");
    Serial.printf("Não conectou. Criei a rede \"%s\" (senha %s).\n", AP_NOME, AP_SENHA);
    Serial.printf("Conecte nela e abra: http://%s\n", WiFi.softAPIP().toString().c_str());
  }
  if (MDNS.begin(NOME_MDNS)) {
    MDNS.addService("http", "tcp", PORTA_HTTP);
    Serial.printf("Também pode tentar: http://%s.local\n", NOME_MDNS);
  }
}

// Chamada a cada volta do loop. Compara o estado atual com o da volta
// anterior para saber quando a conexão caiu ou voltou. Não usa os callbacks
// de eventos do Wi-Fi, que rodam em outra task e não poderiam gravar o log.
void verificarWiFi() {
  unsigned long agora = millis();
  bool conectado = WiFi.status() == WL_CONNECTED;

  if (conectado && !wifiConectado) {            // acabou de conectar
    String ip = WiFi.localIP().toString();
    if (jaConectouAntes) {
      reconexoes++;
      registrarLog(NIVEL_INFO, "Wi-Fi reconectado após " + String((agora - tPerdeuWiFi) / 1000) +
                               " s fora (IP " + ip + ")");
    } else {
      registrarLog(NIVEL_INFO, "Wi-Fi conectado (IP " + ip + ")");
    }
    jaConectouAntes = true;
    if (modoAP) {                               // a rede própria não é mais necessária
      WiFi.softAPdisconnect(true);
      modoAP = false;
      Serial.printf("Rede própria desligada. Abra no navegador: http://%s\n", ip.c_str());
    }
  } else if (!conectado && wifiConectado) {     // acabou de cair
    tPerdeuWiFi = agora;
    tTentativaWiFi = agora;                     // primeira tentativa daqui a 15 s
    registrarLog(NIVEL_AVISO, "Wi-Fi perdido");
  }
  wifiConectado = conectado;

  // Sem conexão: tenta de novo a cada 15 s. O WiFi.begin() não espera a
  // conexão terminar, então o loop continua rodando normalmente.
  if (!conectado && agora - tTentativaWiFi >= INTERVALO_TENTATIVA_WIFI_MS) {
    tTentativaWiFi = agora;
    WiFi.disconnect();
    WiFi.begin(WIFI_NOME, WIFI_SENHA);
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
  iniciarLog();
  registrarInicio();
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
  verificarWiFi();
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
  verificarSensores();

  if (!horaJaValida && horaValida()) {     // o relógio acabou de ser acertado
    horaJaValida = true;
    corrigirPesagensSemHora();
    registrarLog(NIVEL_INFO, horaPeloNavegador ? "Hora acertada (navegador)" : "Hora acertada (NTP)");
  }

  if (agora - tStatus >= 5000) {
    tStatus = agora;
    imprimirStatus();
  }
  lerSerial();
}
