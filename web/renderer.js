import { DrawYuv } from './draw-yuv.js';

const pickBtn = document.getElementById('pick-btn');
const playBtn = document.getElementById('play-btn');
const pauseBtn = document.getElementById('pause-btn');
const progress = document.getElementById('progress');
const meta = document.getElementById('meta');
const canvas = document.getElementById('video-canvas');
const yuv = new DrawYuv(canvas);

let info = null;
let playing = false;
let rafId = 0;
let startTs = 0;
let pausePts = 0;
let dragging = false;

function fitCanvasToWindow(videoWidth, videoHeight) {
  const maxW = Math.max(1, window.innerWidth - 24);
  const maxH = Math.max(1, window.innerHeight - 130);
  const ratio = Math.min(maxW / videoWidth, maxH / videoHeight, 1);
  const showW = Math.max(1, Math.floor(videoWidth * ratio));
  const showH = Math.max(1, Math.floor(videoHeight * ratio));
  canvas.style.width = `${showW}px`;
  canvas.style.height = `${showH}px`;
}

function compactPlane(src, lineSize, width, height) {
  const out = new Uint8Array(width * height);
  if (!src || lineSize <= 0 || width <= 0 || height <= 0) {
    return out;
  }
  for (let row = 0; row < height; row += 1) {
    const srcStart = row * lineSize;
    const srcEnd = srcStart + width;
    out.set(src.subarray(srcStart, srcEnd), row * width);
  }
  return out;
}

function stopLoop() {
  if (rafId) {
    cancelAnimationFrame(rafId);
    rafId = 0;
  }
}

async function drawFrame(ptsMs, useSeek) {
  if (!info) return;
  const frame = useSeek ? await window.nativeVideo.seek(ptsMs) : await window.nativeVideo.decode(ptsMs);
  if (!frame) return;
  const ySrc = new Uint8Array(frame.y);
  const uSrc = new Uint8Array(frame.u);
  const vSrc = new Uint8Array(frame.v);
  const yData = compactPlane(ySrc, frame.lineY || frame.width, frame.width, frame.height);
  const uvWidth = frame.width >> 1;
  const uvHeight = frame.height >> 1;
  const uData = compactPlane(uSrc, frame.lineU || uvWidth, uvWidth, uvHeight);
  const vData = compactPlane(vSrc, frame.lineV || uvWidth, uvWidth, uvHeight);
  yuv.play({
    width: frame.width,
    height: frame.height,
    yData,
    uData,
    vData,
  });
}

async function tick() {
  if (!playing || !info) return;
  const pts = Date.now() - startTs;
  if (pts >= info.durationMs) {
    playing = false;
    pausePts = info.durationMs;
    progress.value = '100';
    stopLoop();
    return;
  }
  if (!dragging) {
    await drawFrame(pts, false);
    progress.value = String((pts / info.durationMs) * 100);
    pausePts = pts;
  }
  rafId = requestAnimationFrame(tick);
}

function resizeCanvas(width, height) {
  canvas.width = width;
  canvas.height = height;
  fitCanvasToWindow(width, height);
}

pickBtn.addEventListener('click', async () => {
  const filePath = await window.nativeVideo.pickFile();
  if (!filePath) return;
  await window.nativeVideo.close();
  info = await window.nativeVideo.open(filePath);
  resizeCanvas(info.width, info.height);
  meta.textContent = `${filePath} | ${info.width}x${info.height} | ${Math.round(info.durationMs)}ms | ${info.fps.toFixed(2)}fps`;
  pausePts = 0;
  progress.value = '0';
  await drawFrame(0, true);
  // Fallback: some streams may not return a frame on the first seek.
  await drawFrame(0, false);
});

playBtn.addEventListener('click', () => {
  if (!info) return;
  playing = true;
  startTs = Date.now() - pausePts;
  stopLoop();
  rafId = requestAnimationFrame(tick);
});

pauseBtn.addEventListener('click', () => {
  playing = false;
  stopLoop();
});

progress.addEventListener('input', async () => {
  if (!info) return;
  dragging = true;
  await window.nativeVideo.holdSeek(true);
  const pts = (Number(progress.value) / 100) * info.durationMs;
  pausePts = pts;
  await drawFrame(pts, true);
});

progress.addEventListener('change', async () => {
  if (!info) return;
  await window.nativeVideo.holdSeek(false);
  dragging = false;
  const pts = (Number(progress.value) / 100) * info.durationMs;
  pausePts = pts;
  if (playing) {
    startTs = Date.now() - pausePts;
  }
});

window.addEventListener('beforeunload', async () => {
  stopLoop();
  await window.nativeVideo.close();
});

window.addEventListener('resize', () => {
  if (info) {
    fitCanvasToWindow(info.width, info.height);
  }
});
