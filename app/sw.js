// Service worker: guarda os arquivos do app para abrir rápido e funcionar como app instalado.
// Ao mudar qualquer arquivo do app, aumente a versão para os celulares baixarem a nova.
const CACHE = 'meu-pc-v1';
const FILES = ['./', 'index.html', 'app.js', 'mqtt.min.js', 'manifest.webmanifest',
               'icon-192.png', 'icon-512.png', 'apple-touch-icon.png'];

self.addEventListener('install', e => {
  e.waitUntil(caches.open(CACHE).then(c => c.addAll(FILES)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', e => {
  e.waitUntil(caches.keys()
    .then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k))))
    .then(() => self.clients.claim()));
});

// Rede primeiro (pega atualizações quando há internet), cache como reserva
self.addEventListener('fetch', e => {
  if (e.request.method !== 'GET' || new URL(e.request.url).origin !== location.origin) return;
  e.respondWith(
    fetch(e.request)
      .then(res => { const copy = res.clone(); caches.open(CACHE).then(c => c.put(e.request, copy)); return res; })
      .catch(() => caches.match(e.request))
  );
});
