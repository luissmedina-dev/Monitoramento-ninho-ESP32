#pragma once
#include <Arduino.h>

// Página web servida em GET / (fica guardada na memória flash do ESP32).

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
--marca-txt:#9a4a1f;--marca-bg:#f8ebe2;--neutro-bg:#efe9e3;--info-txt:#5a4f48;--ok-bg:#e5f1e9}
@media (prefers-color-scheme:dark){:root{color-scheme:dark;--fundo:#1b1714;--cartao:#26201c;--texto:#f1e9e2;--suave:#a89c93;--suave2:#a89c93;--linha:#3a312b;--marca:#e0874f;
--ok:#7fc79a;--ok-txt:#7fc79a;--alerta:#f08a72;--alerta-txt:#f08a72;--alerta-bg:#3a2620;--frio:#8ab4e8;--frio-txt:#8ab4e8;--frio-bg:#1d2f45;
--marca-txt:#e0874f;--marca-bg:#3a2a20;--neutro-bg:#322a25;--info-txt:#c9bdb4;--ok-bg:#1f3327}}
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
.caiu{color:var(--alerta-txt);border-color:var(--alerta)}.caiu .ponto{background:var(--alerta)}
.pisca{animation:pisca .7s ease-out}
@keyframes pisca{from{box-shadow:0 0 0 0 var(--ok)}to{box-shadow:0 0 0 7px transparent}}
.abas{order:3;flex:1 1 100%;display:grid;grid-template-columns:1fr 1fr auto;gap:4px;padding:4px;background:var(--linha);border-radius:12px}
.abas button{min-height:44px;border:0;border-radius:9px;background:transparent;color:var(--texto);font:inherit;font-size:14px;font-weight:600;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:8px}
.abas button[aria-selected=true]{background:var(--cartao);font-weight:700}
/* Técnico: rótulo discreto, para o dono do ninho não achar que precisa abrir */
.abas .discreta{padding:0 12px;font-size:12px;font-weight:500;color:var(--suave2)}
.abas .discreta[aria-selected=true]{color:var(--texto);font-weight:650}
.cont{min-width:20px;height:20px;padding:0 6px;border-radius:99px;background:var(--botao);color:#fff;font-size:11px;font-weight:700;display:inline-flex;align-items:center;justify-content:center}
.painel{max-width:980px;margin:0 auto;padding:12px 16px 28px;display:grid;gap:12px}
.painel>*{min-width:0}
/* No celular a ordem dos cartões segue o design; no desktop vale a ordem do HTML */
@media (max-width:759px){.o0{order:0}.o1{order:1}.o2{order:2}.o3{order:3}.o4{order:4}.o5{order:5}.o6{order:6}.o7{order:7}}
.so-desk{display:none}
@media (min-width:760px){
 .abas{order:0;flex:0 0 auto;grid-template-columns:auto auto auto}.abas button{padding:0 22px}.abas .discreta{padding:0 14px}
 .painel{grid-template-columns:repeat(12,minmax(0,1fr));gap:16px}
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
.chave input[type=checkbox]{width:20px;height:20px;accent-color:var(--botao)}
#avisos:empty{display:none}
.av{display:flex;gap:10px;align-items:flex-start;padding:12px 14px;border-radius:12px;font-size:12px;margin-bottom:12px}
.av b{display:block;font-size:14px;margin-bottom:2px}.av svg{flex:none}
.av.q{background:var(--alerta-bg)}.av.f{background:var(--frio-bg)}.av.m{background:var(--marca-bg);color:var(--marca-txt)}
.av.perdido{background:#c4412a;color:#fff;flex-wrap:wrap}.av.perdido .btn{border:1.5px solid #fff;background:transparent;color:#fff}
.parado .painel .cartao{opacity:.6}
.av.ok{background:var(--ok-bg);color:var(--ok-txt)}.av.ok b{color:var(--texto)}
.duas{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.linha-tec{margin:0;font-size:13px;color:var(--suave2)}
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
#historico{grid-template-columns:minmax(0,1fr);max-width:760px}
/* Selos do histórico: ERRO = "Precisa de atenção", AVISO = "Aviso", INFO sem selo */
.selo{align-self:flex-start;padding:2px 8px;border-radius:5px;font-size:11px;font-weight:700}
.selo.atencao{background:var(--alerta);color:var(--cartao)}.selo.aviso{padding:1px 7px;border:1.5px solid var(--marca);color:var(--marca-txt)}
.hora{font-size:12px;color:var(--suave);min-width:42px}
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
  <h1>Ninho do filhote</h1>
  <div class="abas" role="tablist" aria-label="Seções">
    <button role="tab" id="abaNinho" aria-selected="true" aria-controls="ninho" onclick="mostrar('ninho')">Ninho</button>
    <button role="tab" id="abaHist" aria-selected="false" aria-controls="historico" onclick="mostrar('historico')">Histórico <span class="cont" id="naoVistos" hidden></span></button>
    <button role="tab" id="abaTec" class="discreta" aria-selected="false" aria-controls="tecnico" onclick="mostrar('tecnico')">Técnico</button>
  </div>
  <div class="vivo" id="vivo" role="status"><span class="ponto" id="ponto"></span><span id="vivoTxt">Conectando…</span></div>
</div>
</header>

<!-- Visão do cliente: o dono do ninho só quer saber se o filhote está bem.
     Aqui não aparecem termos técnicos (eles ficam na aba Técnico). -->
<main id="ninho" class="painel" role="tabpanel" aria-labelledby="abaNinho">
  <div id="resumo" class="c12 o0"></div>

  <section class="cartao c7 o1">
    <div class="cab">
      <h2 class="rot">Filhote</h2>
      <div class="vistas" role="group" aria-label="Vista do ninho">
        <span class="nota" id="semTermica" hidden>vista térmica indisponível</span>
        <button aria-pressed="true" id="vNormal" onclick="vista(false)">Normal</button>
        <button aria-pressed="false" id="vTermica" onclick="vista(true)">Térmica</button>
      </div>
    </div>
    <div id="ninhoSvg"></div>
    <p class="nota legenda" id="legenda" hidden><span id="corLeg"></span>Uma cor só: a temperatura é medida em um ponto da borda do ninho. Não é mapa de calor.</p>
    <div class="duas" style="border-top:1px solid var(--linha);padding-top:12px">
      <div class="pilha"><span class="rot">Temperatura</span><span class="big" id="temp">—</span><span class="med" id="tempPalavra"></span><span class="nota" id="faixa"></span></div>
      <div class="pilha"><span class="rot">Peso</span><span class="big" id="peso">—</span><span class="med" id="pesoPalavra"></span><span class="nota" id="pesoNota"></span></div>
    </div>
  </section>

  <section class="cartao c5 o2">
    <div class="cab"><h2 class="rot">Comida dos pais</h2><span class="chip marca" id="modo">—</span></div>
    <div class="lado">
      <div class="fig" id="comSvg"></div>
      <div class="pilha" style="gap:12px">
        <div class="pilha"><span class="rot">Prato</span><span class="big" id="nivel">—</span></div>
        <div class="pilha"><span class="rot">Liberação automática</span><span class="med" id="proxima"></span></div>
      </div>
    </div>
    <span class="nota" id="ultima"></span>
    <label class="chave"><input type="checkbox" id="auto" onchange="acao('/auto?on='+(this.checked?1:0))"> Liberar comida automaticamente</label>
    <button class="btn prim" id="btnLiberar" onclick="acao('/liberar')">Liberar comida</button>
  </section>

  <section class="cartao c7 o3">
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
  </section>

  <section class="cartao c5 o4 so-desk">
    <div class="cab"><h2 class="rot">Últimos acontecimentos</h2><button class="lk" onclick="mostrar('historico')">Ver histórico</button></div>
    <div id="ultimos"></div>
  </section>

  <p class="rodape c12 o5" id="rodape"></p>
</main>

<section id="historico" class="painel" role="tabpanel" aria-labelledby="abaHist" hidden>
  <div class="cab-log"><h2>Histórico do ninho</h2></div>
  <div id="listaHist" style="display:grid;gap:14px"><p class="vazio">Carregando o histórico…</p></div>
</section>

<!-- Aba Técnico: requisitos do trabalho (rede, log com níveis, sensores) -->
<section id="tecnico" class="painel" role="tabpanel" aria-labelledby="abaTec" hidden>
  <p class="c12 linha-tec" id="tecTopo">Informações técnicas do sistema (servidor HTTP no ESP32, porta 80)</p>
  <div id="avisos" class="c12"></div>

  <section class="cartao c7">
    <div class="cab"><h2 class="rot">Sensores</h2><span class="estado" id="estTemp"></span></div>
    <div class="tres">
      <div><span class="rot">Temperatura</span><span class="big" id="tecTemp">—</span><span class="nota" id="tecFaixa"></span></div>
      <div><span class="rot">Umidade</span><span class="big" id="umid">—</span><span class="nota">relativa · DHT22</span></div>
      <div><span class="rot">Balança</span><span class="big" id="tecPeso">—</span><span class="nota" id="tecPesoNota"></span></div>
    </div>
    <dl>
      <div><dt class="rot">Distância até a comida</dt><dd id="dist">—</dd></div>
      <div><dt class="rot">Comporta (servo)</dt><dd id="comporta">—</dd></div>
      <div><dt class="rot">Liberações desde o boot</dt><dd id="nLib">—</dd></div>
      <div><dt class="rot">Pesagens guardadas</dt><dd id="nPes">—</dd></div>
    </dl>
    <div class="sep">
      <span class="rot">Ajustes da balança (HX711)</span>
      <p class="nota" style="margin:0">O peso é registrado sozinho quando fica estável. Para calibrar: zere com a balança vazia, coloque um objeto de peso conhecido, informe o peso e toque em Calibrar.</p>
      <div class="botoes">
        <button class="btn" onclick="acao('/pesar')">Registrar pesagem</button>
        <button class="btn" onclick="acao('/tarar')">Zerar balança</button>
      </div>
      <p class="botoes" style="margin:0"><label class="chave"><input type="number" id="massa" value="100" min="1" step="0.1"> g</label><button class="btn" onclick="calibrar()">Calibrar</button></p>
      <div class="botoes"><a class="lk" href="/pesagens.csv">Baixar planilha (CSV)</a><button class="lk" onclick="limpar()">Apagar pesagens</button></div>
    </div>
  </section>

  <section class="cartao c5">
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

  <section class="cartao c12">
    <h2 class="rot" id="tTemp">Temperatura · desde que a página abriu</h2>
    <div id="gTemp"></div>
    <span class="nota" id="notaFaixa"></span>
    <h2 class="rot" id="tUmid">Umidade · desde que a página abriu</h2>
    <div id="gUmid"></div>
  </section>

  <section class="cartao c12" id="log">
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
  <p class="rodape c12" id="rodapeTec"></p>
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
let conectado = false, jaTentou = false, tUltimaResposta = 0;
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
    toast(r.j ? r.j.msg : 'O ninho respondeu com erro ' + r.status);   // inclusive o erro, ex.: "A comporta já está aberta"
  } catch (e) {
    toast('Não consegui falar com o ninho');
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
  conectado = false; jaTentou = true;
  desenharPainel();
}
function receberLogs(j) {
  const antes = new Set(logs.map(e => e.id));
  // Eventos novos (só depois da primeira carga) entram com animação
  novos = logs.length ? new Set(j.logs.filter(e => !antes.has(e.id)).map(e => e.id)) : new Set();
  const mudou = j.logs.length !== logs.length || (j.logs[0] && j.logs[0].id) !== (logs[0] && logs[0].id);
  logs = j.logs; bootAtual = j.boot_atual;
  if (!$('historico').hidden) marcarVistos();
  contarNaoVistos();
  if (mudou) { desenharLog(); desenharHistorico(); desenharUltimos(); }
  novos = new Set();
}

// ---------------------------------------------------------------- painel
function desenharPainel() {
  document.body.classList.toggle('parado', !conectado);
  desenharCabecalho();
  desenharResumo();
  desenharAvisos();
  desenharRede();
  const btn = $('btnLiberar');      // só a falta de conexão bloqueia a liberação manual
  btn.disabled = !conectado;
  btn.textContent = conectado ? 'Liberar comida' : 'Liberar comida · sem conexão';
  const d = dados;
  if (!d) { $('rodape').textContent = 'Aguardando a primeira leitura do ninho…'; return; }
  const quando = (conectado ? 'Atualizado às ' : 'Última leitura às ') + hms(new Date(tUltimaResposta));
  $('rodape').textContent = quando;
  $('rodapeTec').textContent = quando + (bootAtual ? ' · boot ' + bootAtual : '');
  $('tecTopo').textContent = 'Informações técnicas do sistema (servidor HTTP no ESP32, porta ' + d.porta + ')';

  // Filhote: temperatura com uma palavra; o peso vem da última pesagem
  const quente = d.temp != null && d.temp > d.temp_max, frio = d.temp != null && d.temp < d.temp_min;
  const tc = quente ? ' q' : frio ? ' f' : '';
  const tTxt = d.temp == null ? '—' : fmt(d.temp, 1) + ' <small>°C</small>';
  $('temp').innerHTML = tTxt; $('temp').className = 'big' + tc;
  const palavra = d.temp == null ? ['q', 'sensor sem resposta'] : quente ? ['q', 'quente demais'] :
                  frio ? ['f', 'frio demais'] : ['b', 'boa'];
  $('tempPalavra').className = 'med ' + palavra[0];
  $('tempPalavra').textContent = palavra[1];
  $('faixa').textContent = 'ideal entre ' + fmt(d.temp_min, 0) + ' e ' + fmt(d.temp_max, 0) + ' °C';
  desenharPesoFilhote();

  // Comida: nível = onde a distância medida fica entre "prato cheio" e "prato vazio"
  let nivel = null;
  if (d.dist != null) nivel = Math.min(1, Math.max(0, (d.dist_vazio - d.dist) / (d.dist_vazio - d.dist_cheio)));
  $('modo').textContent = d.auto ? 'Automático' : 'Manual';
  $('nivel').textContent = palavraPrato(d, nivel);
  $('nivel').className = 'big' + (d.vazio || nivel == null ? ' q' : '');
  // espera_s = quanto falta para a próxima liberação AUTOMÁTICA ser permitida.
  // Não bloqueia a liberação manual.
  $('proxima').textContent = !d.auto ? 'desligada' :
    d.espera_s > 0 ? 'próxima em ' + duracao(d.espera_s) : 'quando o prato esvaziar';
  $('ultima').textContent = d.liberacoes > 0 ?
    'Última liberação há ' + duracao(d.ultima_lib_ha_s) + (d.origem === 'manual' ? ' (por você)' : ' (automática)') :
    'Nenhuma liberação desde que o ninho ligou';
  if (document.activeElement !== $('auto')) $('auto').checked = d.auto;
  desenharNinho();
  desenharComedouro(nivel);
  if ((d.eclosao || 0) !== eclAntes) desenharPeso();   // a data de eclosão mudou

  // Técnico: sensores com os valores crus
  $('tecTemp').innerHTML = tTxt; $('tecTemp').className = 'big' + tc;
  $('tecFaixa').textContent = 'ideal ' + fmt(d.temp_min, 0) + '–' + fmt(d.temp_max, 0) + ' °C';
  $('umid').innerHTML = d.umid == null ? '—' : fmt(d.umid, 0) + ' <small>%</small>';
  const pesando = d.balanca_ok && d.peso != null && d.peso >= 5;
  $('tecPeso').innerHTML = d.balanca_ok && d.peso != null ? fmt(d.peso, 1) + ' <small>g</small>' : '—';
  $('tecPesoNota').textContent = !d.balanca_ok ? 'HX711 sem leitura' : pesando ? 'filhote na balança' : 'balança vazia';
  $('tecPesoNota').className = 'nota' + (d.balanca_ok ? '' : ' q');
  const est = d.temp == null ? ['q', 'DHT22 sem leitura'] : quente ? ['q', 'Acima da faixa ideal'] :
              frio ? ['f', 'Abaixo da faixa ideal'] : ['b', 'Dentro da faixa ideal'];
  $('estTemp').className = 'estado ' + est[0];
  $('estTemp').innerHTML = '<span class="ponto"></span>' + est[1];
  $('dist').textContent = d.dist == null ? 'HC-SR04 sem leitura' :
    fmt(d.dist, 1) + ' cm · ' + (nivel == null ? '' : Math.round(nivel * 100) + ' % (cheio ' + fmt(d.dist_cheio, 0) + ' cm, vazio ' + fmt(d.dist_vazio, 0) + ' cm)');
  $('dist').className = d.dist == null ? 'q' : '';
  $('comporta').textContent = d.aberta ? 'Aberta' : 'Fechada';
  $('nLib').textContent = d.liberacoes + (d.liberacoes > 0 ? ' · última há ' + duracao(d.ultima_lib_ha_s) + ' (' + d.origem + ')' : '');
  $('nPes').textContent = pesagens.length + ' de 120';

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
}

// Cabeçalho: só "Online" ou "Sem conexão com o ninho" (a latência fica no Técnico)
function desenharCabecalho() {
  const caiu = !conectado && jaTentou;
  $('vivo').classList.toggle('on', conectado);
  $('vivo').classList.toggle('caiu', caiu);
  $('vivoTxt').textContent = conectado ? 'Online' : !caiu ? 'Conectando…' :
    'Sem conexão com o ninho' + (tUltimaResposta ? ' · última leitura às ' + hms(new Date(tUltimaResposta)).slice(0, 5) : '');
}

function palavraPrato(d, nivel) {
  if (nivel == null) return 'sem leitura';
  return d.vazio ? 'vazio' : nivel < 0.35 ? 'acabando' : nivel < 0.75 ? 'pela metade' : 'cheio';
}

// Peso do cartão "Filhote": a última pesagem registrada, comparada com a
// faixa de referência quando há data de eclosão (a mesma classificar() do gráfico)
function desenharPesoFilhote() {
  const d = dados, ecl = (d && d.eclosao) || 0;
  const comData = pesagens.filter(p => p[0] > 0);
  const u = comData[comData.length - 1] || pesagens[pesagens.length - 1];
  let palavra = '', cls = '', nota = 'nenhuma pesagem ainda';
  if (u) {
    if (ecl && u[0] > 0 && u[0] >= ecl) {
      const idade = (u[0] - ecl) / 86400, dias = Math.floor(idade), c = classificar(idade, u[1]);
      const pd = dias === 1 ? '1 dia' : dias + ' dias';
      palavra = { ok: 'dentro do esperado', abaixo: 'abaixo do esperado', acima: 'acima do esperado', sem: 'ainda sem referência' }[c] + ' para ' + pd;
      cls = c === 'abaixo' ? 'q' : c === 'ok' ? 'b' : '';
    }
    nota = u[0] ? 'pesado em ' + ddmm(new Date(u[0] * 1000)) + ' às ' + hms(new Date(u[0] * 1000)).slice(0, 5) : 'última pesagem, sem data';
    if (!ecl) nota += ' · informe a data de eclosão para comparar com o esperado';
  }
  if (d && !d.balanca_ok) { nota = 'balança sem resposta' + (u ? ' · ' + nota : ''); cls = cls || 'q'; }
  $('peso').innerHTML = u ? fmt(u[1], 1) + ' <small>g</small>' : '—';
  $('pesoPalavra').textContent = palavra;
  $('pesoPalavra').className = 'med ' + cls;
  $('pesoNota').textContent = nota;
  $('pesoNota').className = 'nota' + (d && !d.balanca_ok ? ' q' : '');
}

// Resumo do topo da aba Ninho: "Tudo certo" ou a lista do que precisa de
// atenção. Só reescreve quando o texto muda (leitor de tela).
let resumoAntes = '';
function desenharResumo() {
  const d = dados, itens = [];
  if (!conectado && jaTentou) {
    itens.push(faixa('perdido', 'semWifi', 'Sem conexão com o ninho',
      (tUltimaResposta ? 'Última leitura às ' + hms(new Date(tUltimaResposta)).slice(0, 5) + '. ' : '') +
      'Pode ser falta de energia, o Wi-Fi de casa fora do ar ou este aparelho longe da rede. Tentando de novo sozinho.',
      '<button class="btn" onclick="ciclo()">Tentar agora</button>'));
  } else if (d) {
    const ideal = 'O ideal é entre ' + fmt(d.temp_min, 0) + ' e ' + fmt(d.temp_max, 0) + ' °C.';
    if (d.temp == null) itens.push(faixa('m', 'tri', 'Sensor de temperatura sem resposta', 'Não dá para saber a temperatura do ninho agora.'));
    else if (d.temp > d.temp_max) itens.push(faixa('q', 'term', 'Ninho quente demais: ' + fmt(d.temp, 1) + ' °C', ideal));
    else if (d.temp < d.temp_min) itens.push(faixa('f', 'term', 'Ninho frio demais: ' + fmt(d.temp, 1) + ' °C', ideal));
    if (d.vazio) itens.push(faixa('q', 'tri', 'O prato de comida está vazio',
      d.auto ? 'A liberação automática repõe assim que puder. Você também pode liberar agora.' : 'Toque em “Liberar comida” para repor.'));
    if (d.dist == null) itens.push(faixa('m', 'tri', 'Sensor de comida sem resposta', 'Não dá para saber quanta comida tem no prato.'));
    if (!d.balanca_ok) itens.push(faixa('m', 'tri', 'Balança sem resposta', 'O filhote não está sendo pesado.'));
    if (d.modo_ap) itens.push(faixa('f', 'wifi', 'O ninho não está no Wi-Fi de casa', 'Ele continua cuidando do filhote e tenta se conectar de novo sozinho.'));
  }
  let h;
  if (itens.length) h = (conectado ? '<h2 class="rot" style="margin:0 0 8px">Precisa de atenção</h2>' : '') + itens.join('');
  else if (d) h = faixa('ok', 'ok', 'Tudo certo com o ninho', 'Temperatura boa, comida no prato e todos os sensores respondendo.');
  else h = '<div class="av neutro" role="status"><div><b>Conectando ao ninho…</b></div></div>';
  if (h !== resumoAntes) { $('resumo').innerHTML = h; resumoAntes = h; }
}

// Faixas de aviso no topo do painel. Só reescreve quando o texto muda,
// para o leitor de tela não repetir o aviso a cada 2 s.
const ICONES = {
  term: 'M14 14.8V5a2 2 0 1 0-4 0v9.8a4 4 0 1 0 4 0z',
  tri: 'M12 3 2 20h20L12 3zM12 10v4M12 17v.5',
  wifi: 'M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0M12 19.5v.5',
  semWifi: 'M5 12.5a10 10 0 0 1 14 0M8.5 16a5 5 0 0 1 7 0M12 19.5v.5M4 4l16 16',
  ok: 'M5 12.5l4.5 4.5L19 7'
};
let avisosAntes = '';
function faixa(classe, icone, titulo, texto, extra) {
  return '<div class="av ' + classe + '" role="' + (classe === 'm' || classe === 'f' || classe === 'ok' ? 'status' : 'alert') + '">' +
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
    '<text class="il-t" x="170" y="208" text-anchor="middle">balança</text>' +
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
    '<text x="14" y="56" font-size="11" font-weight="600" style="fill:' + sens + '">' + (semT ? 'sensor sem resposta' : 'sensor de temperatura') + '</text>';
  // Etiqueta do peso só na vista normal: na térmica ela ficaria embaixo da
  // seta da régua (o peso continua nos números logo abaixo do desenho)
  if (!term && !d.balanca_ok)
    s += '<rect x="250" y="26" width="92" height="30" rx="9" stroke-dasharray="4 3" style="fill:var(--cartao);stroke:var(--alerta);stroke-width:1.5"/>' +
         '<text x="296" y="45" text-anchor="middle" font-size="11" font-weight="600" style="fill:var(--alerta-txt)">balança sem resposta</text>';
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
    (sem ? 'sensor de comida sem resposta' : 'prato ' + palavraPrato(d, nivel)) + ', comporta ' + (d.aberta ? 'aberta' : 'fechada') + '">' +
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
    // HC-SR04 em cima do prato, medindo a distância até a comida (os cm ficam no Técnico)
    '<text class="il-t" x="150" y="127" text-anchor="middle" font-size="9">sensor</text>' +
    '<rect x="128" y="132" width="44" height="14" rx="3" style="fill:var(--frio)"/>' +
    '<circle cx="139" cy="139" r="5" fill="#dfe5ea"/><circle cx="161" cy="139" r="5" fill="#dfe5ea"/>';
  if (sem) s += '<text x="150" y="162" text-anchor="middle" font-size="11" font-weight="600" style="fill:var(--alerta-txt)">sem resposta</text>';
  else s += '<path d="M150 147V' + (yS - 1) + '" stroke-width="1.5" stroke-dasharray="3 3" style="stroke:var(--frio)"/>' +
    '<path d="M146 ' + (yS - 6) + 'L150 ' + (yS - 1) + 'L154 ' + (yS - 6) + '" stroke-width="1.5" style="fill:none;stroke:var(--frio)"/>';
  $('comSvg').innerHTML = s + '</svg>';
}

function desenharGraficos() {
  if (!dados || $('tecnico').hidden) return;
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
  $('notaPeso').textContent = sem ? sem + (sem > 1 ? ' pesagens sem data não aparecem' : ' pesagem sem data não aparece') + ' no gráfico.' : '';
  desenharPesoFilhote();

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
    $('eclMsg').textContent = r.status === 404 ? 'Esta versão do ninho ainda não guarda a data de eclosão.' :
      r.j ? r.j.msg : 'O ninho respondeu com erro ' + r.status + '.';
  } catch (e) {
    $('eclMsg').textContent = 'Não consegui falar com o ninho.';
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
function filtrar(b) {
  filtro = b.dataset.n;
  for (const x of document.querySelectorAll('.filtros button')) x.setAttribute('aria-pressed', x === b);
  desenharLog();
}

// ---------------------------------------------------------------- histórico
// O firmware grava mensagens técnicas na flash; a tradução para o dono do
// ninho acontece só aqui. Cada regra reconhece o COMEÇO da mensagem e
// reaproveita os números dela. Mensagens: registrarLog() no ninho_esp32.ino.
// Eventos internos não aparecem (null); mensagem desconhecida volta como
// está, para nada sumir sem querer.
const INTERNOS = [/^ESP32 iniciado/, /^Hora acertada/, /^Entrou em modo AP/, /^Wi-Fi conectado/];
const TRADUCOES = [
  [/^Comida liberada \(manual\)/, () => 'Você liberou comida'],
  [/^Comida liberada \(autom/, () => 'Comida liberada automaticamente'],
  [/^Pesagem \S+ registrada: ([\d.,]+) g/, m => 'Filhote pesado: ' + m[1] + ' g'],
  [/^Temperatura fora da faixa: (-?[\d.,]+) °C/, m => 'Ninho ' + (quenteDemais(m[1]) ? 'quente' : 'frio') + ' demais: ' + m[1] + ' °C'],
  [/^Temperatura voltou à faixa(?:: (-?[\d.,]+) °C)?/, m => 'Temperatura do ninho voltou ao normal' + (m[1] ? ': ' + m[1] + ' °C' : '')],
  [/^Wi-Fi perdido/, () => 'O ninho ficou sem internet'],
  [/^Wi-Fi reconectado após (\d+) s/, m => 'O ninho voltou a se conectar (ficou ' + duracao(+m[1]) + ' sem internet)'],
  [/^DHT22 sem leitura/, () => 'Sensor de temperatura sem resposta'],
  [/^DHT22 voltou a responder/, () => 'Sensor de temperatura voltou a responder'],
  [/^HX711 sem leitura/, () => 'Balança sem resposta'],
  [/^HX711 voltou a responder/, () => 'Balança voltou a responder'],
  [/^HC-SR04 sem leitura/, () => 'Sensor de comida sem resposta'],
  [/^HC-SR04 voltou a responder/, () => 'Sensor de comida voltou a responder'],
  [/^Reset por queda de tensão/, () => 'O ninho reiniciou por falta de energia'],
  [/^Reset por (watchdog|travamento)/, () => 'O ninho reiniciou sozinho após uma falha'],
  [/^Alimentação automática (ligada|desligada)/, m => 'Alimentação automática ' + m[1]],
  [/^Data de eclosão salva/, () => 'Data de eclosão salva'],
  [/^Pesagens apagadas \((\d+)\)/, m => 'Histórico de pesagens apagado (' + m[1] + (m[1] === '1' ? ' pesagem)' : ' pesagens)')],
  [/^Balança tarada automaticamente/, () => 'Balança zerada automaticamente (primeira vez)'],
  [/^Balança tarada/, () => 'Balança zerada'],
  [/^Balança calibrada/, () => 'Balança calibrada'],   // o fator só interessa no Técnico
];
// "Fora da faixa" não diz o lado: decide pelo valor e pela faixa do /dados
function quenteDemais(v) {
  const t = parseFloat(v.replace(',', '.'));
  const lo = dados ? dados.temp_min : 25, hi = dados ? dados.temp_max : 35;
  return t > hi || (t >= lo && t >= (lo + hi) / 2);
}
function traduzirEvento(msg) {
  if (INTERNOS.some(r => r.test(msg))) return null;
  for (const [r, f] of TRADUCOES) { const m = msg.match(r); if (m) return f(m); }
  return msg;
}
// Eventos que o dono do ninho vê, já traduzidos (mais recente primeiro)
function eventosCliente() {
  return logs.map(e => ({ e, txt: traduzirEvento(e.msg) })).filter(x => x.txt != null);
}
const SELOS = { ERRO: '<span class="selo atencao">Precisa de atenção</span>', AVISO: '<span class="selo aviso">Aviso</span>' };
const DIAS_SEM = ['domingo', 'segunda', 'terça', 'quarta', 'quinta', 'sexta', 'sábado'];
function nomeDia(t) {
  if (!t) return 'Sem data';
  const d = new Date(t * 1000), hoje = new Date(), dia = new Date(d);
  hoje.setHours(0, 0, 0, 0); dia.setHours(0, 0, 0, 0);
  const dif = Math.round((hoje - dia) / 86400000);
  if (dif === 0) return 'Hoje';
  if (dif === 1) return 'Ontem';
  return DIAS_SEM[d.getDay()] + ', ' + ddmm(d) + (d.getFullYear() !== hoje.getFullYear() ? '/' + d.getFullYear() : '');
}
const horaEv = e => e.t ? hms(new Date(e.t * 1000)).slice(0, 5) : 'sem hora';
function desenharHistorico() {
  // Agrupa por dia, mantendo a ordem (mais recente primeiro)
  const grupos = [];
  for (const x of eventosCliente()) {
    const dia = nomeDia(x.e.t);
    if (!grupos.length || grupos[grupos.length - 1].dia !== dia) grupos.push({ dia, ev: [] });
    grupos[grupos.length - 1].ev.push(x);
  }
  let h = '';
  for (const g of grupos) {
    h += '<section class="boot"><div><h3 class="rot">' + g.dia + '</h3><span class="nota">' + g.ev.length + (g.ev.length > 1 ? ' acontecimentos' : ' acontecimento') + '</span></div><ul>';
    for (const x of g.ev) h += '<li' + (novos.has(x.e.id) ? ' class="novo"' : '') + '><div class="linha-ev"><span class="hora">' + horaEv(x.e) + '</span>' +
      (SELOS[x.e.nivel] || '') + '</div><p>' + esc(x.txt) + '</p></li>';
    h += '</ul></section>';
  }
  $('listaHist').innerHTML = h || '<p class="vazio">Nada aconteceu por aqui ainda.</p>';
}
function desenharUltimos() {
  $('ultimos').innerHTML = eventosCliente().slice(0, 4).map(x => '<div class="ev"><div class="linha-ev">' + (SELOS[x.e.nivel] || '') +
    '<span class="nota">' + nomeDia(x.e.t) + ' · ' + horaEv(x.e) + '</span></div><span>' + esc(x.txt) + '</span></div>').join('') ||
    '<p class="vazio">Nada aconteceu por aqui ainda.</p>';
}
// Contador da aba Histórico: só "Precisa de atenção" e "Aviso" que o dono do
// ninho veria (eventos internos não contam), com id maior que o último visto
function contarNaoVistos() {
  if (logs.length && vistoAte > logs[0].id) vistoAte = 0;   // log apagado: os ids recomeçaram
  const n = eventosCliente().filter(x => x.e.nivel !== 'INFO' && x.e.id > vistoAte).length;
  $('naoVistos').hidden = !n;
  $('naoVistos').textContent = n;
  $('abaHist').setAttribute('aria-label', n ? 'Histórico, ' + n + (n > 1 ? ' avisos novos' : ' aviso novo') : 'Histórico');
}
function marcarVistos() {
  if (!logs.length) return;
  vistoAte = logs[0].id;
  try { localStorage.setItem('ninhoVisto', vistoAte); } catch (e) {}
  contarNaoVistos();
}

const ABAS = { ninho: 'abaNinho', historico: 'abaHist', tecnico: 'abaTec' };
function mostrar(aba) {
  for (const p in ABAS) { $(p).hidden = p !== aba; $(ABAS[p]).setAttribute('aria-selected', p === aba); }
  if (aba === 'historico') marcarVistos();
  if (aba === 'ninho') desenharPeso();      // gráficos medem a largura: só desenham visíveis
  if (aba === 'tecnico') { desenharGraficos(); desenharRede(); }
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
