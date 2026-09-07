/* HapticPad Studio — Frontend.
   Kein Framework: der Zustand liegt in S, jede Aenderung rendert den betroffenen
   Bereich neu. Bei drei Reitern und einem 15x15-Raster reicht das voellig. */

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

const S = {
  meta: null,
  config: { Settings: {}, Profiles: [] },
  activeProfile: 0,
  view: 'xml',
  preview: { xml: '', yaml: '', notes: [] },
  size: 15,
  pixels: [],
  source: 'text',
  imageData: null,
  mdiSelected: null,
  shapeSelected: null,
  catalog: [],
  sd: { root: '', profiles: [] },
  keycodeOptions: '',
};

// --- Basis ------------------------------------------------------------------

async function api(path, body) {
  const options = body === undefined
    ? {}
    : { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) };
  const response = await fetch(path, options);
  const data = await response.json().catch(() => ({ error: 'Antwort war kein JSON' }));
  if (!response.ok) throw new Error(data.error || ('HTTP ' + response.status));
  return data;
}

let toastTimer = null;
function toast(message, isError) {
  const box = $('#toast');
  box.textContent = message;
  box.classList.toggle('err', !!isError);
  box.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => box.classList.remove('show'), isError ? 7000 : 3500);
}

function el(tag, attrs = {}, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(attrs)) {
    if (value === null || value === undefined || value === false) continue;
    if (key === 'class') node.className = value;
    else if (key === 'html') node.innerHTML = value;
    else if (key.startsWith('on')) node.addEventListener(key.slice(2).toLowerCase(), value);
    else if (key === 'value') node.value = value;
    else if (value === true) node.setAttribute(key, '');
    else node.setAttribute(key, value);
  }
  for (const child of children.flat()) {
    if (child === null || child === undefined || child === false) continue;
    node.append(child.nodeType ? child : document.createTextNode(String(child)));
  }
  return node;
}

const blank = (size) => Array.from({ length: size }, () => '0'.repeat(size));

function drawPixels(canvas, pixels) {
  const size = pixels.length;
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#05070a';
  ctx.fillRect(0, 0, size, size);
  ctx.fillStyle = '#d8f0ff';
  pixels.forEach((row, y) => {
    for (let x = 0; x < row.length; x += 1) if (row[x] === '1') ctx.fillRect(x, y, 1, 1);
  });
}

function pixelCanvas(pixels, cls) {
  const canvas = el('canvas', cls ? { class: cls } : {});
  drawPixels(canvas, pixels && pixels.length ? pixels : blank(15));
  return canvas;
}

// --- Reiter -----------------------------------------------------------------

$('#tabs').addEventListener('click', (event) => {
  const tab = event.target.closest('.tab');
  if (!tab) return;
  $$('.tab').forEach((t) => t.classList.toggle('is-active', t === tab));
  $$('.tabpanel').forEach((p) => p.classList.toggle('is-active', p.id === 'tab-' + tab.dataset.tab));
  if (tab.dataset.tab === 'catalog') { loadCatalog(); loadSd(); }
});

// --- Settings ---------------------------------------------------------------

function settingValue(name) {
  const value = S.config.Settings[name];
  return value === undefined ? S.meta.defaults.settings[name] : value;
}

function setSetting(name, value) {
  S.config.Settings[name] = value;
  schedulePreview();
}

function renderNumberField(spec) {
  const value = settingValue(spec.name);
  const isInt = spec.kind === 'int';
  const step = isInt ? 1 : (spec.step || 0.05);

  const number = el('input', {
    type: 'number', value, min: spec.min, max: spec.max, step,
  });
  const range = el('input', {
    type: 'range', value, min: spec.min, max: spec.max, step,
  });

  const commit = (raw) => {
    let next = isInt ? Math.round(Number(raw)) : Number(raw);
    if (Number.isNaN(next)) next = spec.default;
    next = Math.min(spec.max, Math.max(spec.min, next));
    number.value = next;
    range.value = next;
    setSetting(spec.name, next);
  };
  range.addEventListener('input', (e) => commit(e.target.value));
  number.addEventListener('change', (e) => commit(e.target.value));

  return el('label', { class: 'field' },
    el('span', {}, spec.label + (spec.legacy ? ' (alt)' : '')),
    el('div', { class: 'slider' }, range, number),
    spec.help ? el('small', { class: 'hint' }, spec.help) : null);
}

function renderRgbField(spec) {
  const value = settingValue(spec.name).slice();
  const toHex = (rgb) => '#' + rgb.map((v) => Number(v).toString(16).padStart(2, '0')).join('');

  const picker = el('input', { type: 'color', value: toHex(value) });
  const parts = value.map((component, index) => {
    const input = el('input', { type: 'number', min: 0, max: 255, value: component });
    input.addEventListener('change', () => {
      value[index] = Math.min(255, Math.max(0, Math.round(Number(input.value) || 0)));
      input.value = value[index];
      picker.value = toHex(value);
      setSetting(spec.name, value.slice());
    });
    return input;
  });

  picker.addEventListener('input', () => {
    const hex = picker.value;
    for (let i = 0; i < 3; i += 1) {
      value[i] = parseInt(hex.slice(1 + i * 2, 3 + i * 2), 16);
      parts[i].value = value[i];
    }
    setSetting(spec.name, value.slice());
  });

  return el('label', { class: 'field' },
    el('span', {}, spec.label),
    el('div', { class: 'rgbrow' }, picker, ...parts),
    spec.help ? el('small', { class: 'hint' }, spec.help) : null);
}

function renderEnumField(spec) {
  const select = el('select', {},
    ...spec.options.map((option) => el('option', { value: option }, option)));
  select.value = settingValue(spec.name);
  const help = el('small', { class: 'hint' }, S.meta.ledModeHelp[select.value] || spec.help || '');
  select.addEventListener('change', () => {
    setSetting(spec.name, select.value);
    help.textContent = S.meta.ledModeHelp[select.value] || spec.help || '';
  });
  return el('label', { class: 'field' }, el('span', {}, spec.label), select, help);
}

function renderModeListField(spec) {
  const chosen = settingValue(spec.name).slice();
  const rows = spec.options.map((mode) => {
    const box = el('input', { type: 'checkbox' });
    box.checked = chosen.includes(mode);
    box.addEventListener('change', () => {
      const next = spec.options.filter((m) => (m === mode ? box.checked : chosen.includes(m)));
      chosen.length = 0;
      chosen.push(...next);
      setSetting(spec.name, next);
    });
    return el('label', {}, box, mode, el('small', {}, S.meta.ledModeHelp[mode] || ''));
  });
  return el('div', { class: 'field' },
    el('span', {}, spec.label),
    el('div', { class: 'modelist' }, ...rows),
    spec.help ? el('small', { class: 'hint' }, spec.help) : null);
}

function renderCsvField(spec) {
  const input = el('input', { type: 'text', value: settingValue(spec.name) });
  input.addEventListener('change', () => setSetting(spec.name, input.value));
  return el('label', { class: 'field' },
    el('span', {}, spec.label), input,
    spec.help ? el('small', { class: 'hint' }, spec.help) : null);
}

function renderSettings() {
  const host = $('#settingsForm');
  host.textContent = '';

  const groups = [];
  for (const spec of S.meta.settings) {
    let group = groups.find((g) => g.name === spec.group);
    if (!group) { group = { name: spec.group, specs: [] }; groups.push(group); }
    group.specs.push(spec);
  }

  for (const group of groups) {
    const body = el('div', {});
    for (const spec of group.specs) {
      if (spec.kind === 'rgb') body.append(renderRgbField(spec));
      else if (spec.kind === 'enum') body.append(renderEnumField(spec));
      else if (spec.kind === 'modelist') body.append(renderModeListField(spec));
      else if (spec.kind === 'csv') body.append(renderCsvField(spec));
      else body.append(renderNumberField(spec));
    }
    const details = el('details', { class: 'group' }, el('summary', {}, group.name), body);
    if (group.name !== 'Legacy-PID') details.open = true;
    host.append(details);
  }
}

// --- Profile ----------------------------------------------------------------

function keycodeSelect(value, onChange) {
  const select = el('select', { html: S.keycodeOptions });
  select.value = String(value || 0);
  select.addEventListener('change', () => onChange(Number(select.value)));
  return select;
}

function sdIconFor(profileName, slot) {
  const profile = S.sd.profiles.find((p) => p.name === profileName);
  if (!profile) return null;
  const entry = profile.slots.find((s) => s.slot === slot);
  return entry && entry.pixels ? entry.pixels : null;
}

function renderProfileTabs() {
  const host = $('#profileTabs');
  host.textContent = '';
  S.config.Profiles.forEach((profile, index) => {
    const tab = el('button', {
      class: 'ptab' + (index === S.activeProfile ? ' is-active' : ''),
      onClick: () => { S.activeProfile = index; renderProfiles(); },
    }, profile.name || '(ohne Namen)');
    host.append(tab);
  });
  $('#profileCount').textContent = S.config.Profiles.length;
}

function renderButtonCard(profile, button, index) {
  const icon = sdIconFor(profile.name, index + 1);
  const preview = el('canvas', { class: 'iconslot' + (icon ? '' : ' missing'), title: icon
    ? 'Icon aus ' + profile.name + '/' + (index + 1) + '.bmp'
    : 'Kein Icon in ' + profile.name + '/' + (index + 1) + '.bmp' });
  drawPixels(preview, icon || blank(15));
  preview.addEventListener('click', () => {
    if (icon) { S.pixels = icon.slice(); S.size = icon.length; renderEditor(); }
    $$('.tab').find((t) => t.dataset.tab === 'bmp').click();
    $('#assignProfile').value = profile.name;
    $('#assignSlot').value = String(index + 1);
  });

  const label = el('input', { type: 'text', value: button.label || '' });
  label.addEventListener('input', () => { button.label = label.value; schedulePreview(); });

  const card = el('div', { class: 'buttoncard' },
    el('header', {}, preview, el('b', {}, 'Taste ' + (index + 1)), label));

  for (let i = 0; i < S.meta.actionsPerButton; i += 1) {
    if (!button.actions[i]) button.actions[i] = [0, 0];
    const action = button.actions[i];
    const delay = el('input', { type: 'number', min: 0, max: 255, value: action[0], title: 'Delay in ms vor dieser Aktion' });
    delay.addEventListener('change', () => {
      action[0] = Math.min(255, Math.max(0, Math.round(Number(delay.value) || 0)));
      delay.value = action[0];
      schedulePreview();
    });
    card.append(el('div', { class: 'actionrow' }, delay,
      keycodeSelect(action[1], (code) => { action[1] = code; schedulePreview(); })));
  }

  const domainToggle = el('input', { type: 'checkbox' });
  domainToggle.checked = !!button.wheelMode;
  domainToggle.addEventListener('change', () => {
    if (domainToggle.checked) {
      button.wheelMode = profile.WheelMode;
    } else {
      button.wheelMode = null;
      button.wheelUp = null;
      button.wheelDown = null;
    }
    renderProfiles();
    schedulePreview();
  });

  const domain = el('div', { class: 'domainbox' },
    el('label', { class: 'check' }, domainToggle, 'Wheel-Domain'));

  if (button.wheelMode) {
    const modeSelect = el('select', {},
      ...S.meta.wheelModes.map((mode) => el('option', { value: mode }, mode)));
    modeSelect.value = button.wheelMode;
    modeSelect.addEventListener('change', () => {
      button.wheelMode = modeSelect.value;
      schedulePreview();
    });
    domain.append(el('label', { class: 'field' }, el('span', {}, 'Modus beim Druck'), modeSelect));

    for (const [key, caption] of [['wheelUp', 'Tick hoch'], ['wheelDown', 'Tick runter']]) {
      const pair = button[key] || [0, 0];
      domain.append(el('label', { class: 'field' }, el('span', {}, caption),
        keycodeSelect(pair[1], (code) => {
          button[key] = code ? [pair[0] || 0, code] : null;
          schedulePreview();
        })));
    }
    domain.append(el('small', { class: 'hint' },
      'Ohne Tasten scrollt das Rad normal, nur mit der Haptik dieses Modus.'));
  }

  card.append(domain);
  return card;
}

function renderProfiles() {
  renderProfileTabs();
  const host = $('#profileEditor');
  host.textContent = '';

  if (!S.config.Profiles.length) {
    host.append(el('div', { class: 'empty' }, 'Noch kein Profil. Lege eins mit „+ Profil“ an.'));
    return;
  }
  if (S.activeProfile >= S.config.Profiles.length) S.activeProfile = 0;
  const profile = S.config.Profiles[S.activeProfile];

  const name = el('input', { type: 'text', value: profile.name });
  name.addEventListener('input', () => { profile.name = name.value; renderProfileTabs(); schedulePreview(); });

  const mode = el('select', {}, ...S.meta.wheelModes.map((m) => el('option', { value: m }, m)));
  mode.value = profile.WheelMode;
  const modeHelp = el('small', { class: 'hint' }, S.meta.wheelModeHelp[profile.WheelMode] || '');
  mode.addEventListener('change', () => {
    profile.WheelMode = mode.value;
    modeHelp.textContent = S.meta.wheelModeHelp[mode.value] || '';
    schedulePreview();
  });

  const actions = el('div', { class: 'rowbtns' },
    el('button', {
      class: 'btn ghost', title: 'Profil nach links schieben',
      onClick: () => moveProfile(-1),
    }, '←'),
    el('button', {
      class: 'btn ghost', title: 'Profil nach rechts schieben',
      onClick: () => moveProfile(1),
    }, '→'),
    el('button', {
      class: 'btn ghost',
      onClick: () => {
        const copy = JSON.parse(JSON.stringify(profile));
        copy.name = profile.name + ' Kopie';
        S.config.Profiles.splice(S.activeProfile + 1, 0, copy);
        S.activeProfile += 1;
        renderProfiles(); schedulePreview();
      },
    }, 'Duplizieren'),
    el('button', {
      class: 'btn danger',
      onClick: () => {
        if (!confirm('Profil „' + profile.name + '“ loeschen?')) return;
        S.config.Profiles.splice(S.activeProfile, 1);
        S.activeProfile = Math.max(0, S.activeProfile - 1);
        renderProfiles(); schedulePreview(); renderAssignTargets();
      },
    }, 'Loeschen'));

  host.append(el('div', { class: 'profilehead' },
    el('label', { class: 'field' }, el('span', {}, 'Name — zugleich der Icon-Ordner auf der SD-Karte'), name),
    el('label', { class: 'field' }, el('span', {}, 'WheelKey (gehalten beim Drehen)'),
      keycodeSelect(profile.WheelKey, (code) => { profile.WheelKey = code; schedulePreview(); })),
    el('label', { class: 'field' }, el('span', {}, 'Wheel-Modus'), mode, modeHelp),
    actions));

  const grid = el('div', { class: 'buttongrid' });
  for (let i = 0; i < S.meta.buttonsPerProfile; i += 1) {
    if (!profile.buttons[i]) profile.buttons[i] = JSON.parse(JSON.stringify(S.meta.defaults.button));
    grid.append(renderButtonCard(profile, profile.buttons[i], i));
  }
  host.append(grid);
}

function moveProfile(delta) {
  const target = S.activeProfile + delta;
  if (target < 0 || target >= S.config.Profiles.length) return;
  const [profile] = S.config.Profiles.splice(S.activeProfile, 1);
  S.config.Profiles.splice(target, 0, profile);
  S.activeProfile = target;
  renderProfiles();
  schedulePreview();
}

$('#btnAddProfile').addEventListener('click', () => {
  const profile = JSON.parse(JSON.stringify(S.meta.defaults.profile));
  profile.name = 'Profil ' + (S.config.Profiles.length + 1);
  S.config.Profiles.push(profile);
  S.activeProfile = S.config.Profiles.length - 1;
  renderProfiles();
  renderAssignTargets();
  schedulePreview();
});

// --- Vorschau und Speichern -------------------------------------------------

let previewTimer = null;
function schedulePreview() {
  clearTimeout(previewTimer);
  previewTimer = setTimeout(refreshPreview, 200);
}

async function refreshPreview() {
  try {
    S.preview = await api('/api/config/preview', { config: S.config });
  } catch (error) {
    toast(error.message, true);
    return;
  }
  $('#xmlPreview').textContent = S.view === 'xml' ? S.preview.xml : S.preview.yaml;
  const notes = $('#notes');
  notes.textContent = '';
  for (const note of S.preview.notes) notes.append(el('div', { class: 'note' }, note));
}

$$('.viewswitch .pill').forEach((pill) => pill.addEventListener('click', () => {
  S.view = pill.dataset.view;
  $$('.viewswitch .pill').forEach((p) => p.classList.toggle('is-active', p === pill));
  $('#xmlPreview').textContent = S.view === 'xml' ? S.preview.xml : S.preview.yaml;
}));

$('#btnCopyXml').addEventListener('click', async () => {
  await navigator.clipboard.writeText(S.view === 'xml' ? S.preview.xml : S.preview.yaml);
  toast(S.view.toUpperCase() + ' in der Zwischenablage.');
});

$('#btnSave').addEventListener('click', async () => {
  const targets = $$('.savetarget').filter((box) => box.checked).map((box) => box.value);
  if (!targets.length) { toast('Kein Ziel ausgewaehlt.', true); return; }
  try {
    const result = await api('/api/config/save', { config: S.config, targets });
    toast('Geschrieben:\n' + result.written.join('\n'));
  } catch (error) {
    toast(error.message, true);
  }
});

$('#btnLoadRepo').addEventListener('click', () => loadConfig());
$('#btnImportFile').addEventListener('click', () => $('#fileImport').click());
$('#fileImport').addEventListener('change', async (event) => {
  const file = event.target.files[0];
  if (!file) return;
  try {
    const result = await api('/api/config/import', { text: await file.text() });
    S.config = result.config;
    S.activeProfile = 0;
    renderSettings(); renderProfiles(); renderAssignTargets(); refreshPreview();
    toast(file.name + ' geladen.');
  } catch (error) {
    toast(error.message, true);
  }
  event.target.value = '';
});

async function loadConfig() {
  const result = await api('/api/config');
  S.config = result.config;
  S.activeProfile = 0;
  renderSettings();
  renderProfiles();
  renderAssignTargets();
  refreshPreview();
  if (!result.existed) toast('Keine config.xml gefunden — mit Standardwerten gestartet.');
}

// --- BMP-Editor -------------------------------------------------------------

function renderPreviews() {
  drawPixels($('#preview1x'), S.pixels);
  const row = $('#previewRow');
  row.textContent = '';
  for (let i = 0; i < 6; i += 1) {
    row.append(pixelCanvas(i === 2 ? S.pixels : blank(S.size)));
  }
}

function renderEditor() {
  const grid = $('#pixelGrid');
  grid.style.gridTemplateColumns = 'repeat(' + S.size + ', 22px)';
  grid.textContent = '';

  S.pixels.forEach((row, y) => {
    for (let x = 0; x < S.size; x += 1) {
      const cell = el('div', { class: 'px' + (row[x] === '1' ? ' on' : '') });
      cell.dataset.x = x;
      cell.dataset.y = y;
      grid.append(cell);
    }
  });

  renderPreviews();
}

function setPixel(x, y, on) {
  if (S.pixels[y][x] === (on ? '1' : '0')) return;
  const row = S.pixels[y].split('');
  row[x] = on ? '1' : '0';
  S.pixels[y] = row.join('');
  // Nur die eine Zelle anfassen: beim Ziehen wuerde ein Neuaufbau des Rasters
  // das Element unter dem Zeiger austauschen und den Zug abreissen lassen.
  const cell = $('#pixelGrid').children[y * S.size + x];
  if (cell) cell.classList.toggle('on', on);
  renderPreviews();
}

let painting = null;
$('#pixelGrid').addEventListener('pointerdown', (event) => {
  const cell = event.target.closest('.px');
  if (!cell) return;
  const x = Number(cell.dataset.x);
  const y = Number(cell.dataset.y);
  painting = S.pixels[y][x] !== '1';
  setPixel(x, y, painting);
  event.preventDefault();
});
$('#pixelGrid').addEventListener('pointerover', (event) => {
  if (painting === null) return;
  const cell = event.target.closest('.px');
  if (!cell) return;
  setPixel(Number(cell.dataset.x), Number(cell.dataset.y), painting);
});
window.addEventListener('pointerup', () => { painting = null; });

$('#btnClear').addEventListener('click', () => { S.pixels = blank(S.size); renderEditor(); });

$$('.toolrow .btn').forEach((btn) => btn.addEventListener('click', async () => {
  const op = btn.dataset.op;
  const post = {
    invert: op === 'invert',
    flipH: op === 'flipH',
    flipV: op === 'flipV',
    rotate: op === 'rotate' ? 90 : 0,
    shiftX: op === 'left' ? -1 : op === 'right' ? 1 : 0,
    shiftY: op === 'up' ? -1 : op === 'down' ? 1 : 0,
  };
  try {
    const result = await api('/api/bmp/render', { source: 'pixels', pixels: S.pixels, size: S.size, post });
    S.pixels = result.pixels;
    renderEditor();
  } catch (error) {
    toast(error.message, true);
  }
}));

$('#bmpSource').addEventListener('click', (event) => {
  const seg = event.target.closest('.seg');
  if (!seg) return;
  S.source = seg.dataset.src;
  $$('.seg').forEach((s) => s.classList.toggle('is-active', s === seg));
  $$('.srcpane').forEach((p) => p.classList.toggle('is-active', p.dataset.pane === S.source));
  if (S.source !== 'shape') renderFromSource();
});

async function renderFromSource() {
  const body = { source: S.source, size: S.size };

  if (S.source === 'text') {
    Object.assign(body, {
      text: $('#txtText').value,
      fontPath: $('#txtFont').value,
      fontSize: Number($('#txtSize').value) || 0,
      bold: $('#txtBold').checked,
      threshold: Number($('#txtThreshold').value),
    });
    if (!body.text) { S.pixels = blank(S.size); renderEditor(); return; }
  } else if (S.source === 'image') {
    if (!S.imageData) return;
    Object.assign(body, {
      data: S.imageData,
      threshold: Number($('#imgThreshold').value),
      gamma: Number($('#imgGamma').value),
      fit: $('#imgFit').value,
      trim: $('#imgTrim').checked,
      dither: $('#imgDither').checked,
      invert: $('#imgInvert').checked,
    });
  } else if (S.source === 'mdi') {
    if (!S.mdiSelected) return;
    Object.assign(body, {
      icon: S.mdiSelected,
      fontSize: 0,
      threshold: Number($('#mdiThreshold').value),
    });
  } else if (S.source === 'shape') {
    if (!S.shapeSelected) return;
    body.shape = S.shapeSelected;
  }

  try {
    const result = await api('/api/bmp/render', body);
    S.pixels = result.pixels;
    renderEditor();
  } catch (error) {
    toast(error.message, true);
  }
}

['#txtText', '#txtFont', '#txtSize', '#txtBold', '#txtThreshold'].forEach((sel) => {
  const update = () => {
    $('#txtThresholdVal').textContent = $('#txtThreshold').value;
    renderFromSource();
  };
  $(sel).addEventListener('input', update);
  $(sel).addEventListener('change', update);
});

$('#mdiThreshold').addEventListener('input', () => {
  $('#mdiThresholdVal').textContent = $('#mdiThreshold').value;
  renderFromSource();
});

['#imgThreshold', '#imgGamma', '#imgFit', '#imgTrim', '#imgDither', '#imgInvert'].forEach((sel) => {
  $(sel).addEventListener('input', () => {
    $('#imgThresholdVal').textContent = $('#imgThreshold').value;
    $('#imgGammaVal').textContent = Number($('#imgGamma').value).toFixed(1);
    renderFromSource();
  });
});

$('#imgFile').addEventListener('change', (event) => {
  const file = event.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = () => { S.imageData = reader.result; renderFromSource(); };
  reader.readAsDataURL(file);
});

$('#bmpSize').addEventListener('change', () => {
  const next = Number($('#bmpSize').value);
  const old = S.pixels;
  S.size = next;
  S.pixels = blank(next).map((row, y) => {
    if (y >= old.length) return row;
    return (old[y] + '0'.repeat(next)).slice(0, next);
  });
  renderEditor();
  renderFromSource();
});

// --- MDI --------------------------------------------------------------------

async function loadMdi(query) {
  const result = await api('/api/mdi?limit=180&q=' + encodeURIComponent(query || ''));
  const ready = result.status.fontAvailable && result.status.metaAvailable;
  $('#mdiMissing').hidden = ready;
  $('#mdiReady').hidden = !ready;
  if (!ready) return;

  $('#mdiStatus').textContent = result.icons.length + ' von ' + result.status.count
    + ' Icons (MDI ' + result.status.version + ')';

  const grid = $('#mdiGrid');
  grid.textContent = '';
  for (const icon of result.icons) {
    const tile = el('div', { class: 'tile' + (icon.name === S.mdiSelected ? ' is-active' : '') },
      el('div', { class: 'mdiglyph' }, String.fromCodePoint(parseInt(icon.codepoint, 16))),
      el('span', { title: icon.name }, icon.name));
    tile.addEventListener('click', () => {
      S.mdiSelected = icon.name;
      $('#bmpName').value = icon.name;
      $$('.tile', grid).forEach((t) => t.classList.remove('is-active'));
      tile.classList.add('is-active');
      renderFromSource();
    });
    grid.append(tile);
  }
}

let mdiTimer = null;
$('#mdiQuery').addEventListener('input', () => {
  clearTimeout(mdiTimer);
  mdiTimer = setTimeout(() => loadMdi($('#mdiQuery').value), 220);
});

$('#btnInstallMdi').addEventListener('click', async (event) => {
  const btn = event.target;
  btn.disabled = true;
  btn.textContent = 'Laedt…';
  try {
    const result = await api('/api/mdi/install', {});
    toast('MDI installiert: ' + result.files.map((f) => f.file).join(', '));
    // Die Webfont steht jetzt auch dem Browser zur Verfuegung.
    const style = document.createElement('style');
    style.textContent = '@font-face{font-family:"Material Design Icons";'
      + 'src:url("/static/mdi.ttf") format("truetype");}';
    document.head.append(style);
    await loadMdi('');
  } catch (error) {
    toast('MDI konnte nicht geladen werden: ' + error.message, true);
  } finally {
    btn.disabled = false;
    btn.textContent = 'Von jsDelivr laden (Internet)';
  }
});

// --- Formen -----------------------------------------------------------------

function renderShapeGrid() {
  const grid = $('#shapeGrid');
  grid.textContent = '';
  for (const shape of S.meta.shapes) {
    const tile = el('div', { class: 'tile' }, pixelCanvas(shape.pixels), el('span', {}, shape.label));
    tile.addEventListener('click', () => {
      S.shapeSelected = shape.id;
      $('#bmpName').value = shape.label;
      $$('.tile', grid).forEach((t) => t.classList.remove('is-active'));
      tile.classList.add('is-active');
      renderFromSource();
    });
    grid.append(tile);
  }
}

// --- Speichern und Zuweisen -------------------------------------------------

$('#btnSaveCatalog').addEventListener('click', async () => {
  try {
    await api('/api/catalog/save', {
      pixels: S.pixels,
      size: S.size,
      name: $('#bmpName').value || 'Icon',
      tags: $('#bmpTags').value.split(',').map((t) => t.trim()).filter(Boolean),
      sourceLabel: S.source,
    });
    toast('Im Katalog gespeichert.');
    loadCatalog();
  } catch (error) {
    toast(error.message, true);
  }
});

$('#btnDownload').addEventListener('click', async () => {
  try {
    const result = await api('/api/bmp/download', {
      pixels: S.pixels, size: S.size, name: $('#bmpName').value || 'icon',
    });
    const link = el('a', {
      href: 'data:image/bmp;base64,' + result.data,
      download: result.name,
    });
    document.body.append(link);
    link.click();
    link.remove();
  } catch (error) {
    toast(error.message, true);
  }
});

function renderAssignTargets() {
  const select = $('#assignProfile');
  const previous = select.value;
  select.textContent = '';
  const names = new Set(S.config.Profiles.map((p) => p.name).filter(Boolean));
  S.sd.profiles.forEach((p) => names.add(p.name));
  for (const name of names) select.append(el('option', { value: name }, name));
  if (previous && names.has(previous)) select.value = previous;

  const slot = $('#assignSlot');
  if (!slot.options.length) {
    for (let i = 1; i <= 6; i += 1) slot.append(el('option', { value: String(i) }, 'Taste ' + i));
  }
  $('#assignHint').textContent = 'Schreibt nach ' + S.sd.root + '\\<Profil>\\<Taste>.bmp';
}

$('#btnAssign').addEventListener('click', async () => {
  try {
    const result = await api('/api/bmp/assign', {
      pixels: S.pixels,
      profile: $('#assignProfile').value,
      slot: Number($('#assignSlot').value),
    });
    toast('Geschrieben: ' + result.written);
    await loadSd();
    renderProfiles();
  } catch (error) {
    toast(error.message, true);
  }
});

// --- Katalog ----------------------------------------------------------------

async function loadCatalog() {
  const result = await api('/api/catalog');
  S.catalog = result.entries;
  renderCatalog();
}

function renderCatalog() {
  const query = $('#catalogSearch').value.trim().toLowerCase();
  const wall = $('#catalogWall');
  wall.textContent = '';

  const entries = S.catalog.filter((entry) => !query
    || entry.name.toLowerCase().includes(query)
    || entry.tags.some((tag) => tag.toLowerCase().includes(query)));

  $('#catalogCount').textContent = entries.length;

  if (!entries.length) {
    wall.append(el('div', { class: 'empty' }, S.catalog.length
      ? 'Nichts gefunden.'
      : 'Der Katalog ist leer. Erzeuge Icons im BMP-Generator oder lies die SD-Karte ein.'));
    return;
  }

  for (const entry of entries) {
    const canvas = pixelCanvas(entry.pixels);
    canvas.title = 'In den Editor uebernehmen';
    canvas.addEventListener('click', () => {
      S.pixels = entry.pixels.slice();
      S.size = entry.pixels.length;
      $('#bmpSize').value = String(S.size);
      $('#bmpName').value = entry.name;
      $('#bmpTags').value = entry.tags.join(', ');
      renderEditor();
      $$('.tab').find((t) => t.dataset.tab === 'bmp').click();
    });

    wall.append(el('div', { class: 'card' }, canvas,
      el('b', { title: entry.name }, entry.name),
      el('small', {}, entry.tags.join(', ') || entry.source),
      el('div', { class: 'rowbtns' },
        el('button', {
          class: 'btn ghost',
          onClick: async () => {
            const name = prompt('Neuer Name', entry.name);
            if (name === null) return;
            await api('/api/catalog/rename', { id: entry.id, name });
            loadCatalog();
          },
        }, 'Name'),
        el('button', {
          class: 'btn danger',
          onClick: async () => {
            if (!confirm('„' + entry.name + '“ loeschen?')) return;
            await api('/api/catalog/delete', { id: entry.id });
            loadCatalog();
          },
        }, 'X'))));
  }
}

$('#catalogSearch').addEventListener('input', renderCatalog);

$('#btnImportSd').addEventListener('click', async () => {
  try {
    const result = await api('/api/catalog/import-sd', {});
    toast(result.added + ' Icon(s) uebernommen.');
    loadCatalog();
  } catch (error) {
    toast(error.message, true);
  }
});

async function loadSd() {
  S.sd = await api('/api/sd');
  $('#sdPath').value = S.sd.root;
  renderAssignTargets();
  renderSdWall();
}

function renderSdWall() {
  const wall = $('#sdWall');
  wall.textContent = '';

  if (!S.sd.profiles.length) {
    wall.append(el('div', { class: 'empty' }, 'Keine Icon-Ordner unter ' + S.sd.root + '.'));
    return;
  }

  for (const profile of S.sd.profiles) {
    const slots = el('div', { class: 'sdslots' });
    for (const slot of profile.slots) {
      const canvas = pixelCanvas(slot.pixels || blank(15));
      canvas.title = slot.problem || 'In den Editor uebernehmen';
      canvas.addEventListener('click', () => {
        if (!slot.pixels) return;
        S.pixels = slot.pixels.slice();
        S.size = slot.pixels.length;
        renderEditor();
        $('#assignProfile').value = profile.name;
        $('#assignSlot').value = String(slot.slot);
        $$('.tab').find((t) => t.dataset.tab === 'bmp').click();
      });
      slots.append(el('div', { class: 'sdslot' + (slot.problem ? ' bad' : '') }, canvas,
        el('small', {}, slot.problem ? '⚠ ' + slot.slot : String(slot.slot))));
    }
    wall.append(el('div', { class: 'sdprofile' }, el('h3', {}, profile.name), slots));
  }
}

$('#btnSetSd').addEventListener('click', async () => {
  try {
    await api('/api/sd/root', { path: $('#sdPath').value });
    await loadSd();
    toast('SD-Ordner gesetzt.');
  } catch (error) {
    toast(error.message, true);
  }
});

// --- Start ------------------------------------------------------------------

function buildKeycodeOptions() {
  const groups = [];
  for (const entry of S.meta.keycodes) {
    let group = groups.find((g) => g.name === entry.group);
    if (!group) { group = { name: entry.group, items: [] }; groups.push(group); }
    group.items.push(entry);
  }
  S.keycodeOptions = groups.map((group) => '<optgroup label="' + group.name + '">'
    + group.items.map((item) => '<option value="' + item.code + '">' + item.name
      + (item.code ? ' (' + item.code + ')' : '') + '</option>').join('')
    + '</optgroup>').join('');
}

async function boot() {
  S.meta = await api('/api/meta');
  buildKeycodeOptions();

  $('#pathHint').textContent = S.meta.paths.repo;

  const sizeSelect = $('#bmpSize');
  for (const size of S.meta.sizes) {
    sizeSelect.append(el('option', { value: String(size) }, size + '×' + size));
  }
  S.size = S.meta.defaultSize;
  sizeSelect.value = String(S.size);
  S.pixels = blank(S.size);

  const fontSelect = $('#txtFont');
  fontSelect.append(el('option', { value: '' }, 'Pillow-Standard (5×8 Pixel)'));
  for (const font of S.meta.fonts) fontSelect.append(el('option', { value: font.path }, font.name));

  if (S.meta.mdi.fontAvailable) {
    const style = document.createElement('style');
    style.textContent = '@font-face{font-family:"Material Design Icons";'
      + 'src:url("/static/mdi.ttf") format("truetype");}';
    document.head.append(style);
  }

  renderEditor();
  await loadSd();
  await loadConfig();
  await loadCatalog();
  renderShapeGrid();
  loadMdi('');
  renderFromSource();
}

boot().catch((error) => toast('Start fehlgeschlagen: ' + error.message, true));
