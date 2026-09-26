# Controle remoto do PC — ESP32 + Wake-on-LAN + MQTT + PWA

Ligar o PC (Windows 11, Ethernet) de qualquer lugar pelo celular. Um ESP32 na rede de casa recebe
o comando via MQTT e envia um Magic Packet (Wake-on-LAN) para a placa de rede do PC.

```
Celular (PWA) ──MQTT/WSS──▶ Broker MQTT (nuvem) ──MQTT/TLS──▶ ESP32 ──UDP broadcast (WoL)──▶ PC
```

## Hardware / ambiente

| Item | Valor |
|---|---|
| Placa | ESP32 Dev Module (ESP32-WROOM-32), USB-serial **CH340** em **COM3** |
| Core | `esp32:esp32` 3.3.12 (arduino-cli 1.5.1 que vem com o Arduino IDE) |
| PC alvo | MAC `B0:82:E2:4B:C2:9A` (Ethernet) |
| Node.js | v24.21.0 |
| ESP32 na rede | hostname `esp32-pc-remote`, IP 192.168.1.11 |

## Estrutura

```
esp32-pc-remote/esp32-pc-remote.ino   sketch do ESP32
esp32-pc-remote/ca_cert.h             certificados raiz Let's Encrypt (validação TLS do broker)
flash.ps1                             compila + grava (esptool, 115200 baud, sem auto-reset)
monitor.ps1                           lê a Serial (e opcionalmente envia um comando)
mqtt-test.ps1                         escuta meu-pc/# no broker e envia um comando (via npx mqtt)
app/                                  PWA (index.html, app.js, sw.js, manifest, ícones)
```

## Como gravar o ESP32

Esta placa **não entra sozinha em modo download** (erro `Wrong boot mode detected (0x13)`),
e a 921600 baud a transferência cai. Por isso o `flash.ps1` usa 115200 e `--before no-reset`.

1. Segure **BOOT**, aperte e solte **EN**, solte **BOOT**
2. `powershell -ExecutionPolicy Bypass -File .\flash.ps1`
3. Ler a serial: `powershell -ExecutionPolicy Bypass -File .\monitor.ps1 -Seconds 10`

## Progresso

- [x] **Fase 1 — Setup do ESP32**: core esp32 instalado, sketch de teste gravado,
      Serial imprimindo `ESP32 funcionando!` a cada 1 s.
- [x] **Fase 2 — Wi-Fi**: conecta na rede de casa (2,4 GHz), IP **192.168.1.11** (DHCP), RSSI -37 dBm.
      Reconexão automática pelo driver + nova tentativa a cada 10 s se continuar sem conexão.
      Credenciais em `esp32-pc-remote/secrets.h` (fora do git; modelo em `secrets.example.h`).
- [x] **Fase 3 — Wake-on-LAN local**: comando `LIGAR` na Serial envia o Magic Packet
      (3x para 192.168.1.255 e 255.255.255.255, UDP 9). **Testado com o PC desligado: ligou.**
      Windows: Wake on Magic Packet e Shutdown WoL habilitados, inicialização rápida desativada.
      Atalho: botão **BOOT** (GPIO0) = LIGAR. LED azul (GPIO2): aceso = Wi-Fi ok,
      3 piscadas rápidas = pacote enviado, 1 piscada longa = falha (sem Wi-Fi).
- [x] **Fase 4 — MQTT**: HiveMQ Cloud Serverless, TLS na 8883 validando o certificado com as
      raízes ISRG X1/X2 (PubSubClient 2.8). Usuários `esp32` (no `secrets.h`) e `app67` (PC/app).
      Testado pela internet com `mqtt-test.ps1`: `STATUS` → `online`, `LIGAR` → `wol_enviado`.
- [~] **Fase 5 — App web (PWA)**: pasta `app/` (HTML/JS puro, mqtt.js 5.16 incluso, sem build).
      Conecta via `wss://<cluster>:8884/mqtt`; host/usuário/senha digitados na tela de
      configuração e salvos só no aparelho (nada de credencial no código). Botão LIGAR fica
      ativo só com o ESP32 `online`; mostra a resposta ou "não respondeu" após 8 s.
      Testado no Chrome (localhost): conexão WSS, login recusado tratado, service worker ativo.
      Rodar local: `npx http-server app -p 8080`. **Falta: hospedar em HTTPS e instalar no celular.**
- [ ] Fase 6 — Revisão de segurança (autenticação + TLS)

## MQTT (Fase 4)

| Tópico | Direção | Conteúdo |
|---|---|---|
| `meu-pc/comando` | app → ESP32 | `LIGAR` ou `STATUS` (QoS 1, **sem retain**) |
| `meu-pc/status` | ESP32 → app | `online` / `offline` (retido; `offline` vem do Last Will se o ESP32 cair) |
| `meu-pc/resposta` | ESP32 → app | JSON, ex.: `{"comando":"LIGAR","resultado":"wol_enviado","uptime":123}` |

Resultados possíveis: `wol_enviado`, `falha` (sem Wi-Fi), `ignorado` (LIGAR repetido em < 5 s),
`online` (resposta ao STATUS), `desconhecido`.

### Criar o broker (HiveMQ Cloud, plano Serverless gratuito)
1. Crie a conta em https://console.hivemq.cloud e um cluster **Serverless (Free)**
2. Em **Overview**, copie a URL do cluster (`xxxx.s1.eu.hivemq.cloud`) → `MQTT_HOST` no `secrets.h`
3. Em **Access Management**, crie dois usuários: `esp32` (vai no `secrets.h`) e `app67` (PC/celular)
4. Grave o ESP32 (`flash.ps1`) e confira na Serial: `[MQTT] Conectado!`
5. Teste pelo PC: `powershell -ExecutionPolicy Bypass -File .\mqtt-test.ps1 -User app67 -Password <senha> -Send LIGAR`

Na Serial, `[MQTT] Falhou, estado -2` = problema de rede/TLS (host/porta errados);
estado `4` ou `5` = usuário/senha recusados.
