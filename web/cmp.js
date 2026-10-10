// cmp.js - Before/after slider
//
// Copyright (C) 2026 uiop <uiop@wasdhjkl.xyz>
//
// flash.mp4 is before stacked on top of after. The <video> shows the bottom
// half, the canvas paints the top half left of the split from the same
// element. One decoder, so both halves are always on the same frame.

const v = document.getElementById('vid');
const c = document.getElementById('before');
const s = document.getElementById('split');
const frame = c.parentElement;
const ctx = c.getContext('2d');

function draw() {
  const w = v.videoWidth, h = v.videoHeight / 2;
  if (!h) return;
  if (c.width !== w || c.height !== h) {
    c.width = w;
    c.height = h;
  }
  const x = Math.round((w * s.value) / 100);
  ctx.clearRect(0, 0, w, h);
  if (x > 0) ctx.drawImage(v, 0, 0, x, h, 0, 0, x, h);
}

function split() {
  frame.style.setProperty('--split', `${s.value}%`);
  draw();
}

const tick = 'requestVideoFrameCallback' in v
  ? () => v.requestVideoFrameCallback(() => { draw(); tick(); })
  : () => requestAnimationFrame(() => { draw(); tick(); });

// A page about killing a flashbang shouldn't autoplay one
const still = matchMedia('(prefers-reduced-motion: reduce)').matches;

function start() {
  if (still) {
    v.pause();
    v.currentTime = 1.5; // flash peak, as a still
  }
  split();
  tick();
}

s.addEventListener('input', split);
v.addEventListener('seeked', draw);
if (still) v.removeAttribute('autoplay');
if (v.readyState >= HTMLMediaElement.HAVE_CURRENT_DATA) start();
else v.addEventListener('loadeddata', start, { once: true });
