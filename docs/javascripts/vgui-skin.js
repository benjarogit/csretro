/* Swap vendored vgui.css skins with the Material color-scheme toggle. */
(function () {
  const sharedHref = "assets/vgui/styles/steam_shared.css";
  const lightHref = "assets/vgui/styles/greensteam/greensteam.css";
  const darkHref = "assets/vgui/styles/blacksteam/blacksteam.css";

  function ensureLink(id, href) {
    let el = document.getElementById(id);
    if (!el) {
      el = document.createElement("link");
      el.id = id;
      el.rel = "stylesheet";
      document.head.appendChild(el);
    }
    el.href = new URL(href, document.baseURI).href;
    return el;
  }

  function apply() {
    const scheme =
      document.documentElement.getAttribute("data-md-color-scheme") || "default";
    ensureLink("csr-vgui-shared", sharedHref).disabled = false;
    const light = ensureLink("csr-vgui-light", lightHref);
    const dark = ensureLink("csr-vgui-dark", darkHref);
    const darkOn = scheme === "slate";
    light.disabled = darkOn;
    dark.disabled = !darkOn;
  }

  apply();
  const observer = new MutationObserver(apply);
  observer.observe(document.documentElement, {
    attributes: true,
    attributeFilter: ["data-md-color-scheme"],
  });
})();
