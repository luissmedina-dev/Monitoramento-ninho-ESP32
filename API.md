# API HTTP do Ninho ESP32

O ESP32 roda um servidor HTTP na **porta 80**. Todas as respostas de dados são JSON (`application/json`), exceto a página (`/`) e a planilha (`/pesagens.csv`).

## Endereço do ESP32

| Situação | Endereço |
|---|---|
| Conectado ao Wi-Fi (interface STA) | IP dado pelo roteador (aparece no Monitor Serial e no campo `ip` de `/dados`), ou `http://ninho.local` |
| Wi-Fi não conectou no boot (interface AP) | Conecte na rede `Ninho-ESP32` (senha `ninho1234`) e use `http://192.168.4.1` |

Nos exemplos abaixo, troque `IP` pelo endereço do ESP32. No Windows PowerShell, use `curl.exe` em vez de `curl`, porque no PowerShell 5.1 `curl` é um apelido de outro comando.

```sh
IP=192.168.0.23
```

## Comportamento do Wi-Fi

1. No boot, o ESP32 tenta a rede configurada por até 20 s.
2. Se não conseguir, entra em **modo AP+STA**: cria a rede `Ninho-ESP32` (AP) e continua tentando a rede configurada (STA) a cada 15 s. Fica registrado um **AVISO** "Entrou em modo AP".
3. Quando a STA conecta, registra **INFO** "Wi-Fi conectado (IP ...)" e **desliga o AP**. Quem estava na rede `Ninho-ESP32` precisa voltar para a rede do roteador e usar o novo IP.
4. Se a conexão cair depois, registra **AVISO** "Wi-Fi perdido" e tenta reconectar **só a STA** a cada 15 s, **sem religar o AP**. Enquanto o Wi-Fi não voltar, o ESP32 fica inacessível pela rede, mas continua lendo os sensores, cuidando do comedouro e gravando o log.
5. Quando volta, registra **INFO** "Wi-Fi reconectado após N s fora (IP ...)" e soma 1 em `reconexoes`.

A queda só é percebida quando o ESP32 deixa de receber o roteador, o que leva alguns segundos (normalmente de 5 a 10 s) depois de o roteador ser desligado.

---

## Endpoints de leitura (GET)

### `GET /`
Página web do ninho (HTML).

```sh
curl http://$IP/
```

### `GET /dados`
Leituras atuais, estado do comedouro e da rede. A página consulta este endpoint a cada 2 s.

```sh
curl http://$IP/dados
```

| Campo | Tipo | Descrição |
|---|---|---|
| `temp` | número ou `null` | Temperatura do ninho (°C); `null` = DHT22 sem leitura |
| `umid` | número ou `null` | Umidade (%) |
| `temp_min`, `temp_max` | número | Faixa ideal de temperatura (°C) |
| `peso` | número ou `null` | Peso atual na balança (g) |
| `balanca_ok` | bool | HX711 respondendo |
| `dist` | número ou `null` | Distância do sensor até a comida (cm); `null` = HC-SR04 sem leitura |
| `dist_cheio`, `dist_vazio` | número | Distâncias de prato cheio e de "pouca comida" (cm) |
| `vazio` | bool | Pouca comida no prato |
| `auto` | bool | Alimentação automática ligada |
| `aberta` | bool | Comporta aberta neste momento |
| `liberacoes` | inteiro | Liberações de comida desde o boot |
| `ultima_lib` | inteiro | Hora da última liberação (epoch, s); 0 = sem hora |
| `ultima_lib_ha_s` | inteiro | Segundos desde a última liberação; -1 = nenhuma |
| `origem` | texto | Origem da última liberação (`manual` ou `automática`) |
| `espera_s` | inteiro | Segundos até a próxima liberação automática ser permitida |
| `pesagens_ver` | inteiro | Muda sempre que as pesagens mudam |
| `modo_ap` | bool | `true` = o ESP32 está com a rede própria (AP) ligada |
| `rssi` | inteiro | Sinal do Wi-Fi (dBm); 0 em modo AP |
| `hora_ok` | bool | O relógio do ESP32 está acertado |
| `interface` | texto | Interface que `ip`, `mac` e `ssid` descrevem: `"AP"` (rede própria) ou `"STA"` (cliente do roteador) |
| `ip` | texto | IP dessa interface. Em STA desconectada, vale `"0.0.0.0"` |
| `mac` | texto | MAC dessa interface (o MAC do AP é diferente do MAC da STA) |
| `ssid` | texto | Em AP, o nome da rede própria; em STA, o nome da rede configurada |
| `porta` | inteiro | Porta do servidor HTTP (80) |
| `conectado` | bool | A STA está conectada ao roteador |
| `uptime_s` | inteiro | Segundos desde que o ESP32 ligou |
| `reconexoes` | inteiro | Quantas vezes o Wi-Fi voltou depois de cair, desde o boot |

Exemplo (trecho com os campos de rede):

```json
{ "...": "...", "modo_ap": false, "rssi": -58, "hora_ok": true,
  "interface": "STA", "ip": "192.168.0.23", "mac": "24:6F:28:AA:BB:CC",
  "ssid": "MinhaRede", "porta": 80, "conectado": true, "uptime_s": 3725, "reconexoes": 1 }
```

### `GET /logs`
Log do sistema gravado na memória flash (LittleFS). Guarda os últimos 75 a 150 eventos e **sobrevive ao desligamento**. Vem do **mais recente para o mais antigo**.

```sh
curl http://$IP/logs
```

```json
{ "boot_atual": 14,
  "logs": [
    { "id": 231, "boot": 14, "t": 1761740000, "ms": 2140, "nivel": "AVISO", "msg": "Wi-Fi perdido" }
  ] }
```

| Campo | Descrição |
|---|---|
| `boot_atual` | Número da inicialização atual (contador guardado na NVS) |
| `id` | Número sequencial do evento (continua entre boots) |
| `boot` | Em qual inicialização o evento aconteceu |
| `t` | Data/hora do evento (epoch em segundos, UTC). 0 = o ESP32 não tinha hora naquele boot |
| `ms` | `millis()` no momento do evento (milissegundos desde aquele boot) |
| `nivel` | `INFO`, `AVISO` ou `ERRO` |
| `msg` | Descrição (até 62 bytes) |

**Eventos sem hora.** Logo depois do boot, o ESP32 ainda não sabe a hora. Esses eventos são gravados com `t = 0`. Quando a hora é acertada (por NTP ou pelo navegador), é gravado o evento "Hora acertada". Ao responder o `/logs`, o ESP32 calcula o `t` dos eventos anteriores **do mesmo boot** a partir desse evento: `t = t_ref − (ms_ref − ms) / 1000`. O arquivo não é regravado. Em boots que nunca tiveram hora, `t` continua 0.

Eventos registrados:

| Nível | Mensagem |
|---|---|
| INFO | `ESP32 iniciado (<motivo do reset>)` |
| INFO | `Wi-Fi conectado (IP <ip>)` |
| INFO | `Wi-Fi reconectado após <N> s fora (IP <ip>)` |
| INFO | `Hora acertada (NTP)` / `Hora acertada (navegador)` |
| INFO | `Comida liberada (manual)` / `Comida liberada (automática)` |
| INFO | `Pesagem manual registrada: <g> g` / `Pesagem automática registrada: <g> g` |
| INFO | `Balança tarada` / `Balança tarada automaticamente (primeira vez)` / `Balança calibrada (fator <f>)` |
| INFO | `Alimentação automática ligada` / `Alimentação automática desligada` |
| INFO | `DHT22 voltou a responder` / `HX711 voltou a responder` / `HC-SR04 voltou a responder` |
| INFO | `Temperatura voltou à faixa: <t> °C` |
| AVISO | `Wi-Fi perdido` |
| AVISO | `Entrou em modo AP (rede Ninho-ESP32)` |
| AVISO | `Temperatura fora da faixa: <t> °C` |
| AVISO | `Pesagens apagadas (<n>)` |
| ERRO | `DHT22 sem leitura` / `HX711 sem leitura` / `HC-SR04 sem leitura` |
| ERRO | `Reset por watchdog` / `Reset por queda de tensão (brownout)` / `Reset por travamento (panic)` |

Os eventos de sensor só são gravados quando o estado muda e continua igual por 5 s. A temperatura tem histerese de 0,5 °C: sai da faixa ao passar do limite, mas só volta quando fica 0,5 °C para dentro dele.

### `GET /historico`
Temperatura e umidade das últimas 6 horas (1 ponto por minuto, só na RAM).

```sh
curl http://$IP/historico
```

```json
{ "agora": 3725000, "pontos": [[3660000, 28.4, 55], [3720000, 28.5, 55]] }
```
`agora` é o `millis()` atual. Cada ponto é `[millis, temperatura °C, umidade %]`.

### `GET /pesagens`
Pesagens guardadas na NVS (até 120).

```sh
curl http://$IP/pesagens
```

```json
{ "pontos": [[1761740000, 52.3], [1761826400, 55.1]] }
```
Cada ponto é `[epoch em s (0 = sem hora), gramas]`.

### `GET /pesagens.csv`
As mesmas pesagens em planilha (separador `;` e vírgula decimal, formato do Excel brasileiro).

```sh
curl -o pesagens.csv http://$IP/pesagens.csv
```

### `GET /hora?ts=<epoch>`
Acerta o relógio do ESP32 com a hora do navegador, mas só se ele ainda não tiver hora válida. A página chama isso sozinha ao abrir.

```sh
curl "http://$IP/hora?ts=$(date +%s)"
```

```json
{ "ok": true }
```
`ok` = o relógio está acertado depois da chamada.

---

## Endpoints de ação (POST)

Todos respondem `{"ok": true|false, "msg": "<texto>"}`, com HTTP 200 quando deu certo e 400 quando não deu.

### `POST /liberar`
Abre a comporta e libera comida agora.

```sh
curl -X POST http://$IP/liberar
```

### `POST /auto?on=<0|1>`
Liga (`1`) ou desliga (`0`) a alimentação automática. A configuração fica guardada na NVS.

```sh
curl -X POST "http://$IP/auto?on=0"
curl -X POST "http://$IP/auto?on=1"
```

### `POST /tarar`
Zera a balança, que deve estar vazia.

```sh
curl -X POST http://$IP/tarar
```

### `POST /calibrar?g=<gramas>`
Calibra a balança com um objeto de peso conhecido em cima dela.

```sh
curl -X POST "http://$IP/calibrar?g=100"
```

### `POST /pesar`
Registra uma pesagem agora (o peso precisa ser de pelo menos 5 g).

```sh
curl -X POST http://$IP/pesar
```

### `POST /limpar`
Apaga todas as pesagens guardadas.

```sh
curl -X POST http://$IP/limpar
```
