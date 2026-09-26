// Meu PC — PWA que envia LIGAR ao ESP32 pelo HiveMQ Cloud (MQTT sobre WebSocket seguro, porta 8884)

const TOPIC_COMANDO = 'meu-pc/comando';
const TOPIC_STATUS = 'meu-pc/status';
const TOPIC_RESPOSTA = 'meu-pc/resposta';
const RESPOSTA_TIMEOUT_MS = 8000;
const CONFIG_KEY = 'meu-pc-config';

const $ = id => document.getElementById(id);
let client = null;
let espOnline = false;
let pending = null;  // { comando, timer } aguardando a resposta do ESP32

function loadConfig() {
  try { return JSON.parse(localStorage.getItem(CONFIG_KEY)) || null; } catch { return null; }
}

function saveConfig(cfg) {
  try { localStorage.setItem(CONFIG_KEY, JSON.stringify(cfg)); } catch {}
}

function show(view) {
  $('home').hidden = view !== 'home';
  $('settings').hidden = view !== 'settings';
}

function setPill(name, state, text) {
  $(name + '-dot').className = 'dot ' + state;
  $(name + '-text').textContent = text;
}

function setMsg(text, cls = '') {
  $('msg').textContent = text;
  $('msg').className = cls;
}

function log(text) {
  const li = document.createElement('li');
  const hora = new Date().toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  li.textContent = `${hora} — ${text}`;
  $('log').prepend(li);
  while ($('log').children.length > 30) $('log').lastChild.remove();
}

function updatePowerButton() {
  $('power').disabled = !(client && client.connected && espOnline) || pending !== null;
}

function connect() {
  const cfg = loadConfig();
  if (!cfg) { show('settings'); return; }
  if (client) client.end(true);

  espOnline = false;
  setPill('broker', 'wait', 'Conectando…');
  setPill('esp', '', 'ESP32 ?');
  updatePowerButton();

  client = mqtt.connect(`wss://${cfg.host}:8884/mqtt`, {
    username: cfg.user,
    password: cfg.pass,
    clientId: 'meu-pc-app-' + Math.random().toString(16).slice(2, 10),
    clean: true,
    reconnectPeriod: 3000,
    connectTimeout: 10000,
  });

  client.on('connect', () => {
    setPill('broker', 'ok', 'Broker');
    client.subscribe([TOPIC_STATUS, TOPIC_RESPOSTA], { qos: 1 });
    updatePowerButton();
  });

  client.on('reconnect', () => setPill('broker', 'wait', 'Reconectando…'));
  client.on('offline', () => { setPill('broker', 'bad', 'Sem conexão'); updatePowerButton(); });
  client.on('close', updatePowerButton);

  client.on('error', err => {
    // 4/5 (MQTT 3.1.1) ou 134/135 (MQTT 5) = usuário/senha recusados: não adianta ficar tentando
    if ([4, 5, 134, 135].includes(err.code)) {
      client.end(true);
      setPill('broker', 'bad', 'Login recusado');
      setMsg('Usuário ou senha recusados pelo broker', 'bad');
      log('Login recusado — confira as configurações');
    } else {
      setPill('broker', 'bad', 'Erro');
      log('Erro de conexão: ' + err.message);
    }
  });

  client.on('message', (topic, payload) => {
    const text = payload.toString();
    if (topic === TOPIC_STATUS) {
      espOnline = text === 'online';
      setPill('esp', espOnline ? 'ok' : 'bad', espOnline ? 'ESP32 online' : 'ESP32 offline');
      updatePowerButton();
    } else if (topic === TOPIC_RESPOSTA) {
      handleResposta(text);
    }
  });
}

function handleResposta(text) {
  let r;
  try { r = JSON.parse(text); } catch { return; }
  if (!pending || r.comando !== pending.comando) return;
  clearTimeout(pending.timer);
  pending = null;
  $('power').classList.remove('busy');
  updatePowerButton();

  const textos = {
    wol_enviado: ['Sinal enviado! O PC deve ligar em alguns segundos.', 'ok'],
    ignorado: ['Comando repetido ignorado (aguarde 5 s).', ''],
    falha: ['O ESP32 está sem Wi-Fi, o sinal não foi enviado.', 'bad'],
  };
  const [msg, cls] = textos[r.resultado] || [`Resposta: ${r.resultado}`, ''];
  setMsg(msg, cls);
  log(msg);
}

function ligar() {
  if (!client || !client.connected || pending) return;
  // Sem retain: um LIGAR retido seria reexecutado a cada reconexão do ESP32
  client.publish(TOPIC_COMANDO, 'LIGAR', { qos: 1, retain: false });
  if (navigator.vibrate) navigator.vibrate(30);
  $('power').classList.add('busy');
  setMsg('Enviando…');
  pending = {
    comando: 'LIGAR',
    timer: setTimeout(() => {
      pending = null;
      $('power').classList.remove('busy');
      updatePowerButton();
      setMsg('O ESP32 não respondeu. Tente de novo.', 'bad');
      log('Sem resposta do ESP32');
    }, RESPOSTA_TIMEOUT_MS),
  };
  updatePowerButton();
}

function openSettings() {
  const cfg = loadConfig() || {};
  const f = $('settings-form');
  f.host.value = cfg.host || '';
  f.user.value = cfg.user || '';
  f.pass.value = cfg.pass || '';
  $('cancel-settings').hidden = !loadConfig();
  show('settings');
}

$('power').addEventListener('click', ligar);
$('open-settings').addEventListener('click', openSettings);
$('cancel-settings').addEventListener('click', () => show('home'));
$('settings-form').addEventListener('submit', e => {
  e.preventDefault();
  const f = e.target;
  const host = f.host.value.trim().replace(/^(wss?|mqtts?):\/\//, '').replace(/[:/].*$/, '');
  saveConfig({ host, user: f.user.value.trim(), pass: f.pass.value });
  setMsg('Toque para ligar o PC');
  show('home');
  connect();
});

// Ao voltar para o app (celular tira a aba do segundo plano), reconecta se a conexão caiu
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && client && !client.connected && loadConfig()) client.reconnect();
});

if ('serviceWorker' in navigator) navigator.serviceWorker.register('sw.js').catch(() => {});

if (loadConfig()) { show('home'); connect(); } else openSettings();
