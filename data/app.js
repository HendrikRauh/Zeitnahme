function formatDuration(ms) {
    let milliseconds = ms % 1000;
    let totalSeconds = Math.floor(ms / 1000);
    let seconds = totalSeconds % 60;
    let totalMinutes = Math.floor(totalSeconds / 60);
    let minutes = totalMinutes % 60;
    let hours = Math.floor(totalMinutes / 60);

    let parts = [];
    if (hours > 0) parts.push(String(hours));
    if (minutes > 0 || hours > 0)
        parts.push(String(minutes).padStart(hours > 0 ? 2 : 1, "0"));
    parts.push(String(seconds).padStart(minutes > 0 || hours > 0 ? 2 : 1, "0"));

    return parts.join(":") + "," + String(milliseconds).padStart(3, "0");
}

// Initial formatting
document.addEventListener("DOMContentLoaded", function () {
    const zeitElement = document.getElementById("zeit");
    const settingsBtn = document.getElementById("settings-btn");

    // Inaktivitäts-Timeout (z.B. 10 Sekunden)
    let hideTimeout;
    function showSettingsBtn() {
        settingsBtn.style.transition = "opacity 0.5s";
        settingsBtn.style.opacity = "1";
        settingsBtn.style.pointerEvents = "auto";
        clearTimeout(hideTimeout);
        hideTimeout = setTimeout(() => {
            settingsBtn.style.opacity = "0";
            settingsBtn.style.pointerEvents = "none";
        }, 10000); // 10 Sekunden
    }

    // Bei Aktivität Button wieder anzeigen und Timer zurücksetzen
    ["mousemove", "keydown", "touchstart"].forEach((event) => {
        document.addEventListener(event, showSettingsBtn);
    });
    // Initial ausblenden nach Timeout
    showSettingsBtn();

    // Lade aktuelle Zeit vom Server
    fetch("/api/last_time")
        .then((response) => response.json())
        .then((data) => {
            zeitElement.textContent = formatDuration(data.lastTime);
        })
        .catch((err) => {
            console.log("Fehler beim Laden der letzten Zeit:", err);
            zeitElement.textContent = formatDuration(0);
        });

    let ws = new WebSocket("ws://" + location.host + "/ws");
    ws.onmessage = function (event) {
        try {
            let msg = JSON.parse(event.data);
            if (msg.type === "lastTime") {
                zeitElement.textContent = formatDuration(Number(msg.value));
            }
        } catch (e) {
            console.error("WebSocket message error:", e);
        }
    };

    ws.onopen = function () {
        console.log("WebSocket connected");
    };

    ws.onclose = function () {
        console.log("WebSocket disconnected");
    };

    ws.onerror = function (error) {
        console.error("WebSocket error:", error);
    };
});
