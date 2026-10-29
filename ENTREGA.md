# Entrega: log persistente, Wi-Fi e API de rede (branch `feat/log-wifi`)

> ## ⚠️ Status da compilação: NÃO compilado/verificado
>
> Esta máquina não tinha o `arduino-cli` nem a Arduino IDE. A instalação do `arduino-cli` e do core `esp32:esp32` (3.3.12) foi iniciada, mas **interrompida antes de terminar**: as bibliotecas não chegaram a ser instaladas e **nenhuma compilação foi executada**. Não há resultado de erros nem de avisos do compilador.
>
> **Antes do code freeze (08/11), compilem na Arduino IDE** com a placa "ESP32 Dev Module" e o partition scheme padrão ("Default 4MB with spiffs"). Bibliotecas necessárias: ESP32Servo, DHT sensor library + Adafruit Unified Sensor e **HX711 Arduino Library (Bogdan Necula)**. Cuidado: a biblioteca chamada só "HX711" é outra, do Rob Tillaart.

> ## 🛑 Não fazer merge na main antes de testar no hardware

Requisitos atendidos: **RF1** (reconexão do Wi-Fi com registro no log), **RF3** (log persistente com nível, data e hora) e **RF4** (IP, MAC e status da conexão).

---

## 1. O que mudou, arquivo por arquivo

### `ninho_esp32.ino`

| Parte | Mudança |
|---|---|
| Includes | `LittleFS.h` e `esp_system.h` (para `esp_reset_reason()`). |
| Seção 1 (Wi-Fi) | Constante `INTERVALO_TENTATIVA_WIFI_MS` (15 s). O SSID e a senha **não foram alterados**. |
| Seção 3 (Ajustes) | `HISTERESE_TEMP_C` (0,5 °C) e `TEMPO_CONFIRMA_SENSOR_MS` (5 s). |
| Variáveis | `PORTA_HTTP`; estado do Wi-Fi (`wifiConectado`, `jaConectouAntes`, `tPerdeuWiFi`, `tTentativaWiFi`, `reconexoes`); `struct RegistroLog` de 80 bytes e constantes do log; estado dos sensores já registrado no log; `horaPeloNavegador`. |
| Nova seção "LOG DO SISTEMA" | `registrarLog()` grava no Serial e na flash, com rotação entre `/log.bin` e `/log_antigo.bin` (75 registros cada). `iniciarLog()` incrementa o contador de boots na NVS, monta o LittleFS e recupera o último `id`. `registrarInicio()` grava "ESP32 iniciado" com o motivo do reset (brownout, watchdog e panic também viram ERRO). |
| Nova seção "EVENTOS DOS SENSORES" | `mudouEstado()` confirma a mudança por 5 s. `verificarSensores()` registra ERRO/INFO do DHT22, HX711 e HC-SR04 e o AVISO de temperatura fora da faixa, com histerese. Ela **só lê** variáveis que já existiam; a lógica dos sensores não mudou. |
| Comedouro e balança | Os `Serial.printf` de comida liberada, pesagem, tara e calibração viraram `registrarLog()`, que também imprime no Serial. Nenhuma lógica foi alterada. |
| Servidor | `escaparJson()`; novo `GET /logs` (resposta chunked, do mais novo ao mais antigo, com a hora calculada para eventos sem hora); `/dados` ganhou `interface`, `ip`, `mac`, `ssid`, `porta`, `conectado`, `uptime_s` e `reconexoes`; `/auto` só registra quando muda; `/limpar` registra AVISO; `/hora` marca que a hora veio do navegador. |
| Wi-Fi | `conectarWiFi()`: autoreconexão do core desligada, NTP configurado sempre e, se falhar, modo **AP+STA** com AVISO no log. Novo `verificarWiFi()` no `loop()`: detecta queda e volta, registra os eventos, conta reconexões, tenta `WiFi.begin()` a cada 15 s e **desliga o AP quando a STA conecta**. |
| `setup()` / `loop()` | `iniciarLog()` e `registrarInicio()` logo depois do `prefs.begin()`; `verificarWiFi()` e `verificarSensores()` no `loop()`; evento "Hora acertada (NTP/navegador)". |
| Cabeçalho | Comentário "Como usar" explica o modo AP+STA e o `/logs`. |

Não foram alterados: pinos, lógica dos sensores e do comedouro, HTML/CSS/JS da página e credenciais.

### `API.md` (novo)
Todos os 13 endpoints com método, campos e um exemplo de `curl` para cada um; a lista de eventos do log e o comportamento do Wi-Fi.

### `ENTREGA.md` (novo)
Este arquivo.

### Limitações conhecidas (sabidas e aceitas)
- **O AP não é religado depois de uma queda em execução.** Se o Wi-Fi cair com o ESP32 já rodando, ele só tenta reconectar a STA. Enquanto o roteador não voltar, o ESP32 fica inacessível pela rede, mas continua lendo os sensores, cuidando do comedouro e gravando o log.
- **Enquanto está em AP+STA**, a cada tentativa a STA procura a rede trocando de canal. O ESP32 tem um rádio só, então quem está ligado no AP pode ter pequenas quedas a cada 15 s.
- O cálculo da hora dos eventos sem hora usa `millis()`, que volta a zero depois de cerca de 49 dias ligados. Nesse caso raro, o `t` calculado sairia errado.
- As credenciais do Wi-Fi continuam no código e no histórico do Git (decisão da equipe).

---

## 2. Roteiro de teste no hardware

Preparação: gravem o código, abram o Monitor Serial em 115200 e anotem o IP. Os eventos também aparecem no Serial, como `[AVISO] Wi-Fi perdido`.

```sh
IP=192.168.0.23        # troque pelo IP do ESP32
# No Windows PowerShell, use curl.exe em vez de curl
```

Para ver o log com o JSON formatado (isso também confirma que o JSON é válido):

```sh
curl -s http://$IP/logs | python -m json.tool
```

| # | Evento | Como provocar | Como conferir |
|---|---|---|---|
| 1 | INFO "ESP32 iniciado (energia ligada ou botão EN)" | Apertar o botão EN ou desligar e ligar a USB | `curl http://$IP/logs`: primeiro evento do boot, e `boot_atual` subiu 1 |
| 2 | Persistência do log | Desligar o ESP32 da energia e ligar de novo | `/logs` ainda mostra os eventos com `boot` anterior |
| 3 | INFO "Wi-Fi conectado (IP …)" | Boot normal com o roteador ligado | `/logs` |
| 4 | INFO "Hora acertada (NTP)" | Boot com o roteador ligado e com internet | `/logs`; em `/dados`, `hora_ok: true` |
| 5 | AVISO "Wi-Fi perdido" | Desligar o roteador e esperar uns 10 s | Ver no Serial (o ESP32 fica fora da rede). Depois do item 6, aparece no `/logs` |
| 6 | INFO "Wi-Fi reconectado após N s fora" | Religar o roteador | `curl http://$IP/logs` e `curl http://$IP/dados` (`reconexoes` = 1, `conectado: true`) |
| 7 | AVISO "Entrou em modo AP" | Ligar o ESP32 com o roteador desligado e esperar 20 s | Conectar na rede `Ninho-ESP32` (senha `ninho1234`) e rodar `curl http://192.168.4.1/dados`: `interface: "AP"`, `ip: "192.168.4.1"`, MAC do AP |
| 8 | INFO "Wi-Fi conectado" + AP desligado | Com o ESP32 em modo AP, religar o roteador | Em até ~15 s a rede `Ninho-ESP32` some; usar o IP do roteador: `curl http://$IP/dados` mostra `interface: "STA"` |
| 9 | INFO "Hora acertada (navegador)" + hora retroativa | Boot em modo AP (item 7) e abrir `http://192.168.4.1` no celular | No `/logs`, "Hora acertada (navegador)" aparece, e os eventos anteriores **do mesmo boot** passam a ter `t` diferente de 0 |
| 10 | ERRO "DHT22 sem leitura" / INFO "DHT22 voltou a responder" | Tirar o fio de dados do DHT22 (GPIO 27); recolocar depois | `/logs`: o ERRO aparece em ~13 s (3 falhas + 5 s) e o INFO ~5 s depois de recolocar |
| 11 | ERRO/INFO HX711 | Tirar o fio DT do HX711 (GPIO 13); recolocar | `/logs` (~7 s) |
| 12 | ERRO/INFO HC-SR04 | Tirar o fio ECHO (GPIO 35); recolocar | `/logs` (~5 s) |
| 13 | AVISO "Temperatura fora da faixa" / INFO "voltou à faixa" | Esquentar o DHT22 acima de 35 °C (secador, com cuidado) ou esfriar abaixo de 25 °C (gelo em saco plástico) | `/logs`: um único AVISO ao passar do limite. O INFO só aparece abaixo de 34,5 °C ou acima de 25,5 °C (histerese) |
| 14 | INFO "Comida liberada (manual)" | `curl -X POST http://$IP/liberar` | `/logs` |
| 15 | INFO "Comida liberada (automática)" | Esvaziar o prato com a alimentação automática ligada | `/logs` |
| 16 | INFO "Alimentação automática desligada/ligada" | `curl -X POST "http://$IP/auto?on=0"` e depois `?on=1` | `/logs` (repetir o mesmo valor não gera evento) |
| 17 | INFO "Pesagem manual registrada" | Objeto na balança e `curl -X POST http://$IP/pesar` | `/logs` |
| 18 | INFO "Balança tarada" | Balança vazia e `curl -X POST http://$IP/tarar` | `/logs` |
| 19 | INFO "Balança calibrada" | Objeto de 100 g e `curl -X POST "http://$IP/calibrar?g=100"` | `/logs` |
| 20 | AVISO "Pesagens apagadas" | `curl -X POST http://$IP/limpar` | `/logs` |
| 21 | Rotação (no máximo 150 eventos) | Gerar 160 eventos: `for i in $(seq 1 80); do curl -s -X POST "http://$IP/auto?on=0" >/dev/null; curl -s -X POST "http://$IP/auto?on=1" >/dev/null; done` | `curl -s http://$IP/logs \| python -c "import json,sys; l=json.load(sys.stdin)['logs']; print(len(l), l[0]['id'], l[-1]['id'])"` mostra entre 75 e 150 eventos com ids em sequência |
| 22 | ERRO "Reset por brownout/watchdog" | **Não provocar de propósito** (pode danificar o equipamento). Se acontecer sozinho (por exemplo, o servo puxando corrente por um cabo USB fraco), aparece no log | `/logs` |

---

## 3. Perguntas prováveis da banca

**1. Por que LittleFS e não guardar o log na NVS, como as pesagens?**
A NVS foi feita para pares chave/valor pequenos. O log é uma sequência de registros que só cresce, ou seja, um arquivo. O LittleFS tem nivelamento de desgaste (espalha as gravações pela flash) e é resistente a queda de energia: se a energia cair no meio de uma gravação, o arquivo fica como estava antes. Ele usa a partição `spiffs` do esquema padrão, então não é preciso configurar nada na IDE.

**2. Por que registros de tamanho fixo e dois arquivos?**
Com 80 bytes fixos, o registro *i* fica na posição `i × 80`: dá para ler de trás para frente com `seek()` e contar os registros com `tamanho / 80`, sem parser. Com dois arquivos de 75, a rotação é só apagar o antigo e renomear o atual. Nunca é preciso reescrever um arquivo inteiro, o que poupa a flash. O custo é guardar entre 75 e 150 eventos (até 12 KB), em vez de exatamente 150.

**3. Por que não gravar o log direto no callback de evento do Wi-Fi?**
Os callbacks rodam na task de eventos do Wi-Fi, que é diferente da task do `loop()`. Duas tasks mexendo no mesmo arquivo e nas mesmas variáveis ao mesmo tempo podem corromper dados. Por isso o `loop()` compara `WiFi.status()` com o estado da volta anterior. Toda gravação acontece na mesma task: no `setup()`, no `loop()` ou nos handlers HTTP, que rodam dentro de `server.handleClient()`.

**4. Como a reconexão funciona sem travar o sistema?**
O `verificarWiFi()` usa `millis()`. Sem conexão, a cada 15 s ele chama `WiFi.begin()`, que só inicia a conexão e retorna na hora. O `loop()` continua lendo os sensores e cuidando do comedouro. Desligamos a autoreconexão do core para que o intervalo seja controlado pelo nosso código.

**5. O que é o modo AP+STA e qual a limitação?**
O ESP32 é ao mesmo tempo ponto de acesso (rede própria, `192.168.4.1`) e cliente do roteador. Como ele tem um único rádio, AP e STA precisam estar no mesmo canal. Enquanto a STA procura a rede, o rádio troca de canal e os clientes do AP podem perder a conexão por alguns instantes. Quando a STA conecta, desligamos o AP. Se o Wi-Fi cair depois, o AP não é religado.

**6. Como os eventos sem hora ganham data e hora?**
Cada evento grava também `ms` (o `millis()`, tempo desde o boot). Quando a hora é acertada, é gravado o evento "Hora acertada" com `t` e `ms`. Ao servir o `/logs`, para cada evento anterior do mesmo boot o ESP32 calcula `t = t_ref − (ms_ref − ms) / 1000`. Isso só vale dentro do mesmo boot, porque o `millis()` zera a cada reinício. Boots que nunca tiveram hora ficam com `t = 0`.

**7. Por que o `/logs` usa resposta "chunked"?**
Com 150 eventos, o JSON tem mais de 15 KB. Montar tudo numa `String` exigiria um bloco grande e contínuo de RAM, que pode faltar por causa da fragmentação do heap. Com `Transfer-Encoding: chunked` (HTTP/1.1), cada registro é enviado assim que é lido, usando só algumas centenas de bytes.

**8. Por que o MAC muda entre modo AP e STA, e o que é a histerese da temperatura?**
Cada interface de rede do ESP32 tem o seu MAC; o do AP é o da STA mais 1. Por isso o `/dados` informa em `interface` qual delas está sendo mostrada. A histerese usa limites diferentes para sair (passar de 35 °C) e para voltar (ficar abaixo de 34,5 °C). Sem ela, uma leitura oscilando em cima do limite geraria um evento a cada leitura.
