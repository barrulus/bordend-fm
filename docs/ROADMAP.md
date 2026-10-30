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
- [x] **3.2 Tipi MIME**
  Glob + magic di `shared-mime-info`.
- [x] **3.3 Watcher inotify**
  Creazione, rimozione e rinomina riflesse nella vista.
- [x] **3.4 Icone per tipo di file**

## Fase 4 — Viste

- [x] **4.1 Lista scorrevole virtualizzata**
  Solo le righe visibili vengono disegnate; scroll con inerzia.
- [x] **4.2 Vista a colonne (Miller)**
  Navigazione a frecce, colonne ridimensionabili, scorrimento orizzontale animato.
- [x] **4.3 Selezione**
  Singola, multipla (Ctrl/Shift), da tastiera.
- [x] **4.4 Vista a griglia**
  Celle virtualizzate, dimensione regolabile, passaggio colonne ↔ griglia.

## Fase 5 — Miniature e Quick Look

- [x] **5.1 Decodifica immagini**
  png, jpeg, gif (wuffs), webp, jxl.
- [x] **5.2 Cache miniature freedesktop**
  Lettura e scrittura in `~/.cache/thumbnails`, compatibile con le altre app.
- [x] **5.3 Quick Look immagini**
  Overlay con barra spazio, frecce per scorrere, Esc per chiudere.
- [x] **5.4 RAW**
  JPEG incorporato negli ORF via exiv2, orientamento corretto.
  Fatto quando: Quick Look di un ORF < 100 ms (misurato).
- [x] **5.5 Preview testo e fallback info**
- [x] **5.6 Vista dettagli** (Ctrl+3)
  Righe con colonne nome, dimensione, data di modifica, tipo; ordinamento con un clic
  sull'intestazione (di nuovo: verso opposto), colonne ridimensionabili; stesso `Folder` delle
  altre viste (cursore e selezione), passaggio Ctrl+1/2/3 senza perdere il posto.
  Prima della fase 6, perché le operazioni sui file nascano e si provino su tutte e tre le viste.
  Fatto quando: cartella da 10k file ordinata per dimensione o data senza scatti nello
  scorrimento (misurato).
- [x] **5.7 Pannello info in ogni vista** (Ctrl+I)
  Il pannello informazioni delle colonne anche nella griglia e nei dettagli, a destra; Ctrl+I lo
  apre e lo chiude in tutte e tre le viste (uno stato solo, aperto all'avvio).
  Fatto quando: aprirlo e chiuderlo non fa perdere cursore, selezione né posizione, e lo
  scorrimento resta fluido con il pannello aperto (misurato).
- [x] **5.8 Anteprima nel pannello info**
  Il pannello diventa un'anteprima, uguale nelle tre viste: miniatura per foto e RAW, prime righe
  per il testo; contenuto centrato e diviso in sezioni (informazioni, dati di scatto EXIF, voto
  del sidecar darktable), con "più dettagli" a richiesta. Ctrl+I agisce solo su griglia e
  dettagli: nelle colonne il pannello è l'ultima colonna e resta sempre.
  Fatto quando: scorrendo una cartella di ORF con le frecce la miniatura compare senza rallentare
  lo scorrimento (misurato).
- [x] **5.9 Barra laterale dei luoghi** (Ctrl+B)
  A sinistra in tutte e tre le viste: Home, cartelle XDG, file system, segnalibri GTK, unità
  montate (aggiornate quando si monta o si smonta). Un clic, o Invio, apre il luogo nella vista
  corrente; è evidenziato il luogo che contiene la cartella corrente. Ctrl+B la mostra e la
  nasconde, F6 porta la tastiera dentro e fuori; larghezza trascinabile come il pannello info.
  Fatto quando: montando un'unità la voce compare senza riavviare, aprirla funziona in tutte e
  tre le viste e da ferma la finestra non consuma CPU (misurato).
- [x] **5.10 Breadcrumb navigabile**
  Il percorso nella barra in alto diventa una fila di segmenti cliccabili (la home come icona;
  un percorso troppo lungo si accorcia da sinistra con "…"). Un clic apre quella cartella nella
  vista corrente; nelle colonne mette a fuoco la colonna che la mostra, se c'è già.
  Fatto quando: un clic su qualsiasi segmento porta lì in tutte e tre le viste, con il cursore
  sulla cartella da cui si è risaliti.
- [x] **5.11 Barra di stato e scorciatoie**
  In basso: a sinistra elementi e selezione (tolti dalla barra in alto) e lo spazio libero
  dell'unità (`statvfs` su un worker); a destra i pulsanti delle tre viste (quella attiva
  evidenziata) e "?", che come F1 apre un overlay con tutte le scorciatoie, generate dalla stessa
  tabella che usa il codice.
  Fatto quando: i pulsanti cambiano vista come Ctrl+1/2/3, lo spazio libero si aggiorna dopo una
  copia e l'overlay elenca ogni scorciatoia effettiva (verificato da un test).

## Fase 6 — Operazioni sui file

- [x] **6.1 Apri con l'app predefinita** (risolutore freedesktop nostro, DESIGN "Apertura dei file")
  Enter o doppio clic su un file lo apre con l'app predefinita per il tipo che Nordend mostra; su una
  cartella entra come prima. Più file selezionati per la stessa app partono in una sola istanza.
  Le app con `Terminal=true` partono nel terminale. La finestra aperta riceve il focus
  (`xdg-activation-v1`). Riscontro nella barra di stato. Tool `nordend-open --dry-run`.
  Fatto quando: un ORF si apre in darktable, un JPEG in swayimg, un `.txt` in Helix dentro un
  terminale, con il focus sulla nuova finestra; cinque ORF selezionati aprono un solo darktable; un
  tipo senza app e un lancio fallito danno un messaggio nella barra di stato; Nordend non si blocca,
  non lascia zombie e chiuderlo non chiude le app; `nordend-open --dry-run` coincide con
  `gio mime` su tutti i tipi della macchina (o ogni differenza è spiegata); test con directory XDG
  finte (precedenza, tipo padre, `%F/%U`, escaping, `Terminal=true`, `TryExec` mancante).
- [x] **6.2 Nuova cartella, rinomina**
  Ctrl+Shift+N crea "New Folder" (o "New Folder 2", ...) con il nome già in modifica; F2 rinomina
  l'elemento sotto il cursore. Campo nel posto del nome, in tutte e tre le viste: nome selezionato
  senza l'estensione, Enter conferma, Escape annulla, un clic fuori conferma; cursore fisso (niente
  lampeggio: idle a zero). `renameat2(RENAME_NOREPLACE)` su un worker: mai sovrascrivere. Rinominare
  una foto rinomina con lei i sidecar di darktable (`X.ORF.xmp`, `X_01.ORF.xmp`, ...).
  Fatto quando: Ctrl+Shift+N crea la cartella col nome in modifica in tutte e tre le viste; F2
  rinomina con l'estensione esclusa dalla selezione, Enter/Escape/clic fuori funzionano; il cursore
  resta sull'elemento rinominato; un nome esistente o non valido dà un errore, il campo resta aperto
  e niente viene sovrascritto; rinominare un ORF rinomina i suoi `.xmp`, duplicati compresi; in una
  cartella senza permesso di scrittura compare un messaggio, senza crash; idle a zero CPU col campo
  aperto; test su una cartella temporanea e snapshot del campo.
- [x] **6.3 Copia e sposta** asincroni, con progresso e gestione dei conflitti
  Ctrl+C, Ctrl+X, Ctrl+V dentro Nordend (la clipboard di sistema è la 6.5); i file tagliati
  attenuati, Escape annulla il taglio. Motore su un thread suo, un'operazione alla volta:
  stesso disco = `renameat2(RENAME_NOREPLACE)`; copia o disco diverso = `copy_file_range` a
  blocchi su un file temporaneo nascosto, rinominato alla fine; tra dischi l'originale si cancella
  solo dopo `fsync`. Permessi, date e link simbolici conservati; i sidecar di darktable seguono.
  Controlli: cartella dentro se stessa, spazio libero, sola lettura. Conflitti in una scheda:
  Sostituisci (il vecchio sparisce solo a copia completa; cartelle unite), Salta, Tieni entrambi
  ("P1 2.ORF"), "per tutti"; nella stessa cartella Tieni entrambi da sé. Avanzamento nella barra
  di stato (file, byte, velocità, tempo rimasto, × per annullare); alla fine i file incollati
  selezionati, gli errori riassunti.
  Fatto quando: copiando i 163 ORF di `vero` in `/tmp` si vedono avanzamento e velocità, i sidecar
  arrivano e le date restano; velocità entro il 10 % di `cp` (benchmarks); uno spostamento sullo
  stesso disco è istantaneo, tra dischi copia e poi cancella; i conflitti funzionano (Sostituisci,
  Salta, Tieni entrambi, "per tutti"); annullando a metà non resta nessun file parziale; cartella
  dentro se stessa, spazio insufficiente e sola lettura danno un messaggio senza crash; i tagliati
  attenuati; idle a zero a operazione finita; test su cartelle temporanee, compreso il percorso tra
  dischi forzato.
- [x] **6.3b Misura della memoria con le foto**
  Prima si misura, poi (passo a parte, con l'OK dell'autore) si corregge. Rapporto di sviluppo nel
  log su `SIGUSR1` e con `nordend-snapshot --memory`: RSS, anonima privata, file (smaps), heap in
  uso e trattenuto da glibc (`mallinfo2`), VRAM e GTT del processo (fdinfo DRM), e la contabilità
  di Nordend (texture delle miniature, Quick Look, pannello, atlanti dei glifi, cartelle). heaptrack
  per chi alloca. Scenari ripetibili: avvio nei sorgenti, `vero` in colonne, griglia scorsa fino in
  fondo, Quick Look su 20 foto, ritorno ai sorgenti, copia da 3 GB.
  Fatto quando: in benchmarks una tabella per scenario spiega almeno il 90 % della memoria
  privata; heaptrack conferma o smentisce le ipotesi con le righe responsabili; si sa se la memoria
  torna giù lasciando una cartella di foto e perché; ogni correzione sensata ha una stima di
  guadagno ed è proposta come passo successivo.
- [x] **6.3c Restituire la memoria delle decodifiche**
  `mallopt(M_MMAP_THRESHOLD, 16 MB)` all'avvio (app e snapshot): i buffer grandi tornano al
  sistema appena liberati. Quick Look libera i pixel dopo averli caricati. Il picco mentre si fanno
  le miniature (8 decodifiche intere insieme) misurato con `VmHWM`; un limite alle decodifiche
  grandi contemporanee (8, 4, 2) solo se abbassa il picco in modo netto con meno del 10 % di tempo
  in più. Il tetto delle texture delle miniature (VRAM) resta com'è.
  Fatto quando: lo script dei 5 passi, sullo schermo e senza `GLIBC_TUNABLES`, resta intorno ai
  150 MB dal passo 3 al 5 e la memoria torna giù lasciando le foto; il primo schermo di miniature
  senza cache costa al massimo il 10 % in più (offscreen, 3 giri); il picco è misurato e la scelta
  sul limite motivata; Quick Look non tiene pixel dopo il caricamento (test); README At a glance con
  la memoria con le foto.
- [x] **6.4 Cestino** (spec freedesktop 1.0, Canc)
  Canc sposta la selezione nel cestino: sul file system della home in `$XDG_DATA_HOME/Trash`,
  sugli altri in `$topdir/.Trash/$uid` (solo se ha lo sticky bit e non è un link) o
  `$topdir/.Trash-$uid`. Prima il `.trashinfo` con `O_EXCL` (percorso originale e data), poi il
  `rename`; mai copie tra file system: se il cestino non si può usare, un errore e nulla perso.
  Le cartelle in `directorysizes`, i sidecar di darktable con la loro foto, sul thread dei
  trasferimenti; il cursore va all'elemento successivo, la barra di stato dice cosa è andato.
  Fatto quando: un altro programma del cestino (`gio trash --list` con gvfs, o `trash-list` di
  trash-cli) vede gli elementi cestinati da Nordend nella home e su DATA e li rimette al loro
  posto (`gio trash --restore`, `trash-restore`); una cartella di sola lettura dà un errore chiaro
  e non perde nulla; test sulla scelta del cestino, sul `.trashinfo` (codifica del percorso, nomi
  doppi) e sui sidecar; la scorciatoia è nella tabella e nell'overlay F1.
- [x] **6.4b Menu contestuale** (tasto destro, tap a due dita, tasto Menu, Shift+F10)
  Su un elemento (selezionato prima, se non lo era): Apri, Apri con ▸ (le app del tipo, la
  predefinita per prima), Copia, Taglia, Incolla, Rinomina (un solo elemento), Sposta nel cestino.
  Sullo spazio vuoto: Nuova cartella, Nuovo file ▸ ("Documento vuoto" e i modelli di
  `XDG_TEMPLATES_DIR`, il nome subito in modifica), Incolla. Disegnato da Nordend dentro la
  finestra con i colori del tema, spostato per restare intero vicino ai bordi; ogni voce con la sua
  scorciatoia presa dalla tabella unica; frecce, Invio, → nel sottomenu, ← ed Esc indietro, un clic
  fuori lo chiude; voci non applicabili spente. Nelle tre viste e nel pannello info.
  Fatto quando: in tutte e tre le viste il menu si apre con tasto destro, touchpad e tastiera;
  "Apri con" elenca le stesse app di `gio mime` per il tipo e le apre; "Nuovo file" crea un file
  vuoto e uno da un modello senza mai sovrascrivere; test sulle voci per ogni situazione (un
  elemento, più elementi, spazio vuoto, appunti vuoti) e sulla navigazione da tastiera; snapshot
  del menu e del sottomenu.
- [x] **6.4c Cestino nella barra laterale e ripristino**
  Una voce "Trash" fra i luoghi, con l'icona vuota o piena. Aperta, le tre viste mostrano tutti i
  cestini (home e dischi montati) come una cartella sola, con i nomi originali; il pannello info
  aggiunge "Original location" e "Deleted"; Quick Look funziona. Di sola lettura (niente incolla,
  rinomina, nuova cartella). Menu contestuale "Restore": l'elemento torna al suo posto, la cartella
  rifatta se manca, la card dei conflitti se il nome è preso, i sidecar con la foto. Si aggiorna da
  sola quando qualcosa entra o esce, anche da altri programmi.
  Fatto quando: la vista mostra quello che mostra `trash-list` (anche quello cestinato da gio o
  trash-cli); Restore rimette gli elementi dove li rimetterebbe `trash-restore`, e `trash-list`
  non li elenca più; test sul modello unito, sul ripristino (nome preso, cartella mancante,
  sidecar) e sull'icona vuota/piena.
- [x] **6.4d Svuota cestino ed elimina definitivamente**
  Nel cestino "Delete Permanently" (anche Canc) ed "Empty Trash", dal menu dello spazio vuoto e
  dal clic destro sulla voce "Trash"; sempre con una card di conferma (quanti elementi, quanti
  MB), sul thread dei trasferimenti con l'avanzamento. Fuori dal cestino Shift+Canc elimina
  definitivamente con la stessa card.
  Fatto quando: "Empty Trash" svuota home e DATA e `trash-list` non mostra più nulla; Escape sulla
  card non cancella niente; test sulla conferma e sullo svuotamento (anche di `directorysizes`).
- [x] **6.5 Clipboard** (`wl_data_device`): copia/incolla di file tra Nordend e altre app
  Ctrl+C / Ctrl+X mettono la selezione anche negli appunti di Wayland: `text/uri-list` (URI come
  li scrive GLib), `x-special/gnome-copied-files` ("copy"/"cut"), testo (i percorsi). Ctrl+V,
  quando gli appunti sono di un'altra app, legge `gnome-copied-files` (cut = sposta) o
  `text/uri-list` (copia), solo file locali, da una pipe nel loop senza bloccare, poi il motore
  della 6.3. Se un'altra app copia, i file tagliati non sono più attenuati. Ctrl+V nel campo del
  nome incolla il testo. Fuori: incollare immagini come file, drag and drop, appunti che
  sopravvivono alla chiusura.
  Fatto quando: dopo Ctrl+C `wl-paste --list-types` mostra i tre formati e `wl-paste -t
  text/uri-list` dà gli URI giusti (spazi, `%`, accenti); `wl-copy -t text/uri-list` di due file
  e Ctrl+V in Nordend li copia (sidecar compresi); un `cut` in `gnome-copied-files` li sposta;
  file copiati in Nordend si incollano in Firefox; Ctrl+V nel campo del nome incolla il testo;
  CPU a riposo 0; test su codifica/lettura delle liste e sulla scelta copia/sposta.
- [x] **6.6 Annulla nel campo del nome** (chiesto dall'autore dopo la 6.5)
  Ctrl+Z annulla l'ultima modifica al testo, Ctrl+Shift+Z la rifà: lettere scritte una dopo
  l'altra (anche sopra la selezione) o cancellate una dopo l'altra sono una modifica sola, un
  incolla è una, spostare il cursore ne chiude una; fino a 100 passi, azzerati a ogni nuovo
  nome. L'overlay delle scorciatoie passa a tre colonne quando la finestra è larga.
  Fatto quando: F2, Ctrl+V di un testo, Ctrl+Z ridà il nome di prima e Ctrl+Shift+Z di nuovo
  il testo; test sui raggruppamenti e sul rifare dopo una modifica nuova.

## Fase 7 — MVP

- [ ] **7.0 Compatibilità compositor**
  Cursore di riserva (libwayland-cursor + tema xcursor) per i compositor senza cursor-shape;
  decorazioni client-side nostre (barra, pulsanti, trascinamento, ridimensionamento) dove
  xdg-decoration manca (GNOME/Mutter); checklist su Umbriel, niri (VM), KDE, GNOME (Ubuntu).
  Fatto quando: Nordend è usabile, con finestra spostabile/chiudibile e cursore visibile, su tutti e quattro.

- [ ] **7.0b Impostazioni e stato ricordato** (Ctrl+,)
  Un pannello che mostra le impostazioni, poi modificabile (scrivendo `config.toml` senza perdere
  i commenti); lo stato ricordato tra un avvio e l'altro (larghezze, barra laterale, ordinamento,
  vista); una sezione "Informazioni" (Nordend, distro, Noctalia, Umbriel, GPU). "Fatto quando"
  da definire quando ci si arriva.

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
- [ ] **15** Quick Look del testo più ricco: evidenziazione della sintassi, selezione e copia del testo nella preview
