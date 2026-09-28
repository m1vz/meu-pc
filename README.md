<div align="center">

# 🖥️ Meu PC, de qualquer lugar

**Ligue seu computador pelo celular, de qualquer lugar do mundo, usando um ESP32 de poucos reais.**

![ESP32](https://img.shields.io/badge/ESP32-Arduino-0a7ea4?logo=espressif&logoColor=white)
![MQTT](https://img.shields.io/badge/MQTT-TLS-660066?logo=mqtt&logoColor=white)
![Wake-on-LAN](https://img.shields.io/badge/Wake--on--LAN-Magic%20Packet-2ea44f)
![PWA](https://img.shields.io/badge/App-PWA-5a0fc8?logo=pwa&logoColor=white)

### 👉 [Abrir o app](https://m1vz.github.io/meu-pc-app/) 👈

*Você não precisa hospedar nada: é só abrir o link, digitar os dados do **seu** broker e pronto.*

</div>

---

## 📖 O que é isso?

Um projeto para **ligar o PC remotamente**, sem mexer na placa-mãe, sem relé e sem abrir o gabinete.

Um ESP32 fica ligado em casa, conectado ao Wi-Fi. Quando você toca no botão do app, o comando viaja pela internet até ele, e ele acorda o PC usando **Wake-on-LAN**, um recurso que já existe na maioria das placas de rede.

## ⚙️ Como funciona

```mermaid
flowchart LR
    A["📱 App no celular<br/>(PWA)"] -- "WSS (criptografado)" --> B[("☁️ Broker MQTT<br/>HiveMQ Cloud")]
    B -- "MQTT + TLS" --> C["📡 ESP32<br/>em casa"]
    C -- "Magic Packet<br/>(UDP broadcast)" --> D["💻 PC"]
```

1. O app publica `LIGAR` no tópico `meu-pc/comando` do broker.
2. O ESP32 está conectado ao mesmo broker (é ele que puxa a conexão, então **não precisa abrir porta no roteador**).
3. Ao receber o comando, o ESP32 dispara o Magic Packet na rede local.
4. O PC acorda e o ESP32 responde `wol_enviado`, que aparece no app.

## 🧰 O que você precisa

| Item | Observação |
| --- | --- |
| **ESP32** (DevKit com ESP32-WROOM-32) | Qualquer placa comum serve |
| **Cabo USB de dados** | Só para gravar; depois basta um carregador de celular |
| **PC com Wake-on-LAN** | De preferência ligado por **cabo Ethernet** |
| **Wi-Fi 2,4 GHz** | O ESP32 não conecta em 5 GHz |
| **Conta no HiveMQ Cloud** | O plano gratuito (Serverless) é suficiente |
| **Arduino IDE** | Com o pacote de placas *esp32* da Espressif |

## 🚀 Passo a passo

### 1. Prepare o PC para acordar pela rede

1. **BIOS/UEFI:** ative *Wake on LAN* (às vezes chamado de *Power On by PCI-E*) e **desative** *ErP/EuP*, que corta a energia da placa de rede quando o PC desliga.
2. **Windows:** no Gerenciador de Dispositivos, abra sua placa de rede → *Gerenciamento de Energia* → marque **Permitir que este dispositivo ative o computador** e **Permitir apenas um Magic Packet**.
3. Ainda na placa de rede, aba *Avançado*: ative **Wake on Magic Packet** e **Shutdown Wake-On-Lan**, se existirem.
4. Desative a **Inicialização rápida** (Painel de Controle → Opções de Energia).
5. Descubra o **MAC** da placa de rede: `ipconfig /all` → *Endereço Físico*. Você vai usar no passo 3.

> 💡 O comando de teste é desligar pelo menu **Desligar** (não hibernar) e esperar uns 10 segundos antes de tentar acordar.

### 2. Crie o broker MQTT (HiveMQ Cloud, grátis)

1. Crie a conta em [console.hivemq.cloud](https://console.hivemq.cloud) e escolha **Nuvem → Serverless (Free)**.
2. Em *Visão geral*, copie a **URL do cluster** (`xxxx.s1.eu.hivemq.cloud`).
3. Em *Gestão de Acessos*, crie **dois usuários com senhas diferentes**:
   - um para o **ESP32**
   - um para o **app/celular**

   Anote as senhas na hora: o HiveMQ só mostra uma vez.

### 3. Configure e grave o ESP32

O firmware é o mesmo para todo mundo. **A única coisa que você edita é um arquivo de configuração, o `secrets.h`.** Não precisa mexer no código (`.ino`) nem no `ca_cert.h`.

**a) Crie o `secrets.h`**

Dentro da pasta `esp32-pc-remote/`, copie o `secrets.example.h` e renomeie a cópia para **`secrets.h`**. Abra e preencha com os seus dados:

```cpp
#define WIFI_SSID     "NomeDaSuaRede"
#define WIFI_PASSWORD "SenhaDoSeuWiFi"

#define MQTT_HOST     "xxxxxxxxxxxx.s1.eu.hivemq.cloud"
#define MQTT_PORT     8883
#define MQTT_USER     "usuario-do-esp32"
#define MQTT_PASSWORD "senha-do-usuario-do-esp32"

#define WOL_MAC       "AA:BB:CC:DD:EE:FF"
```

| Campo | O que colocar | Onde achar |
| --- | --- | --- |
| `WIFI_SSID` | Nome do seu Wi-Fi (**2,4 GHz**) | Ícone de Wi-Fi do PC/celular |
| `WIFI_PASSWORD` | Senha desse Wi-Fi | Sua rede |
| `MQTT_HOST` | URL do cluster, **sem** `https://` e **sem** porta | HiveMQ → *Visão geral* → URL |
| `MQTT_PORT` | Deixe `8883` | Porta MQTT com TLS (o app usa a 8884 sozinho) |
| `MQTT_USER` | Usuário criado **para o ESP32** | HiveMQ → *Gestão de Acessos* |
| `MQTT_PASSWORD` | Senha desse usuário | Anotada quando você criou |
| `WOL_MAC` | MAC da placa de rede **do PC que vai ligar** | `ipconfig /all` → *Endereço Físico* (pode usar `:` ou `-`) |

> ⚠️ Mantenha as **aspas**. O `MQTT_USER` é o usuário do **ESP32**, não o do app.
>
> O `secrets.h` fica só no seu computador: ele já está no `.gitignore`.

**b) Instale o que o Arduino IDE precisa**

- **Placas:** *Gerenciador de Placas* → procure `esp32` → instale o pacote **esp32 by Espressif Systems** (não o "Arduino ESP32 Boards").
- **Biblioteca:** *Gerenciador de Bibliotecas* → instale **PubSubClient** (de Nick O'Leary).
- `WiFi`, `WiFiClientSecure` e `WiFiUdp` já vêm junto com o pacote esp32.

**c) Grave**

1. Abra o arquivo `esp32-pc-remote/esp32-pc-remote.ino` (a pasta precisa continuar com o nome `esp32-pc-remote`).
2. *Ferramentas → Placa →* **ESP32 Dev Module**.
3. *Ferramentas → Porta →* a porta do ESP32.
4. Clique em **Upload**.

**d) Confira**

Abra o *Monitor Serial* em **115200 baud**. Deve aparecer:

```
[WiFi] Conectado! IP: 192.168.x.x  RSSI: -50 dBm
[MQTT] Conectado!
```

Para testar sem o app, digite `LIGAR` no Monitor Serial (com fim de linha *Nova linha*) ou aperte o botão **BOOT** da placa.

> 🔧 O `ca_cert.h` já traz os certificados usados pelo HiveMQ Cloud (Let's Encrypt). Só troque se for usar outro broker.

<details>
<summary>⚠️ Deu erro <code>Wrong boot mode detected (0x13)</code> no upload?</summary>

Placas com chip **CH340** às vezes não entram sozinhas em modo de gravação. Faça assim: segure **BOOT**, aperte e solte **EN**, solte **BOOT** e rode o upload de novo.

</details>

Depois de gravar, **tire o ESP32 do USB do PC** e ligue-o em um carregador de celular na tomada. Se ele ficar no USB do PC, desliga junto com o PC.

### 4. Use o app

1. Abra **[m1vz.github.io/meu-pc-app](https://m1vz.github.io/meu-pc-app/)** no celular.
2. Preencha:
   - **Servidor:** a URL do seu cluster HiveMQ
   - **Usuário e senha:** do usuário **do app** (não o do ESP32)
3. Toque em **Salvar e conectar**.
4. Instale como aplicativo:
   - **Android:** menu ⋮ → *Instalar app*
   - **iPhone:** Compartilhar → *Adicionar à Tela de Início*

🔒 **Seus dados ficam só no seu aparelho.** O app roda inteiro no navegador e conversa direto com o *seu* broker. Não existe servidor intermediário, então quem hospeda o link nunca vê seu host, usuário ou senha.

## 📡 Tópicos MQTT

| Tópico | Direção | Conteúdo |
| --- | --- | --- |
| `meu-pc/comando` | app → ESP32 | `LIGAR` ou `STATUS` (QoS 1, **sem retain**) |
| `meu-pc/status` | ESP32 → app | `online` / `offline` (retido; o `offline` vem do Last Will se o ESP32 cair) |
| `meu-pc/resposta` | ESP32 → app | JSON, ex.: `{"comando":"LIGAR","resultado":"wol_enviado"}` |

Resultados possíveis: `wol_enviado`, `falha` (sem Wi-Fi), `ignorado` (LIGAR repetido em menos de 5 s), `online` (resposta ao STATUS) e `desconhecido`.

## 💡 Indicadores da placa

- **LED azul aceso:** Wi-Fi conectado
- **3 piscadas rápidas:** Magic Packet enviado
- **1 piscada longa:** falha (sem Wi-Fi)
- **Botão BOOT:** funciona como atalho de LIGAR, sem precisar do app

## 🛠️ Solução de problemas

| Sintoma | Provável causa |
| --- | --- |
| `[MQTT] Falhou, estado -2` | Host ou porta errados, ou problema de rede/TLS |
| `[MQTT] Falhou, estado 4` ou `5` | Usuário ou senha do ESP32 recusados |
| App diz "usuário ou senha recusados" | Você usou o usuário do ESP32 no app, ou a senha está errada |
| Pacote enviado, mas o PC não liga | Wake-on-LAN não está ativo na BIOS/Windows, ou o *ErP/EuP* está ligado. Confira se o LED da porta Ethernet continua aceso com o PC desligado |
| ESP32 não conecta no Wi-Fi | A rede precisa ser 2,4 GHz |
| Botão LIGAR desabilitado no app | O ESP32 está offline |

## 🔐 Segurança

- **Nunca publique `LIGAR` com a opção *retain*.** O broker guardaria a mensagem e o PC ligaria toda vez que o ESP32 reconectasse.
- Use **senhas diferentes** para o usuário do ESP32 e o do app.
- A conexão com o broker é sempre **criptografada (TLS)**, e o ESP32 valida o certificado do servidor.
- O arquivo `secrets.h` está no `.gitignore` e **não deve ser enviado ao GitHub**.

## 📁 Estrutura do repositório

```
esp32-pc-remote/
├── esp32-pc-remote.ino    firmware do ESP32
├── ca_cert.h              certificados raiz para validar o TLS do broker
└── secrets.example.h      modelo de configuração (copie para secrets.h)

flash.ps1                  (opcional) compila e grava pelo PowerShell
monitor.ps1                (opcional) lê a Serial e envia comandos
mqtt-test.ps1              (opcional) testa o broker pelo PC
```

Os três scripts `.ps1` são atalhos do meu setup: **Windows**, Arduino IDE 2 instalado na pasta padrão e ESP32 na porta `COM3`. Se quiser usá-los, passe a sua porta (ex.: `-Port COM5`). Pelo Arduino IDE normal você **não precisa deles**.

## 🗺️ Como o projeto foi construído

- [x] **Fase 1:** setup do ESP32 e primeiro upload
- [x] **Fase 2:** conexão Wi-Fi com reconexão automática
- [x] **Fase 3:** Wake-on-LAN local, testado com o PC desligado
- [x] **Fase 4:** MQTT com TLS no HiveMQ Cloud
- [x] **Fase 5:** app web (PWA) instalável no celular
- [ ] **Próximos passos:** revisão final de segurança e detecção do estado real do PC (ligado/desligado)

---

<div align="center">

Feito com 🔌 por [@m1vz](https://github.com/m1vz)

</div>
