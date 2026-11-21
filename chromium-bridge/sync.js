/* Turnwave NX2 v0.5 MusicKit bridge. Runs only at music.apple.com.
 * The native IPC includes resource IDs and public library metadata, never
 * credentials, DRM material or MusicKit authorization tokens.
 */
(() => {
  'use strict';
  if (location.hostname !== 'music.apple.com' || window.__twNx2MusicBridge) return;
  Object.defineProperty(window, '__twNx2MusicBridge', { value: true });
  const collections = [
    ['P', '/v1/me/library/playlists', {limit: 35}],
    ['A', '/v1/me/library/albums', {limit: 35}],
    ['S', '/v1/me/library/songs', {limit: 55}],
    ['R', '/v1/me/library/artists', {limit: 25}],
    ['N', '/v1/me/library/recently-added', {limit: 20}],
  ];
  let synchronized = false, busy = false, playbackRequested = false;
  const clean = (text, limit=130) => String(text ?? '').replace(/[\t\n\r\u0000-\u001f]/g, ' ').trim().slice(0, limit);
  const pack = (text, limit=130) => encodeURIComponent(clean(text, limit));
  function emit(message) {
    if (typeof window.gxTurnwaveSync !== 'function') throw new Error('GX native binding not ready');
    window.gxTurnwaveSync(message);
  }
  function resource(item, kind) {
    const a = item?.attributes || {};
    const p = a.playParams || {};
    const name = clean(a.name || a.title || a.albumName || 'Unknown');
    const detail = clean(a.artistName || a.curatorName || a.albumName || a.description?.short || item.type || '');
    // Library and catalog identifiers are different. Do not guess a catalog ID
    // from a library ID such as l.xxxxxx. A song can lack catalogId entirely.
    const libraryId = clean(item?.id, 90);
    const catalogId = clean(p.catalogId || item?.relationships?.catalog?.data?.[0]?.id || '', 60);
    return `ITEM|${kind}|${pack(libraryId,90)}|${pack(catalogId,60)}|${pack(name)}|${pack(detail)}`;
  }
  async function sync(mk) {
    if (busy || synchronized) return;
    busy = true;
    try {
      let success = 0;
      const messages = [];
      for (const [kind,path,params] of collections) {
        try {
          const response = await mk.api.music(path,params);
          const items = response?.data?.data || [];
          if (!Array.isArray(items)) continue;
          ++success;
          for (const item of items) if (item && item.id) messages.push(resource(item,kind));
        } catch (error) {
          console.info('Turnwave: MusicKit category unavailable',kind, String(error));
        }
      }
      if (success === 0) throw new Error('No accessible MusicKit library endpoints');
      emit('BEGIN|2');
      for (const line of messages.slice(0,180)) emit(line);
      emit('END');
      synchronized = true;
    } catch (error) {
      console.warn('Turnwave: sync failed', String(error));
      try { emit('ABORT'); } catch (_) {}
    } finally { busy = false; }
  }
  async function tryPlay(mk, id) {
    if (playbackRequested || !/^[0-9]{2,30}$/.test(id)) return;
    playbackRequested = true;
    try {
      emit(`PLAY|QUEUE|${id}`);
      if (typeof mk.setQueue !== 'function' || typeof mk.play !== 'function')
        throw new Error('MusicKit setQueue/play not available');
      await mk.setQueue({song:id});
      emit(`PLAY|QUEUED|${id}`);
      await mk.play();
      // A fulfilled Promise proves only that MusicKit accepted the request.
      // It does not establish audible output or a working CDM.
      emit(`PLAY|REQUESTED|${id}`);
      // Playback state is diagnostic only; it cannot establish audible sound.
      for (const wait of [2500, 9000]) {
        setTimeout(() => {
          const state = mk.isPlaying ? 'playing' : 'not-playing';
          const position = Number.isFinite(mk.currentPlaybackTime) ? Math.floor(mk.currentPlaybackTime) : -1;
          try { emit(`PLAY|STATE|${state}|${position}`); } catch (_) {}
        }, wait);
      }
    } catch (error) {
      emit(`PLAY|ERROR|${pack(error?.message || String(error),180)}`);
    }
  }
  function tick() {
    try {
      const mk = window.MusicKit?.getInstance?.();
      if (!mk?.isAuthorized) return;
      if (mk.api && typeof mk.api.music === 'function' && !synchronized) void sync(mk);
      const id = String(window.__twPlayCatalogId || '');
      if (id && !playbackRequested) void tryPlay(mk,id);
    } catch (error) {
      console.info('Turnwave: MusicKit not ready',String(error));
    }
  }
  tick();
  setInterval(tick,1700);
})();
