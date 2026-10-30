"""
MOCK DO ESP32 — para desenvolver a página sem a placa.

Serve a MESMA página que está dentro do ninho_esp32.ino (lida direto do
arquivo, então não existe cópia para ficar desatualizada) e responde aos
mesmos endpoints do API.md com dados falsos que mudam com o tempo.

Uso (precisa só de Python 3, sem instalar nada):
    python mock_esp32.py                      -> Wi-Fi conectado, tudo ok
    python mock_esp32.py --cenario ap         -> ESP32 em modo AP
    python mock_esp32.py --cenario falhas     -> sensores falhando às vezes
    python mock_esp32.py --ino ../ninho_esp32.ino --porta 8000

Depois abra http://localhost:8000 e recarregue a página (F5) a cada
mudança que fizer no HTML/CSS/JS do .ino.

ATENÇÃO: isto NÃO substitui o teste na placa. Os dados são inventados.
"""
import argparse, json, math, os, random, re, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

INICIO = time.time()
AGORA = lambda: int(time.time())
MILLIS = lambda: int((time.time() - INICIO) * 1000)

# ---------------------------------------------------------------- estado
estado = {
    "auto": True, "liberacoes": 0, "ultima_lib": 0, "t_ultima_lib": None,
    "origem": "", "pesagens_ver": 0, "dist_base": 4.0, "reconexoes": 1,
}
pesagens = [[AGORA() - d * 86400, 40 + (7 - d) * 3.2] for d in range(7, 0, -1)]
BOOT = 14
logs = []  # do mais antigo para o mais novo; /logs devolve invertido


def log(nivel, msg, t=None, boot=BOOT, ms=None):
    logs.append({"id": len(logs) + 1, "boot": boot,
                 "t": AGORA() if t is None else t,
                 "ms": MILLIS() if ms is None else ms,
                 "nivel": nivel, "msg": msg})


def logs_iniciais(cenario):
    ontem = AGORA() - 86400
    # boot 13: nunca teve hora (t = 0) -> testa como a tela mostra isso
    log("INFO", "ESP32 iniciado (energia)", t=0, boot=13, ms=120)
    log("AVISO", "Entrou em modo AP (rede Ninho-ESP32)", t=0, boot=13, ms=20500)
    log("ERRO", "Reset por queda de tensão (brownout)", t=0, boot=13, ms=95000)
    # boot 14: o atual
    log("INFO", "ESP32 iniciado (brownout)", t=ontem, ms=150)
    log("INFO", "Wi-Fi conectado (IP 192.168.0.23)", t=ontem + 4, ms=4100)
    log("INFO", "Hora acertada (NTP)", t=ontem + 6, ms=6200)
    log("AVISO", "Wi-Fi perdido", t=ontem + 3600)
    log("INFO", "Wi-Fi reconectado após 42 s fora (IP 192.168.0.23)", t=ontem + 3642)
    log("AVISO", "Temperatura fora da faixa: 35,6 °C", t=ontem + 7200)
    log("INFO", "Temperatura voltou à faixa: 34,4 °C", t=ontem + 9000)
    log("INFO", "Comida liberada (automática)", t=ontem + 20000)
    log("INFO", "Pesagem automática registrada: 62,3 g", t=ontem + 30000)
    if cenario == "falhas":
        log("ERRO", "HX711 sem leitura", t=AGORA() - 600)
        log("ERRO", "DHT22 sem leitura", t=AGORA() - 300)


# ---------------------------------------------------------------- leituras falsas
def leituras(cenario):
    s = time.time() - INICIO
    temp = round(30 + 3 * math.sin(s / 60), 1)          # oscila devagar
    peso = round(62 + random.uniform(-0.4, 0.4), 1) if int(s / 20) % 2 else 0.0
    dist = min(13.0, estado["dist_base"] + s / 40 % 10)  # prato esvaziando
    balanca_ok = True
    if cenario == "falhas":
        if int(s / 15) % 3 == 0: temp = None             # DHT some de vez em quando
        if int(s / 25) % 2 == 0: balanca_ok, peso = False, None
    return temp, peso, round(dist, 1), balanca_ok


def dados(cenario):
    temp, peso, dist, balanca_ok = leituras(cenario)
    ap = cenario == "ap"
    espera = 0
    if estado["t_ultima_lib"]:
        espera = max(0, 300 - int(time.time() - estado["t_ultima_lib"]))
    return {
        "temp": temp, "umid": None if temp is None else 55,
        "temp_min": 25.0, "temp_max": 35.0,
        "peso": peso, "balanca_ok": balanca_ok,
        "dist": dist, "dist_cheio": 4.0, "dist_vazio": 12.0, "vazio": dist >= 12.0,
        "auto": estado["auto"], "aberta": False,
        "liberacoes": estado["liberacoes"], "ultima_lib": estado["ultima_lib"],
        "ultima_lib_ha_s": int(time.time() - estado["t_ultima_lib"]) if estado["t_ultima_lib"] else -1,
        "origem": estado["origem"], "espera_s": espera,
        "pesagens_ver": estado["pesagens_ver"],
        "modo_ap": ap, "rssi": 0 if ap else random.randint(-72, -50), "hora_ok": True,
        "interface": "AP" if ap else "STA",
        "ip": "192.168.4.1" if ap else "192.168.0.23",
        "mac": "24:6F:28:AA:BB:CD" if ap else "24:6F:28:AA:BB:CC",
        "ssid": "Ninho-ESP32" if ap else "MinhaRede",
        "porta": 80, "conectado": not ap,
        "uptime_s": int(time.time() - INICIO), "reconexoes": estado["reconexoes"],
    }


def historico():
    agora = MILLIS() + 3_600_000   # finge que o ESP está ligado há mais de 1 h
    pts = [[agora - i * 60_000, round(30 + 3 * math.sin(i / 9), 1), 55] for i in range(60, -1, -1)]
    return {"agora": agora, "pontos": pts}


# ---------------------------------------------------------------- ações (POST)
def acao(caminho, q):
    if caminho == "/liberar":
        estado.update(liberacoes=estado["liberacoes"] + 1, ultima_lib=AGORA(),
                      t_ultima_lib=time.time(), origem="manual", dist_base=4.0)
        log("INFO", "Comida liberada (manual)")
        return True, "Comida liberada"
    if caminho == "/auto":
        estado["auto"] = q.get("on", ["0"])[0] == "1"
        log("INFO", "Alimentação automática " + ("ligada" if estado["auto"] else "desligada"))
        return True, "Alimentação automática " + ("ligada" if estado["auto"] else "desligada")
    if caminho == "/pesar":
        pesagens.append([AGORA(), 62.3]); estado["pesagens_ver"] += 1
        log("INFO", "Pesagem manual registrada: 62,3 g")
        return True, "Pesagem registrada: 62,3 g"
    if caminho == "/tarar":
        log("INFO", "Balança tarada"); return True, "Balança zerada"
    if caminho == "/calibrar":
        log("INFO", "Balança calibrada (fator 421,7)"); return True, "Balança calibrada (fator 421,7)"
    if caminho == "/limpar":
        log("AVISO", f"Pesagens apagadas ({len(pesagens)})")
        pesagens.clear(); estado["pesagens_ver"] += 1
        return True, "Pesagens apagadas"
    return None, None


# ---------------------------------------------------------------- servidor
class Handler(BaseHTTPRequestHandler):
    def _json(self, obj, status=200):
        corpo = json.dumps(obj, ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(corpo)))
        self.end_headers()
        self.wfile.write(corpo)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/":
            corpo = ler_pagina(self.server.ino).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers(); self.wfile.write(corpo)
        elif u.path == "/dados":      self._json(dados(self.server.cenario))
        elif u.path == "/logs":       self._json({"boot_atual": BOOT, "logs": logs[::-1]})
        elif u.path == "/historico":  self._json(historico())
        elif u.path == "/pesagens":   self._json({"pontos": pesagens})
        elif u.path == "/hora":       self._json({"ok": True})
        else:
            self.send_response(404); self.end_headers(); self.wfile.write(b"Pagina nao encontrada")

    def do_POST(self):
        u = urlparse(self.path)
        ok, msg = acao(u.path, parse_qs(u.query))
        if ok is None:
            self.send_response(404); self.end_headers(); return
        self._json({"ok": ok, "msg": msg}, 200 if ok else 400)

    def log_message(self, *a):  # silencia o log de cada requisição no terminal
        pass


def ler_pagina(caminho_ino):
    with open(caminho_ino, encoding="utf-8") as f:
        texto = f.read()
    m = re.search(r'R"rawliteral\((.*?)\)rawliteral"', texto, re.S)
    if not m:
        return "<h1>Não achei a PAGINA (rawliteral) no .ino</h1>"
    return m.group(1)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--ino", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ninho_esp32.ino"))
    p.add_argument("--porta", type=int, default=8000)
    p.add_argument("--cenario", choices=["normal", "ap", "falhas"], default="normal")
    a = p.parse_args()
    logs_iniciais(a.cenario)
    srv = ThreadingHTTPServer(("127.0.0.1", a.porta), Handler)
    srv.ino, srv.cenario = os.path.abspath(a.ino), a.cenario
    print(f"Mock do ESP32 ({a.cenario}) lendo a página de: {srv.ino}")
    print(f"Abra http://localhost:{a.porta}  (Ctrl+C para parar)")
    srv.serve_forever()
