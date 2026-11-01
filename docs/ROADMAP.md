# Nordend — ROADMAP

Ordine di sviluppo, un passo alla volta. Ogni passo ha un criterio **Fatto quando** verificabile:
si passa al successivo solo quando quello corrente è chiuso e testato dall'autore.
Il perché delle scelte è in [`DESIGN.md`](DESIGN.md); cosa è stato fatto, misurato e deciso passo
per passo in [`devlog.md`](devlog.md); l'elenco completo delle funzioni in
[`features.md`](features.md).

Legenda: `[ ]` da fare · `[~]` in corso · `[x]` fatto

---

## Fatto (fasi 0-6 e 7.0-7.5, 2026-10-26 → 2026-10-31)

Ogni passo è stato chiuso con il suo "Fatto quando" e testato dall'autore; i criteri originali e i
dettagli sono nella cronologia git di questo file e nel devlog.

- [x] **Fase 0 — Fondamenta:** DESIGN approvato, repository, scheletro CMake.
- [x] **Fase 1 — Finestra e rendering:** finestra Wayland, EGL + GLES, scala frazionaria,
  primitive di disegno, testo (HarfBuzz, fallback dei font).
- [x] **Fase 2 — Input, loop e tema:** tastiera, puntatore, loop con `poll()` unico e worker pool
  con priorità e annullamento, config TOML, tema Noctalia con ricarica a caldo.
- [x] **Fase 3 — File system:** caricamento asincrono, tipi MIME identici a GLib, watcher
  inotify, icone per tipo.
- [x] **Fase 4 — Viste:** lista virtualizzata, colonne (Miller), selezione, griglia.
- [x] **Fase 5 — Miniature e Quick Look:** decodifica immagini (wuffs, WebP, JXL), cache
  miniature freedesktop, Quick Look, RAW dalle anteprime incorporate, anteprime di testo,
  vista dettagli, pannello info con anteprima, barra laterale, breadcrumb, barra di stato e
  scorciatoie (F1).
- [x] **Fase 6 — Operazioni sui file:** apertura con l'app predefinita e "Apri con", nuova
  cartella e rinomina, copia/sposta con progresso e conflitti, memoria misurata e restituita,
  cestino freedesktop, menu contestuale, cestino nella barra laterale con ripristino, svuota
  cestino ed eliminazione definitiva, clipboard di sistema, annulla nel campo del nome,
  annulla/rifai dell'ultima operazione.
- [x] **7.0 Navigazione:** cronologia (Alt+←/→, tasti laterali del mouse, cursore rimesso per
  nome, 100 passi), file nascosti (Ctrl+H, nascosti di default).
- [x] **7.1 Drag and drop:** dentro Nordend e con le altre app, su cartelle, spazio vuoto,
  breadcrumb e barra laterale; stesso disco sposta, altro disco copia (Ctrl/Shift forzano);
  sidecar, conflitti e Ctrl+Z come per la copia.
- [x] **7.2 Preferiti nella barra laterale:** Ctrl+D, "Add to Sidebar", trascinando; scritti nei
  segnalibri GTK, visibili anche nelle finestre "Apri file" delle altre app.
- [x] **7.3 Dischi rimovibili:** udisks2 via sdbus-c++; monta con un clic, espelle (smonta e
  spegne), segue collegamenti e distacchi.
- [x] **7.4 Compatibilità:** icone Tabler incorporate, cursore xcursor di riserva, decorazioni
  client-side, colori del desktop dal portale (chiaro/scuro e accento); provato su Umbriel,
  GNOME, Plasma (VM) e niri (annidato). Risultati in `compatibility.md`.
- [x] **7.5 Stato ricordato e informazioni:** `state.toml` (vista, ordinamento, nascosti, barra
  laterale, pannello info, colonne dei Dettagli, dimensione della finestra); forme del
  puntatore (manina e cursore di testo); `nordend --about` per le segnalazioni di bug. Il
  pannello impostazioni (7.5c) è stato fatto e poi ritirato il 2026-10-31 per scelta
  dell'autore: Nordend parte pronto (font di sistema, colori del desktop) e le poche
  impostazioni stanno in `~/.config/nordend/config.toml`, ricaricato a caldo.
- [x] **7.6 Benchmark degli obiettivi del DESIGN** (2026-11-01, tutti raggiunti): avvio a freddo
  128 ms (< 150), cartella da 10k con le prime voci in 0,7 ms (< 50), Quick Look di un ORF
  82-96 ms (< 100, il margine più stretto), scroll con il frame peggiore a 3,7 ms su 13,3 (75 Hz).
  Nuovo strumento `nordend-bench-start`; risultati in `benchmarks.md`.

Rimandati: **0.4 Pacchetto xbps-src** (con il rilascio, 7.8); **2.6 PR del template a Noctalia**
(dopo il rilascio: il template resta in `void-noctalia`). Da guardare alla prossima accensione
della VM: `--about` su GNOME e Plasma, la finestra che torna alla sua misura dopo "massimizzata".

---

## Da fare: fine dell'MVP

- [ ] **7.7 Uso quotidiano per una settimana**, bug annotati e chiusi (il primo vero test:
  scaricare ed elaborare foto, spostare documenti). Con un registro della memoria per tutta la
  settimana (chiesto dall'autore il 2026-10-30): ora e RSS di `nordend` ogni 10 minuti in un
  file, letti insieme alla fine; se la curva sale e non scende, il report di `pkill -USR1`.
- [ ] **7.8 Rilascio 0.1.0** (con il pacchetto xbps-src)

---

## Dopo l'MVP

- [ ] **8** Palette comandi (Ctrl+K) e ricerca fuzzy
- [ ] **9** Schede, doppio pannello, coda operazioni con cronologia degli annulla, rinomina multipla
- [ ] **10** Culling fotografico (stelle, etichette colore, XMP darktable, confronto, EXIF, istogramma)
- (**11** Dischi rimovibili: anticipato a 7.3)
- [ ] **12** Scripting Luau
- [ ] **13** Backend portal FileChooser
- [ ] **14** Editor esterno (helix), vedi appendice del DESIGN
- [ ] **15** Quick Look del testo più ricco: evidenziazione della sintassi, selezione e copia del testo nella preview
- [ ] **16** Traduzioni (chiesto dall'autore il 2026-10-31; fino ad allora solo inglese): un
  catalogo di stringhe di Nordend, senza gettext né librerie nuove, scelto da `LC_ALL`, poi
  `LC_MESSAGES`, poi `LANG`; l'italiano come prima traduzione, l'inglese come riserva per ogni
  stringa mancante. Fatto quando: con `LANG=it_IT.UTF-8` menu, schede, barra di stato, messaggi
  e overlay delle scorciatoie sono in italiano, con `LANG=C` in inglese; nessun testo tagliato
  (snapshot delle schede e dei menu in tutte e due le lingue); un test verifica che ogni stringa
  del catalogo abbia la sua traduzione italiana.
- Piccoli rimandi decisi durante la fase 7: spring-loading nel drag and drop (aprire una
  cartella tenendoci sopra il puntatore); riordino e rinomina dei preferiti; volumi LUKS.
