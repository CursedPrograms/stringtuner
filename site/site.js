// stringtuner - the instrument list and a small browser tuner (YIN on the microphone), with the
// same instruments, needle and strobe as the plugin.

"use strict";

// Source/Instruments.cpp: open strings as MIDI notes, in the order they're usually named
const INSTRUMENTS = [
  ["Chromatic", "Any note", []],
  ["Violino piccolo (Bach)", "Bowed", [58, 65, 72, 79]],
  ["Violin", "Bowed", [55, 62, 69, 76]],
  ["Viola", "Bowed", [48, 55, 62, 69]],
  ["Cello", "Bowed", [36, 43, 50, 57]],
  ["Double bass", "Bowed", [28, 33, 38, 43]],
  ["Double bass, low C", "Bowed", [24, 33, 38, 43]],
  ["Double bass, 5-string", "Bowed", [23, 28, 33, 38, 43]],
  ["Double bass, solo tuning", "Bowed", [30, 35, 40, 45]],
  ["Octobass", "Bowed", [12, 19, 24]],
  ["Viola da gamba (bass)", "Bowed", [38, 43, 48, 52, 57, 62]],
  ["Erhu", "Bowed", [62, 69]],
  ["Guitar", "Guitar", [40, 45, 50, 55, 59, 64]],
  ["Guitar, drop D", "Guitar", [38, 45, 50, 55, 59, 64]],
  ["Guitar, 7-string", "Guitar", [35, 40, 45, 50, 55, 59, 64]],
  ["Guitar, 8-string", "Guitar", [30, 35, 40, 45, 50, 55, 59, 64]],
  ["Baritone guitar", "Guitar", [35, 40, 45, 50, 54, 59]],
  ["Bass, 4-string", "Bass", [28, 33, 38, 43]],
  ["Bass, 5-string", "Bass", [23, 28, 33, 38, 43]],
  ["Bass, 6-string", "Bass", [23, 28, 33, 38, 43, 48]],
  ["Bass, drop D", "Bass", [26, 33, 38, 43]],
  ["Ukulele", "Small", [67, 60, 64, 69]],
  ["Ukulele, low G", "Small", [55, 60, 64, 69]],
  ["Baritone ukulele", "Small", [50, 55, 59, 64]],
  ["Mandolin", "Small", [55, 62, 69, 76]],
  ["Mandola", "Small", [48, 55, 62, 69]],
  ["Cavaquinho", "Small", [62, 67, 71, 74]],
  ["Balalaika (prima)", "Small", [64, 64, 69]],
  ["Banjo, 5-string (open G)", "Folk", [67, 50, 55, 59, 62]],
  ["Bouzouki (Irish)", "Folk", [43, 50, 57, 62]],
];

const NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const noteName = (midi) => NAMES[((midi % 12) + 12) % 12] + (Math.floor(midi / 12) - 1);
const hzFor = (midi, a4) => a4 * Math.pow(2, (midi - 69) / 12);
const midiFor = (hz, a4) => 69 + 12 * Math.log2(hz / a4);
const IN_TUNE = 2;    // cents

// ---------------------------------------------------------------- the instrument list

function listInstruments() {
  const list = document.getElementById("instrument-list");
  if (!list) return;
  for (const [name, group, strings] of INSTRUMENTS) {
    const card = document.createElement("article");
    card.className = "instrument";
    const h = document.createElement("h3");
    h.textContent = name;
    const p = document.createElement("p");
    p.textContent = strings.length ? `${group}: ${strings.map(noteName).join(" ")}` : "any note, C0 to C8";
    card.append(h, p);
    list.appendChild(card);
  }
}

// ---------------------------------------------------------------- YIN

// The period of the newest stretch of samples, or 0. minHz sets how far back it has to look.
function yin(buf, sr, minHz, maxHz) {
  const maxLag = Math.min(Math.floor(sr / minHz), Math.floor(buf.length / 2) - 2);
  const minLag = Math.max(2, Math.floor(sr / maxHz));
  const w = Math.min(buf.length - maxLag - 2, Math.max(maxLag, Math.floor(sr * 0.02)));
  const start = buf.length - w - maxLag - 2;
  if (w <= 0 || minLag >= maxLag) return 0;

  const d = new Float32Array(maxLag + 2);
  for (let lag = 1; lag <= maxLag + 1; lag++) {
    let sum = 0;
    for (let j = 0; j < w; j++) {
      const diff = buf[start + j] - buf[start + j + lag];
      sum += diff * diff;
    }
    d[lag] = sum;
  }
  let running = 0;
  d[0] = 1;
  for (let lag = 1; lag <= maxLag + 1; lag++) {
    running += d[lag];
    d[lag] = running > 0 ? (d[lag] * lag) / running : 1;
  }

  let best = -1;
  for (let lag = minLag; lag <= maxLag; lag++) {
    if (d[lag] < 0.15) {
      while (lag < maxLag && d[lag + 1] < d[lag]) lag++;
      best = lag;
      break;
    }
  }
  if (best < 0) return 0;
  const a = d[best - 1], b = d[best], c = d[best + 1];
  const denom = a - 2 * b + c;
  const shift = denom > 0 ? Math.max(-1, Math.min(1, (0.5 * (a - c)) / denom)) : 0;
  return sr / (best + shift);
}

// ---------------------------------------------------------------- the browser tuner

function setUpTuner() {
  const root = document.querySelector(".tuner");
  const pick = document.getElementById("instrument");
  const refInput = document.getElementById("reference");
  const button = document.getElementById("listen");
  if (!root || !pick || !button) return;

  let group = null, groupName = "";
  INSTRUMENTS.forEach(([name, g], i) => {
    if (g !== groupName) {
      groupName = g;
      group = document.createElement("optgroup");
      group.label = g;
      pick.appendChild(group);
    }
    const o = document.createElement("option");
    o.value = String(i);
    o.textContent = name;
    group.appendChild(o);
  });
  pick.value = "2";                 // violin

  const stringsBox = document.getElementById("strings");
  const showStrings = () => {
    stringsBox.textContent = "";
    const a4 = Number(refInput.value) || 440;
    for (const midi of INSTRUMENTS[Number(pick.value)][2]) {
      const s = document.createElement("div");
      s.className = "string";
      s.innerHTML = `<b>${noteName(midi)}</b><span>${hzFor(midi, a4).toFixed(hzFor(midi, a4) < 100 ? 2 : 1)} Hz</span>`;
      stringsBox.appendChild(s);
    }
  };
  pick.addEventListener("change", showStrings);
  refInput.addEventListener("change", showStrings);
  showStrings();

  const noteEl = document.getElementById("note-name");
  const octEl = document.getElementById("note-octave");
  const needle = document.getElementById("needle");
  const strobe = document.getElementById("strobe");
  const reading = document.getElementById("reading");

  let ctx = null, analyser = null, stream = null, data = null, timer = 0, strobePos = 0, cents = 0, shownNote = null;

  const stop = () => {
    clearInterval(timer);
    if (stream) stream.getTracks().forEach((t) => t.stop());
    if (ctx) ctx.close();
    ctx = analyser = stream = null;
    button.textContent = "START";
    delete root.dataset.state;
    reading.textContent = "press START and play a string";
  };

  const tick = () => {
    analyser.getFloatTimeDomainData(data);
    let energy = 0;
    for (const v of data) energy += v * v;
    if (Math.sqrt(energy / data.length) < 0.004) {
      delete root.dataset.state;
      reading.textContent = "listening...";
      return;
    }
    const [, , strings] = INSTRUMENTS[Number(pick.value)];
    const a4 = Number(refInput.value) || 440;
    const minHz = strings.length ? hzFor(Math.min(...strings) - 7, a4) : 30;
    const maxHz = strings.length ? hzFor(Math.max(...strings) + 7, a4) : 4200;
    const hz = yin(data, ctx.sampleRate, Math.max(30, minHz), maxHz);
    if (!hz) return;

    const midi = midiFor(hz, a4);
    let target = Math.round(midi), lit = -1;
    if (strings.length) {
      lit = 0;
      strings.forEach((s, i) => { if (Math.abs(midi - s) < Math.abs(midi - strings[lit])) lit = i; });
      target = strings[lit];
    }
    const c = (midi - target) * 100;
    cents = shownNote === target ? cents + 0.4 * (c - cents) : c;
    shownNote = target;

    const name = noteName(target);
    noteEl.textContent = name.replace(/-?\d+$/, "");
    octEl.textContent = name.match(/-?\d+$/)[0];
    needle.style.left = `${50 + Math.max(-50, Math.min(50, cents))}%`;
    const a = Math.abs(cents);
    root.dataset.state = a < IN_TUNE ? "in" : a < 15 ? "near" : "far";
    reading.textContent = `${hz.toFixed(hz < 100 ? 2 : 1)} Hz  ·  ${cents >= 0 ? "+" : ""}${cents.toFixed(1)} cents  ·  ` +
      (a < IN_TUNE ? "in tune" : cents < 0 ? "tune up" : "tune down");
    strobePos += Math.max(-60, Math.min(60, cents)) * 0.6;
    strobe.style.backgroundPosition = `${strobePos}px 0`;
    [...stringsBox.children].forEach((el, i) => {
      el.classList.toggle("lit", i === lit);
      el.classList.toggle("in", i === lit && a < IN_TUNE);
    });
  };

  button.addEventListener("click", async () => {
    if (ctx) { stop(); return; }
    try {
      stream = await navigator.mediaDevices.getUserMedia({
        audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false },
      });
    } catch (e) {
      reading.textContent = "no microphone (or permission denied)";
      return;
    }
    ctx = new AudioContext();
    analyser = ctx.createAnalyser();
    analyser.fftSize = 8192;
    data = new Float32Array(analyser.fftSize);
    ctx.createMediaStreamSource(stream).connect(analyser);
    button.textContent = "STOP";
    reading.textContent = "listening...";
    timer = setInterval(tick, 66);
  });
}

listInstruments();
setUpTuner();
