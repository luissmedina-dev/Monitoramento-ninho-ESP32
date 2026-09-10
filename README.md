# Sistema de Monitoramento de Ninho de Aves

Projeto desenvolvido para a disciplina de *Conectividade em Sistemas Ciberfísicos* da PUCPR.

O projeto consiste em um sistema ciberfísico para monitoramento de um ninho de calopsitas, utilizando um ESP32 como servidor HTTP.

## Objetivo

Desenvolver um sistema capaz de coletar e disponibilizar informações sobre o ambiente e o desenvolvimento do filhote, permitindo o acompanhamento por meio de uma interface web.

## Funcionalidades previstas

- Monitoramento da temperatura e umidade do ninho;
- Monitoramento do peso do filhote;
- Monitoramento do nível de ração;
- Interface web para visualização dos dados;
- Histórico das medições;
- Comparação entre peso real e peso esperado;
- Acionamento remoto do dispensador de ração;
- Registro de logs do sistema;
- Monitoramento da conexão Wi-Fi.

## Componentes

- ESP32;
- DHT22;
- Célula de carga de 1 kg;
- Módulo HX711;
- HC-SR04;
- Servo motor SG90.

## Arquitetura

Sensores → ESP32 (Servidor HTTP) → Wi-Fi/TCP-IP → Cliente Web

Para o acionamento do alimentador:

Cliente Web → Requisição HTTP → ESP32 → Servo Motor → Dispensador de ração

## Status

🚧 Projeto em desenvolvimento.
