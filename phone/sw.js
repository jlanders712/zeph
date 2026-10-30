// Caches the page, map libraries and fonts so everything opens with no internet after the first visit.
const CACHE = 'loratrack-v3';
const ASSETS = [
 "./",
 "index.html",
 "lib/maplibre-gl.js",
 "lib/maplibre-gl.css",
 "lib/pmtiles.js",
 "fonts/Noto Sans Medium/0-255.pbf",
 "fonts/Noto Sans Medium/256-511.pbf",
 "fonts/Noto Sans Medium/8192-8447.pbf",
 "fonts/Noto Sans Regular/0-255.pbf",
 "fonts/Noto Sans Regular/256-511.pbf",
 "fonts/Noto Sans Regular/8192-8447.pbf"
];

// Add each file separately so one missing file (e.g. lib/ not downloaded yet) doesn't break the install.
self.addEventListener('install', e => {
  e.waitUntil(
    caches.open(CACHE)
      .then(c => Promise.all(ASSETS.map(u => c.add(new Request(u, { cache: 'reload' })).catch(() => {}))))
      .then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys().then(keys => Promise.all(keys.filter(k => k !== CACHE).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

// Serve from cache instantly; refresh the cache in the background when online.
self.addEventListener('fetch', e => {
  if (e.request.method !== 'GET' || new URL(e.request.url).origin !== location.origin) return;
  e.respondWith(
    caches.open(CACHE).then(async cache => {
      const cached = await cache.match(e.request, { ignoreSearch: true });
      const refresh = fetch(e.request).then(res => {
        if (res && res.ok) cache.put(e.request, res.clone());
        return res;
      }).catch(() => cached || Response.error());
      return cached || refresh;
    })
  );
});
