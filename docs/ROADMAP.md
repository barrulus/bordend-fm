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

- [x] **7.1 Drag and drop** (`wl_data_device`, come la clipboard)
  Dentro Nordend e con le altre app (per esempio verso Firefox e da Firefox): si rilascia su una
  cartella in ogni vista, sulla breadcrumb o su un luogo della barra laterale. Stesso disco =
  sposta, disco diverso = copia; Ctrl forza la copia, Shift lo spostamento. Il motore della 6.3
  (sidecar, conflitti, avanzamento) e Ctrl+Z valgono anche qui.
  Fatto quando: da ogni vista si trascina la selezione (o la voce sotto il puntatore) con
  un'immagine (icona del primo, numero se più di uno), offerta come `text/uri-list` e
  `text/plain`; verso le altre app solo copia. Si rilascia su una cartella (evidenziata), sullo
  spazio vuoto di una vista, sulla breadcrumb, su un luogo della barra laterale (il Cestino
  cestina). Rifiutati: una cartella in sé stessa o in una sua sottocartella, la cartella da cui
  vengono (niente), gli URL non file ("Only files can be dropped"). Scorrimento automatico ai
  bordi, Escape annulla. Test su azione, bersagli, rifiuti e `text/uri-list` (spazi, non ASCII);
  snapshot; provato dall'autore dentro Nordend (Ctrl/Shift, Ctrl+Z), con Firefox e un
  terminale. Lo spring-loading (aprire una cartella tenendoci sopra il puntatore) dopo l'MVP.

- [x] **7.2 Preferiti nella barra laterale**
  Una cartella trascinata nella barra laterale, Ctrl+D o "Add to Sidebar" dal menu la aggiunge;
  il clic destro la toglie. Scritti nel file dei segnalibri GTK (`gtk-3.0/bookmarks`), così si
  vedono anche nelle finestre "Apri file" delle altre app.
  Fatto quando: Ctrl+D aggiunge la cartella mostrata; "Add to Sidebar" nel menu di una cartella
  e dello spazio vuoto; trascinare cartelle sull'intestazione "Bookmarks" o nello spazio vuoto
  sotto i luoghi le aggiunge (linea di inserimento; con dei file in mezzo è rifiutato), mentre
  rilasciare su un luogo continua a spostare dentro; un doppione dà "Already in the sidebar";
  "Remove from Sidebar" sul clic destro di un preferito. Il file si scrive su un worker in modo
  atomico, senza toccare le altre righe (`sftp://`, etichette, ordine), con i nomi codificati come
  GTK, creando `gtk-3.0` se manca; un errore dà un messaggio. Test, snapshot, provato dall'autore
  anche nella finestra "Apri file" di un'app GTK. Riordino e rinomina dopo l'MVP.

- [x] **7.3 Dischi rimovibili** (udisks2, installato)
  I dischi non montati nella barra laterale, un clic li monta; un'icona li espelle (smonta e
  spegne). La libreria D-Bus si sceglie a questo passo (librerie nuove: chiedere prima).
  Fatto quando: i dischi collegati e non montati compaiono in "DEVICES" (attenuati, icona
  diversa; partizioni con un filesystem che udisks non segna di sistema né da ignorare; nome =
  etichetta, senza "466 GB Volume") e la lista segue collegamenti e distacchi; un clic monta e
  apre (un errore dà un messaggio; un NTFS sporco montato in sola lettura lo dice); un'icona
  "espelli" accanto ai dischi esterni smonta tutte le loro partizioni e spegne il disco
  ("kinetic can be unplugged now"), dopo aver riportato la vista alla Home se mostrava quel
  disco; occupato: "kinetic is in use: close the files open from it"; clic destro: Mount,
  Unmount, Eject. Chiamate asincrone, il socket D-Bus nel loop; senza udisks2 la barra resta
  com'è. Niente montaggio automatico; LUKS dopo l'MVP. Libreria: sdbus-c++ 2.3 (OK
  dell'autore il 2026-10-30). Test sulla logica con dati finti, un test di sola lettura sul bus
  reale (saltato senza udisks), snapshot; provato dall'autore con il Kinetic, anche con un file
  aperto.

- [ ] **7.4 Compatibilità**
  Fuori da Noctalia e Umbriel. Proposta del 2026-10-30, discussa con l'autore:
  - font di icone di Nordend: un sottoinsieme del font ufficiale Tabler Icons (MIT) con le sole
    icone usate, generato da `scripts/make-icon-font.sh` (fonttools) dalla release ufficiale, in
    `third_party/tabler-icons/` con versione e sha256 in `THIRD_PARTY.md` (OK dell'autore il
    2026-10-30). Scritto il 2026-10-30: compilato dentro il programma (13 KB), quindi c'è sempre
    e il font di Noctalia non serve più nemmeno come riserva; provato dall'autore il 2026-10-30;
  - cursore di riserva senza cursor-shape-v1: `libwayland-cursor` e il tema xcursor
    (`XCURSOR_THEME`/`XCURSOR_SIZE`, altrimenti "default" 24), alla scala giusta;
  - decorazioni client-side quando il compositor non le disegna (senza xdg-decoration o se chiede
    client-side): titolo al centro, riduci/ingrandisci/chiudi a destra, trascinare sposta, doppio
    clic ingrandisce, i bordi ridimensionano; colori della palette; su Umbriel nulla cambia;
  - sviluppo verificato su weston headless (niente xdg-decoration né cursor-shape), senza finestre
    sullo schermo dell'autore;
  - prove dell'autore: Umbriel, più una VM Arch con tre sessioni (GNOME, Plasma, niri) e SSH per
    compilare e lanciare i test dentro (Arch confermata dall'autore il 2026-10-30; Ubuntu già
    tolto, la VM da preparare prima di queste prove); checklist e risultati in
    `docs/compatibility.md`.
  Fatto quando: su Umbriel, GNOME, Plasma e niri Nordend si avvia con le icone, la finestra si
  sposta, si ridimensiona e si chiude, il cursore si vede e cambia forma, clipboard e drag and drop
  vanno con le altre app, i file si aprono nelle loro app; test e snapshot per barra e icone,
  screenshot di weston guardati.

- [ ] **7.5 Impostazioni e stato ricordato** (Ctrl+,)
  Un pannello che mostra le impostazioni, poi modificabile (scrivendo `config.toml` senza perdere
  i commenti); lo stato ricordato tra un avvio e l'altro (vista, larghezze, barra laterale,
  ordinamento, file nascosti, pannello info); una sezione "Informazioni" (Nordend, distro,
  Noctalia, Umbriel, GPU).

- [ ] **7.6 Benchmark** di tutti gli obiettivi del DESIGN, risultati in `devlog.md`
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
