// Nordend's home page: the light or dark theme, the visitor's choice remembered, else the
// system's (the same key as the documentation's, so both pages agree).
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
