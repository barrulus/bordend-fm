// Nordend's site: one page per tab, chosen by the address ("#guide", "#guide/quick-look"), the
// light or dark theme, and the shortcut tables drawn from the data in index.html. Without
// JavaScript every page shows, one after the other.
"use strict";

document.documentElement.classList.add("js");

// Theme: the visitor's choice, else the system's.
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

// Pages.
const pages = [...document.querySelectorAll(".page")];
const links = [...document.querySelectorAll(".nav a")];
function show() {
    const target = decodeURIComponent(location.hash.slice(1));
    const name = target.split("/")[0];
    const page = pages.find((p) => p.id === name) ?? pages[0];
    for (const p of pages) {
        p.classList.toggle("active", p === page);
    }
    for (const a of links) {
        if (a.getAttribute("href") === `#${page.id}`) {
            a.setAttribute("aria-current", "page");
        } else {
            a.removeAttribute("aria-current");
        }
    }
    const title = page.querySelector("h1, h2");
    document.title = page === pages[0] ? "Nordend" : `${title.textContent} · Nordend`;
    const anchor = target.includes("/") ? document.getElementById(target) : null;
    if (anchor) {
        anchor.scrollIntoView();
    } else {
        window.scrollTo(0, 0);
    }
}
// After choosing a page (not at load), focus its title, so that a screen reader reads where it is
// and the next Tab starts from the page.
// The skip link focuses the page without touching the hash, which chooses the page.
document.querySelector(".skip").addEventListener("click", (event) => {
    event.preventDefault();
    document.getElementById("main").focus();
});

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

// Shortcuts: "Ctrl+Shift+Z" becomes three keys; two spaces separate alternatives ("F1  ?").
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

show();
