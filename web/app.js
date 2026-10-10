// app.js - Batman: Arkham Asylum detective mode flashbang fix
//
// Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>

import createModule from './baadmff.js';

const $ = (id) => document.getElementById(id);
const log = (s) => { $('log').textContent += s + '\n'; };

// Mirrors enum baadmff_status in src/baadmff.h
const OK = 0, ALREADY = 1;
const HELP = {
  1: 'This BmGame.u is already fixed. Nothing to do.',
  2: "That's not BmGame.u. Pick BmGame.u from BmGame\\CookedPC (step 1).",
  3: "This isn't the Steam GOTY BmGame.u, the only version supported so far. " +
     "If it is, open an issue and paste the log.",
  4: 'Your BmGame.u looks modded, refusing to guess where to patch. ' +
     'Verify game files in Steam (see Undo) and try again.',
  5: 'Your BmGame.u looks damaged. Verify game files in Steam (see Undo) ' +
     'and try again.',
  6: 'Your browser ran out of memory. Close some tabs and try again.',
};

const mod = createModule({ print: log, printErr: log });
let url = null;

function status(msg, cls = '') {
  $('status').textContent = msg;
  $('status').className = cls;
}

async function fix(file) {
  $('done').hidden = true;
  $('log').textContent = '';
  if (url) URL.revokeObjectURL(url);
  log(`file: ${file.name} (${file.size} bytes)`);
  status(`Fixing ${file.name}…`);
  await new Promise((r) => requestAnimationFrame(() => setTimeout(r)));

  const m = await mod;
  const data = new Uint8Array(await file.arrayBuffer());
  // wasm32: pointers and size_t are both 4 bytes
  const slots = m._malloc(8);
  const inp = m._malloc(data.length);
  if (!slots || !inp) {
    m._free(inp);
    m._free(slots);
    return status(HELP[6], 'bad');
  }
  m.HEAPU8.set(data, inp);

  const s = m._baadmff_patch(inp, data.length, slots, slots + 4);
  m._free(inp);
  log(`status: ${s} (${m.UTF8ToString(m._baadmff_strerror(s))})`);

  if (s === OK) {
    // HEAP* views are replaced on memory growth, read them after the call
    const out = m.HEAPU32[slots >> 2], len = m.HEAPU32[(slots >> 2) + 1];
    url = URL.createObjectURL(new Blob([m.HEAPU8.subarray(out, out + len)]));
    m._free(out);
    $('save').href = url;
    $('done').hidden = false;
    status('Done! Download it and follow step 3.', 'ok');
  } else {
    status(HELP[s] ?? 'Something went wrong. Open an issue and paste the log.',
           s === ALREADY ? 'ok' : 'bad');
  }
  m._free(slots);
}

function run(file) {
  fix(file).catch((e) => {
    log(String(e?.stack ?? e));
    status('Something went wrong. Open an issue and paste the log.', 'bad');
  });
}

$('file').addEventListener('change', (e) => {
  if (e.target.files[0]) run(e.target.files[0]);
  e.target.value = '';
});
const drop = $('drop');
drop.addEventListener('dragover', (e) => {
  e.preventDefault();
  drop.classList.add('over');
});
drop.addEventListener('dragleave', () => drop.classList.remove('over'));
drop.addEventListener('drop', (e) => {
  e.preventDefault();
  drop.classList.remove('over');
  if (e.dataTransfer.files[0]) run(e.dataTransfer.files[0]);
});
mod.catch((e) => {
  log(String(e));
  status("Couldn't load the patcher. Try a different browser.", 'bad');
});
