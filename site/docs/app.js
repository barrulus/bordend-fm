// Nordend's site, read like documentation: one page at a time, chosen by the address
// ("#guide", "#guide/quick-look"); the sidebar lists every page and, under the one shown, its
// sections; "On this page" follows the reading on wide screens; a search over every page's text;
// previous and next at the bottom; the light or dark theme; the shortcut tables drawn from the data
// in index.html. Without JavaScript every page shows, one after the other.
"use strict";

document.documentElement.classList.add("js");

// ------------------------------------------------------------------ Theme: the visitor's choice,
// else the system's.
const themeButton = document.querySelector(".theme-toggle");
function applyTheme(theme) {
    document.documentElement.dataset.theme = theme;
    themeButton.textContent = theme === "dark" ? "Light" : "Dark";
    themeButton.setAttribute("aria-label", `Switch to the ${themeButton.textContent.toLowerCase()} theme`);
}
let stored = null;
try {
    stored = localStorage.getItem("nordend-theme");
} catch {
    // storage blocked: follow the system
}
applyTheme(stored ?? (matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark"));
themeButton.addEventListener("click", () => {
    const next = document.documentElement.dataset.theme === "dark" ? "light" : "dark";
    applyTheme(next);
    try {
        localStorage.setItem("nordend-theme", next);
    } catch {
        // not remembered, still applied
    }
});

// ------------------------------------------------------------------ Shortcuts: "Ctrl+Shift+Z"
// becomes three keys; two spaces separate alternatives ("F1  ?"). Drawn first, so that their
// groups are sections like any other.
function chord(text) {
    const keys = [];
    let current = "";
    for (const c of text) {
        if (c === "+" && current !== "") {
            keys.push(current);
            current = "";
        } else {
            current += c;
        }
    }
    if (current !== "") {
        keys.push(current);
    }
    return keys;
}

function keysCell(text) {
    const cell = document.createElement("td");
    text.split("  ").forEach((alternative, i) => {
        if (i > 0) {
            const gap = document.createElement("span");
            gap.className = "or";
            cell.append(gap);
        }
        chord(alternative).forEach((key, j) => {
            if (j > 0) {
                cell.append("+");
            }
            const kbd = document.createElement("kbd");
            kbd.textContent = key;
            cell.append(kbd);
        });
    });
    return cell;
}

const data = document.getElementById("shortcut-data");
const holder = document.getElementById("shortcut-groups");
if (data && holder) {
    const groups = new Map();
    for (const shortcut of JSON.parse(data.textContent)) {
        if (!groups.has(shortcut.group)) {
            groups.set(shortcut.group, []);
        }
        groups.get(shortcut.group).push(shortcut);
    }
    holder.replaceChildren();
    for (const [group, list] of groups) {
        const section = document.createElement("section");
        const heading = document.createElement("h3");
        heading.textContent = group;
        heading.id = `shortcuts/${group.toLowerCase().replaceAll(" ", "-")}`;
        const wrap = document.createElement("div");
        wrap.className = "table";
        const table = document.createElement("table");
        for (const shortcut of list) {
            const row = table.insertRow();
            row.append(keysCell(shortcut.keys));
            const action = row.insertCell();
            action.textContent = shortcut.action;
        }
        wrap.append(table);
        section.append(heading, wrap);
        holder.append(section);
    }
}

// ------------------------------------------------------------------ Pages and their sections.
const pages = [...document.querySelectorAll(".page")];
const pageLinks = [...document.querySelectorAll(".nav a[href^='#']")];

// A page's name as the sidebar gives it ("Overview", not its long title).
function titleOf(page) {
    const link = pageLinks.find((a) => a.getAttribute("href") === `#${page.id}`);
    return (link ?? page.querySelector("h1, h2")).textContent.trim();
}

// Every section of every page: its heading and its text up to the next one (the search).
const sections = new Map();
for (const page of pages) {
    const list = [];
    for (const heading of page.querySelectorAll("h3[id]")) {
        let text = heading.textContent;
        for (let node = heading.nextElementSibling; node && node.tagName !== "H3"; node = node.nextElementSibling) {
            text += ` ${node.textContent}`;
        }
        list.push({ heading, title: heading.textContent.trim(), text: text.toLowerCase() });
        // A link to the section beside its title, to copy and share.
        const anchor = document.createElement("a");
        anchor.className = "anchor";
        anchor.href = `#${heading.id}`;
        anchor.textContent = "#";
        anchor.setAttribute("aria-label", `Link to “${heading.textContent.trim()}”`);
        heading.append(anchor);
    }
    sections.set(page, list);
}

// Under each page's link in the sidebar, the list of its sections (shown for the page shown, and
// for every match while searching).
const sectionLists = new Map();
for (const link of pageLinks) {
    const page = pages.find((p) => `#${p.id}` === link.getAttribute("href"));
    if (!page || sections.get(page).length === 0) {
        continue;
    }
    const list = document.createElement("ul");
    list.className = "sections";
    list.hidden = true;
    for (const section of sections.get(page)) {
        const item = document.createElement("li");
        const a = document.createElement("a");
        a.href = `#${section.heading.id}`;
        a.textContent = section.title;
        section.navLink = a;
        item.append(a);
        list.append(item);
    }
    link.after(list);
    sectionLists.set(page, list);
}

const toc = document.querySelector(".toc");
const pager = document.querySelector(".pager");
let observer = null;

function markCurrent(heading) {
    for (const link of document.querySelectorAll(".nav .sections a, .toc a")) {
        if (heading && link.getAttribute("href") === `#${heading.id}`) {
            link.setAttribute("aria-current", "location");
        } else {
            link.removeAttribute("aria-current");
        }
    }
}

function buildToc(page) {
    toc.replaceChildren();
    observer?.disconnect();
    const list = sections.get(page);
    if (list.length === 0) {
        return;
    }
    const title = document.createElement("h2");
    title.textContent = "On this page";
    const ul = document.createElement("ul");
    for (const section of list) {
        const li = document.createElement("li");
        const a = document.createElement("a");
        a.href = `#${section.heading.id}`;
        a.textContent = section.title;
        li.append(a);
        ul.append(li);
    }
    toc.append(title, ul);
    // The section being read: the last heading above the upper part of the window.
    const visible = new Set();
    observer = new IntersectionObserver((entries) => {
        for (const entry of entries) {
            if (entry.isIntersecting) {
                visible.add(entry.target);
            } else {
                visible.delete(entry.target);
            }
        }
        const headings = list.map((s) => s.heading);
        const first = headings.find((h) => visible.has(h));
        if (first) {
            markCurrent(first);
        } else {
            const above = headings.filter((h) => h.getBoundingClientRect().top < 0);
            markCurrent(above.at(-1) ?? null);
        }
    }, { rootMargin: "-60px 0px -55% 0px" });
    for (const section of list) {
        observer.observe(section.heading);
    }
}

function buildPager(page) {
    pager.replaceChildren();
    const index = pages.indexOf(page);
    const add = (other, rel, label) => {
        const a = document.createElement("a");
        a.href = `#${other.id}`;
        a.className = rel;
        a.rel = rel;
        const small = document.createElement("span");
        small.textContent = label;
        const name = document.createElement("strong");
        name.textContent = titleOf(other);
        a.append(small, name);
        pager.append(a);
    };
    if (index > 0) {
        add(pages[index - 1], "prev", "Previous");
    }
    if (index + 1 < pages.length) {
        add(pages[index + 1], "next", "Next");
    }
}

let shownPage = null;
function show() {
    const target = decodeURIComponent(location.hash.slice(1));
    const name = target.split("/")[0];
    const page = pages.find((p) => p.id === name) ?? pages[0];
    for (const p of pages) {
        p.classList.toggle("active", p === page);
    }
    for (const a of pageLinks) {
        if (a.getAttribute("href") === `#${page.id}`) {
            a.setAttribute("aria-current", "page");
        } else {
            a.removeAttribute("aria-current");
        }
    }
    if (!searching()) {
        for (const [p, list] of sectionLists) {
            list.hidden = p !== page;
            for (const item of list.children) {
                item.hidden = false;
            }
        }
    }
    document.title = page === pages[0] ? "Nordend: a fast, native file manager for Wayland" : `${titleOf(page)} · Nordend`;
    if (page !== shownPage) {
        buildToc(page);
        buildPager(page);
        shownPage = page;
    }
    const anchor = target.includes("/") ? document.getElementById(target) : null;
    if (anchor) {
        anchor.scrollIntoView();
        markCurrent(anchor);
    } else {
        window.scrollTo(0, 0);
    }
    closeMenu();
}

// After choosing a page or a section (not at load), focus its title, so that a screen reader
// reads where it is and the next Tab starts from there.
addEventListener("hashchange", () => {
    show();
    const target = decodeURIComponent(location.hash.slice(1));
    const page = document.querySelector(".page.active");
    const anchor = target.includes("/") ? document.getElementById(target) : null;
    const heading = anchor ?? page?.querySelector("h1, h2");
    if (heading) {
        heading.setAttribute("tabindex", "-1");
        heading.focus({ preventScroll: true });
    }
});

// The skip link focuses the page without touching the address, which chooses the page.
document.querySelector(".skip").addEventListener("click", (event) => {
    event.preventDefault();
    document.getElementById("main").focus();
});

// ------------------------------------------------------------------ Search: the pages and the
// sections whose words hold every word typed.
const filter = document.getElementById("filter");
const noMatch = document.querySelector(".nav .no-match");

function searching() {
    return filter.value.trim() !== "";
}

// A word typed matches the start of a word in the text ("raw" finds "raw files", not "draw").
function wordMatcher(word) {
    const escaped = word.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    return new RegExp(`(^|[^\\p{L}\\p{N}])${escaped}`, "u");
}

function runSearch() {
    const words = filter.value.toLowerCase().split(/\s+/).filter(Boolean).map(wordMatcher);
    const groups = [...document.querySelectorAll(".nav h2")];
    if (words.length === 0) {
        for (const a of pageLinks) {
            a.parentElement.hidden = false;
        }
        for (const g of groups) {
            g.hidden = false;
            g.nextElementSibling.hidden = false;
        }
        noMatch.hidden = true;
        show();
        return;
    }
    let found = 0;
    for (const a of pageLinks) {
        const page = pages.find((p) => `#${p.id}` === a.getAttribute("href"));
        const list = sectionLists.get(page);
        const pageText = page.textContent.toLowerCase();
        const pageMatches = words.every((w) => w.test(pageText));
        let shown = 0;
        for (const section of sections.get(page)) {
            const hit = words.every((w) => w.test(section.text));
            section.navLink.parentElement.hidden = !hit;
            shown += hit ? 1 : 0;
        }
        if (list) {
            list.hidden = shown === 0;
        }
        a.parentElement.hidden = !pageMatches;
        found += pageMatches ? 1 : 0;
    }
    for (const g of groups) {
        const ul = g.nextElementSibling;
        const any = [...ul.children].some((li) => !li.hidden);
        g.hidden = !any;
        ul.hidden = !any;
    }
    noMatch.hidden = found > 0;
}

filter.addEventListener("input", runSearch);
filter.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
        // The first section found, else the first page.
        const first = document.querySelector(".nav .sections li:not([hidden]) a, .nav li:not([hidden]) > a");
        if (first) {
            location.hash = first.getAttribute("href");
        }
    } else if (event.key === "Escape") {
        filter.value = "";
        runSearch();
    }
});

// "/" goes to the search, as on many documentation sites.
addEventListener("keydown", (event) => {
    if (event.key === "/" && !["INPUT", "TEXTAREA"].includes(document.activeElement.tagName)) {
        event.preventDefault();
        openMenu();
        filter.focus();
    }
});

// ------------------------------------------------------------------ The sidebar as a drawer on
// narrow screens.
const menuButton = document.querySelector(".menu-toggle");
function openMenu() {
    if (getComputedStyle(menuButton).display === "none") {
        return;
    }
    document.body.classList.add("nav-open");
    menuButton.setAttribute("aria-expanded", "true");
}
function closeMenu() {
    document.body.classList.remove("nav-open");
    menuButton.setAttribute("aria-expanded", "false");
}
menuButton.addEventListener("click", () => {
    if (document.body.classList.contains("nav-open")) {
        closeMenu();
    } else {
        openMenu();
        filter.focus();
    }
});
addEventListener("keydown", (event) => {
    if (event.key === "Escape" && document.body.classList.contains("nav-open")) {
        closeMenu();
        menuButton.focus();
    }
});

show();

// "?q=raw" in the address opens the site with that search made (a search to share).
const query = new URLSearchParams(location.search).get("q");
if (query) {
    filter.value = query;
    runSearch();
}
