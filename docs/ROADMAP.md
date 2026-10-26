# Nordend — ROADMAP

Ordine di sviluppo, un passo alla volta. Ogni passo ha un criterio **Fatto quando** verificabile:
si passa al successivo solo quando quello corrente è chiuso e testato dall'autore.
Il perché delle scelte è in [`DESIGN.md`](DESIGN.md); il diario di lavoro in [`devlog.md`](devlog.md).

Legenda: `[ ]` da fare · `[~]` in corso · `[x]` fatto

---

## Fase 0 — Fondamenta

- [x] **0.1 Approvazione del DESIGN.md**
  Fatto quando: l'autore ha rivisto `DESIGN.md` e tolto la dicitura BOZZA.
- [x] **0.2 Repository**
  `git init`, licenza, `.gitignore`, `.clang-format`, `.editorconfig`, `README.md`.
  Fatto quando: primo commit fatto, `clang-format --dry-run` pulito.
- [x] **0.3 Scheletro CMake**
  Target `nordend` che compila un `main` vuoto, dipendenze trovate con `pkg-config`, warning severi.
  Fatto quando: `cmake -B build && cmake --build build` va senza warning; `./build/nordend` esce con 0.
- [ ] **0.4 Pacchetto xbps-src** — *rimandato a dopo 1.2* (impacchettare un main vuoto non serve)
  Template in `void-noctalia/packages/srcpkgs/nordend`.
  Fatto quando: `./xbps-src pkg nordend` produce il pacchetto e si installa.

## Fase 1 — Finestra e rendering

- [x] **1.1 Finestra Wayland**
  Connessione, registry, `xdg-shell`, decorazioni server-side, chiusura pulita.
  Fatto quando: si apre una finestra decorata da Umbriel, si ridimensiona e si chiude senza leak (ASan pulito).
- [x] **1.2 EGL + GLES**
  Contesto EGL, clear del colore, frame callback, niente rendering se non serve.
  Fatto quando: finestra con colore di sfondo, CPU ~0% a riposo.
- [x] **1.3 Scala frazionaria**
  `fractional-scale-v1` + `viewporter`.
  Fatto quando: rendering nitido a scala 1, 1.25, 1.5, 2.
- [x] **1.4 Primitive di disegno**
  Rettangoli, rettangoli arrotondati, bordi, ombre semplici, clipping.
  Fatto quando: una schermata di prova mostra tutte le primitive senza artefatti.
- [x] **1.5 Testo**
  Shaping con harfbuzz, atlas dei glifi, ellissi, UTF-8 completo.
  Fatto quando: nomi file con accenti, emoji, CJK e testo lungo si disegnano correttamente.

## Fase 2 — Input, loop e tema

- [x] **2.1 Tastiera**
  `xkbcommon`, layout dell'utente, ripetizione dei tasti.
- [x] **2.2 Puntatore**
  Click, doppio click, rotella e touchpad (scroll continuo), `cursor-shape-v1`.
- [x] **2.3 Main loop e worker pool**
  `poll()` unico, eventfd, coda risultati, annullamento dei lavori.
  Fatto quando: test unitari del pool verdi, nessun data race con TSan.
- [x] **2.4 Config**
  `config.toml` con `toml++`, valori di default, errori leggibili.
- [x] **2.5 Tema Noctalia**
  `colors.toml` con ricarica a caldo; template `nordend` per Noctalia.
  Fatto quando: cambiando tema in Noctalia, Nordend si aggiorna senza riavvio.
- [ ] **2.6 PR upstream del template a Noctalia** — *rimandato a dopo 7.3* (un builtin ha senso per un'app installabile; il template resta in `void-noctalia`)

## Fase 3 — File system

- [x] **3.1 Loader asincrono**
  Lettura cartelle nei worker, risultati a blocchi, ordinamento.
  Fatto quando: benchmark su 10k file registrato in `devlog.md`.
- [ ] **3.2 Tipi MIME**
  Glob + magic di `shared-mime-info`.
- [ ] **3.3 Watcher inotify**
  Creazione, rimozione e rinomina riflesse nella vista.
- [ ] **3.4 Icone per tipo di file**

## Fase 4 — Viste

- [ ] **4.1 Lista scorrevole virtualizzata**
  Solo le righe visibili vengono disegnate; scroll con inerzia.
- [ ] **4.2 Vista a colonne (Miller)**
  Navigazione a frecce, colonne ridimensionabili, scorrimento orizzontale animato.
- [ ] **4.3 Selezione**
  Singola, multipla (Ctrl/Shift), da tastiera.
- [ ] **4.4 Vista a griglia**
  Celle virtualizzate, dimensione regolabile, passaggio colonne ↔ griglia.

## Fase 5 — Miniature e Quick Look

- [ ] **5.1 Decodifica immagini**
  png, jpeg, gif (wuffs), webp, jxl.
- [ ] **5.2 Cache miniature freedesktop**
  Lettura e scrittura in `~/.cache/thumbnails`, compatibile con le altre app.
- [ ] **5.3 Quick Look immagini**
  Overlay con barra spazio, frecce per scorrere, Esc per chiudere.
- [ ] **5.4 RAW**
  JPEG incorporato negli ORF via exiv2, orientamento corretto.
  Fatto quando: Quick Look di un ORF < 100 ms (misurato).
- [ ] **5.5 Preview testo e fallback info**

## Fase 6 — Operazioni sui file

- [ ] **6.1 Apri con l'app predefinita** (`xdg-open`)
- [ ] **6.2 Nuova cartella, rinomina**
- [ ] **6.3 Copia e sposta** asincroni, con progresso e gestione dei conflitti
- [ ] **6.4 Cestino** (spec freedesktop)
- [ ] **6.5 Clipboard** (`wl_data_device`): copia/incolla di file tra Nordend e altre app

## Fase 7 — MVP

- [ ] **7.0 Compatibilità compositor**
  Cursore di riserva (libwayland-cursor + tema xcursor) per i compositor senza cursor-shape;
  decorazioni client-side nostre (barra, pulsanti, trascinamento, ridimensionamento) dove
  xdg-decoration manca (GNOME/Mutter); checklist su Umbriel, niri (VM), KDE, GNOME (Ubuntu).
  Fatto quando: Nordend è usabile, con finestra spostabile/chiudibile e cursore visibile, su tutti e quattro.

- [ ] **7.1 Benchmark** di tutti gli obiettivi del DESIGN, risultati in `devlog.md`
- [ ] **7.2 Uso quotidiano per una settimana**, bug annotati e chiusi
- [ ] **7.3 Rilascio 0.1.0**

---

## Dopo l'MVP

- [ ] **8** Palette comandi (Ctrl+K) e ricerca fuzzy
- [ ] **9** Schede, doppio pannello, coda operazioni con annulla, rinomina multipla
- [ ] **10** Culling fotografico (stelle, etichette colore, XMP darktable, confronto, EXIF, istogramma)
- [ ] **11** Dischi rimovibili (udisks2)
- [ ] **12** Scripting Luau
- [ ] **13** Backend portal FileChooser
- [ ] **14** Editor esterno (helix), vedi appendice del DESIGN
