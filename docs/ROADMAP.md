# Nordend — ROADMAP

Ordine di sviluppo, un passo alla volta. Ogni passo ha un criterio **Fatto quando** verificabile:
si passa al successivo solo quando quello corrente è chiuso e testato dall'autore.
Il perché delle scelte è in [`DESIGN.md`](DESIGN.md); cosa è stato fatto, misurato e deciso passo
per passo in [`devlog.md`](devlog.md); l'elenco completo delle funzioni in
[`features.md`](features.md).

Legenda: `[ ]` da fare · `[~]` in corso · `[x]` fatto

---

## Fatto (fasi 0-6, 2026-10-26 → 2026-10-30)

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

Rimandati: **0.4 Pacchetto xbps-src** (con il rilascio, 7.8); **2.6 PR del template a Noctalia**
(dopo il rilascio: il template resta in `void-noctalia`).

---

## Fase 7 — MVP

Ordine concordato con l'autore il 2026-10-30: prima ciò che si usa ogni giorno, poi i pezzi
grossi, alla fine compatibilità e rilascio. Il "Fatto quando" di ogni passo si propone
all'autore quando ci si arriva.

- [x] **7.0 Navigazione**
  Cronologia: Alt+← indietro, Alt+→ avanti, i tasti laterali del mouse (Backspace resta la
  cartella superiore). File nascosti: Ctrl+H li mostra o nasconde, nascosti di default.
  Fatto quando: ogni cambio della cartella aperta (nelle colonne: quella a fuoco) è un passo;
  indietro/avanti rimettono il cursore dov'era (per nome); aprire una cartella dopo "indietro"
  cancella il ramo "avanti"; al massimo 100 passi per finestra; una cartella sparita si salta con
  un messaggio. Ctrl+H vale per tutte le viste e il pannello info; da nascosti Ctrl+A, la ricerca
  per digitazione e il conteggio li ignorano e la barra di stato dice quanti sono; il cursore su un
  nascosto passa al visibile più vicino; una cartella nascosta aperta esplicitamente resta visibile
  nel genitore. Cronologia e Ctrl+H valgono per la sessione (salvati con la 7.5). Test su
  `Browser::key()`, snapshot delle tre viste, scorciatoie aggiornate, provato dall'autore con il
  mouse.

- [ ] **7.1 Drag and drop** (`wl_data_device`, come la clipboard)
  Dentro Nordend e con le altre app (per esempio verso Firefox e da Firefox): si rilascia su una
  cartella in ogni vista, sulla breadcrumb o su un luogo della barra laterale. Stesso disco =
  sposta, disco diverso = copia; Ctrl forza la copia, Shift lo spostamento. Il motore della 6.3
  (sidecar, conflitti, avanzamento) e Ctrl+Z valgono anche qui.

- [ ] **7.2 Preferiti nella barra laterale**
  Una cartella trascinata nella barra laterale, Ctrl+D o "Add to Sidebar" dal menu la aggiunge;
  il clic destro la toglie. Scritti nel file dei segnalibri GTK (`gtk-3.0/bookmarks`), così si
  vedono anche nelle finestre "Apri file" delle altre app.

- [ ] **7.3 Dischi rimovibili** (udisks2, installato)
  I dischi non montati nella barra laterale, un clic li monta; un'icona li espelle (smonta e
  spegne). La libreria D-Bus si sceglie a questo passo (librerie nuove: chiedere prima).

- [ ] **7.4 Compatibilità**
  Fuori da Noctalia e Umbriel: il font di icone Tabler distribuito con Nordend (codice altrui:
  chiedere prima, THIRD_PARTY); cursore di riserva (libwayland-cursor + tema xcursor) per i
  compositor senza cursor-shape; decorazioni client-side nostre dove xdg-decoration manca
  (GNOME/Mutter); checklist su Umbriel, niri (VM), KDE, GNOME (Ubuntu).
  Fatto quando: Nordend è usabile, con icone, finestra spostabile/chiudibile e cursore visibile,
  su tutti e quattro.

- [ ] **7.5 Impostazioni e stato ricordato** (Ctrl+,)
  Un pannello che mostra le impostazioni, poi modificabile (scrivendo `config.toml` senza perdere
  i commenti); lo stato ricordato tra un avvio e l'altro (vista, larghezze, barra laterale,
  ordinamento, file nascosti, pannello info); una sezione "Informazioni" (Nordend, distro,
  Noctalia, Umbriel, GPU).

- [ ] **7.6 Benchmark** di tutti gli obiettivi del DESIGN, risultati in `devlog.md`
- [ ] **7.7 Uso quotidiano per una settimana**, bug annotati e chiusi
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
