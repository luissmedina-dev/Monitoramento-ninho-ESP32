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
uint32_t dataEclosao = 0;          // data de eclosão do filhote (epoch em s); 0 = não informada

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
// A página fica em pagina.h porque o pré-processador do Arduino só gera
// protótipos para arquivos .ino e não entende raw literals: com a página
// aqui, ele lia as funções JS como C++ e o sketch não compilava.
#include "pagina.h"

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
  j.reserve(980);
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
  addCampo(j, "eclosao", String(dataEclosao));
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

void handleEclosao() {
  // t = epoch em segundos (a página manda a meia-noite local do dia escolhido)
  String arg = server.arg("t");
  char* fim;
  long long t = strtoll(arg.c_str(), &fim, 10);
  if (arg.length() == 0 || *fim != '\0') {
    responder(false, "Informe a data de eclosão (t = epoch em segundos)");
    return;
  }
  // Sem relógio acertado não dá para conferir se a data está no futuro
  bool noFuturo = horaValida() && t > (long long)time(nullptr) + 86400;
  if (t < 1700000000LL || t > 0xFFFFFFFFLL || noFuturo) {
    responder(false, "Data de eclosão inválida");
    return;
  }
  dataEclosao = (uint32_t)t;
  prefs.putUInt("eclosao", dataEclosao);
  registrarLog(NIVEL_INFO, "Data de eclosão salva");
  responder(true, "Data de eclosão salva");
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
  server.on("/eclosao", HTTP_POST, handleEclosao);
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
  dataEclosao = prefs.getUInt("eclosao", 0);
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
