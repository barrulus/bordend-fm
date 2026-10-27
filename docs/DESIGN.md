# Nordend File Manager — DESIGN

> **Stato: APPROVATO** dall'autore il 2026-10-26. È l'autorità del progetto.
> I punti marcati *da verificare* non sono ancora fatti certi.
> Ordine di sviluppo: vedi [`ROADMAP.md`](ROADMAP.md).

## Visione (testo originale dell'autore)

> Un file manager moderno per wayland first. Che si sposa perfettamente con le API di noctalia e tutto
> l'ecosistema umbriel, C++ nativo. Comodo, gradevole alla vista, reattivo, performance elevate, ispirato
> al Finder di OSX: premendo barra spazio su un file se ne vede la preview. Doppia visualizzazione
> colonna/griglia. Editing dei file di testo con helix. Modulare, scalabile ed estendibile con script.

## Principi

1. **Mai bloccare il frame.** Nessun I/O sul thread UI. Una cartella da 10k file scorre fluida.
2. **Misurare, non supporre.** Ogni obiettivo di performance ha un benchmark ripetibile.
3. **Coerenza con Noctalia.** Stesso stack, stessa palette, stessa sensazione d'uso.
4. **Tastiera prima, mouse sempre.** Ogni azione ha una scorciatoia e passa dalla palette comandi.
5. **Stabile prima che brillante.** Si aggiunge una feature solo quando quella prima è solida.

## Lingua

- Codice, commenti, identificatori, nomi file, commit, `README.md`: inglese.
- `docs/buglog.md` (materiale per issue e PR upstream), `docs/devlog.md`: inglese.
- `docs/DESIGN.md`, `docs/ROADMAP.md`: italiano.

## Stack tecnico

Allineato a Noctalia per le librerie di sistema; il codice di Nordend lo scriviamo noi (vedi sotto).

| Area | Scelta | Note |
|---|---|---|
| Linguaggio | C++20, GCC 14 | |
| Build | CMake | come `quark++` |
| Finestra | `wayland-client`, `xdg-shell` | niente Qt/GTK/SDL |
| Decorazioni | server-side (`xdg-decoration`, disegnate da Umbriel) | **deciso** |
| Input | `xkbcommon` | |
| Rendering | EGL + OpenGL ES 3.0 (libGLESv2) | ES 3.0 per instancing e texture R8 (atlas glifi) |
| Testo | harfbuzz + freetype + fontconfig | shaping, fallback e atlas dei glifi nostri; niente pango/cairo |
| Config | `toml++`, `~/.config/nordend/config.toml` | |
| Immagini | wuffs, libwebp, libjxl | come Noctalia |
| RAW + EXIF | exiv2 | estrae le anteprime JPEG incorporate (ORF e JPEG delle fotocamere, per miniature e Quick Look); più avanti scrive gli XMP |
| Ricerca fuzzy | fzy | come Noctalia |
| Icone UI | Tabler | come Noctalia |
| Tipi MIME | database `shared-mime-info` (glob + magic) | parser nostro di globs2 e magic, ordine di riconoscimento di GLib (deciso 2026-10-26) |
| D-Bus (post-MVP) | sdbus-c++ | udisks2, portal |
| Scripting (post-MVP) | Luau | sandbox; *da verificare* come Noctalia espone le sue API |

Protocolli Wayland: `xdg-shell`, `xdg-decoration`, `fractional-scale-v1`, `viewporter`,
`cursor-shape-v1`, `wl_data_device` (clipboard, DnD), `xdg-activation`. Più avanti `text-input-v3`.
Il supporto di ciascuno in Umbriel è *da verificare*.

## Codice proprio e codice da altri progetti

**Deciso:** progetto a lungo termine, quindi toolkit, rendering e logica dell'app li scriviamo noi in C++.
Restano le librerie di sistema della tabella sopra (Wayland, EGL, harfbuzz, exiv2, …): non si riscrivono.

Copiare codice da Noctalia o Umbriel (entrambi MIT) è un'eccezione, valutata caso per caso dal copilot:
- si copia solo un pezzo isolato e già collaudato, quando riscriverlo aggiungerebbe rischio senza valore;
- **l'autore viene sempre avvisato prima**, con cosa si copia, da dove e perché;
- il file copiato riporta in testa progetto d'origine, percorso, commit e licenza;
- ogni copia è elencata in `THIRD_PARTY.md` (creato alla prima occasione) e annotata nel devlog.

Studiare il loro codice come riferimento resta sempre possibile e incoraggiato.

## Tema

Nordend legge i colori da `~/.config/nordend/colors.toml` e lo ricarica a caldo (inotify).
Il file è generato da un template `nordend` che proponiamo **upstream a Noctalia con una PR**
(**deciso**). Finché la PR non è accettata, il template vive in `void-noctalia/config/noctalia/templates`.
Senza il file, Nordend usa una palette di default incorporata.

## Architettura

```
app/        main loop, comandi, keybinding, palette comandi
ui/         toolkit minimo: surface, renderer GLES, testo, layout, widget, animazioni
fs/         modello directory, loader asincrono, watcher inotify, operazioni file, MIME
views/      vista a colonne (Miller), vista a griglia, vista dettagli
image/      decodifica immagini (wuffs, libwebp, libjxl) e orientamento EXIF, per preview/ e thumbs/
preview/    quick look: un provider per tipo (immagine, raw, testo, fallback info)
thumbs/     generazione e cache miniature (spec freedesktop, ~/.cache/thumbnails)
theme/      palette Noctalia, ricarica a caldo
platform/   lancio processi, xdg-open, terminale/editor esterno
```

**Threading.** Il thread principale fa un unico `poll()` su: fd Wayland, eventfd dei worker, inotify,
timer. Un pool di worker esegue I/O, decodifica e miniature e restituisce risultati tramite una coda +
eventfd. I lavori hanno priorità (visibile > adiacente > prefetch) e si annullano quando escono dalla vista.

**Preview provider.** Interfaccia unica (`can_handle(mime)`, `load(path, cancel_token)`), così aggiungere
un tipo non tocca il resto. È anche il punto d'aggancio futuro per gli script Luau.

## Viste

- **Colonne (Miller, stile Finder).** Ogni selezione di cartella apre la colonna a destra; l'ultima colonna
  mostra la preview del file selezionato. Scorrimento orizzontale fluido, larghezze ridimensionabili.
- **Griglia.** Miniature con dimensione regolabile (Ctrl +/−), caricamento solo delle celle visibili.
- **Dettagli (Ctrl+3).** Una riga per file con colonne nome, dimensione, data, tipo; ordinamento dal clic
  sull'intestazione, colonne ridimensionabili. Aggiunta il 2026-10-27 con l'OK dell'autore.
- **Quick Look (barra spazio).** Overlay sopra la finestra; frecce per passare al file successivo
  senza chiuderlo; Esc o spazio per chiudere.

## MVP

Dentro:
- finestra Wayland con rendering GLES e testo, decorazioni server-side
- palette Noctalia con ricarica a caldo
- vista a colonne, a griglia e dettagli, navigazione completa da tastiera e mouse
- loader asincrono + watcher inotify
- miniature con cache freedesktop
- Quick Look: immagini (png, jpeg, webp, jxl, gif), RAW tramite JPEG incorporato con orientamento
  corretto, testo (monospace, limite di dimensione), fallback con info del file
- apertura con l'app predefinita (`xdg-open`)
- operazioni base: copia, sposta, rinomina, nuova cartella, cestino (spec freedesktop)

Fuori dall'MVP: schede, doppio pannello, ricerca, palette comandi, culling, Luau, portal, udisks2,
evidenziazione della sintassi, DnD verso altre app, editor esterno.

## Dopo l'MVP (in ordine)

1. Palette comandi (Ctrl+K) e ricerca fuzzy
2. Schede e doppio pannello; coda operazioni con annulla; rinomina multipla con anteprima
3. **Culling fotografico**: stelle 1–5 ed etichette colore nei sidecar XMP di darktable, confronto
   affiancato, EXIF in overlay, istogramma
4. Dischi rimovibili via udisks2
5. Scripting Luau (comandi, preview provider, azioni sui file)
6. Backend `xdg-desktop-portal` FileChooser: Nordend come selettore file di sistema
7. Editor esterno (helix), vedi sotto

## Obiettivi di performance (da misurare, non promesse)

- avvio a freddo fino al primo frame: < 150 ms
- apertura di una cartella da 10k file: primo frame < 50 ms, lista completa in background
- Quick Look di un ORF: < 100 ms
- scroll a frequenza piena del monitor, nessun frame perso nel benchmark di scorrimento

## Appendice: editor esterno (helix) — non prioritario

Non è un must-have: si fa per ultimo, dopo il resto della roadmap.

Fase 1: `e` su un file di testo apre l'editor in un terminale, con un app-id fisso (`nordend-editor`)
così Umbriel può farlo comparire flottante e centrato sopra Nordend (window rule).

Risoluzione del **terminale**, primo che funziona:
1. `editor.terminal` in `config.toml` (comando completo, con segnaposto per app-id e comando)
2. `$TERMINAL`
3. `xdg-terminal-exec` (spec freedesktop proposta; non consente di impostare l'app-id,
   quindi niente overlay via window rule)
4. ricerca nel `PATH` di terminali noti, ognuno con il suo flag per l'app-id:
   `foot --app-id`, `ghostty --class`, `kitty --class`, `alacritty --class`, `wezterm start --class`
   (*da verificare* la sintassi esatta per ciascuno)

Risoluzione dell'**editor**: `editor.command` in config → `$VISUAL` → `$EDITOR` → `hx`.

Se non si trova nulla: notifica in-app con l'indicazione di cosa impostare in config, e il file resta
apribile in sola lettura nel Quick Look. Nessun crash, nessun fallimento silenzioso.

Fase 2 (solo se la 1 non basta): terminale integrato in un pannello con `libvterm`.
