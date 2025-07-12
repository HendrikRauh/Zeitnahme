const INACTIVITY_DELAY = 10000; // ms
const DEBOUNCE_DELAY = 100; // ms

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

// Buttons global verfügbar machen
const settingsBtn = document.getElementById("settings-btn");
const wakelockBtn = document.getElementById("wakelock-btn");

document.addEventListener("DOMContentLoaded", function () {
    const zeitElement = document.getElementById("zeit");

    if (!settingsBtn) {
        console.warn(
            "#settings-btn not found in DOM. Settings button logic will be skipped."
        );
        return;
    }

    // Inaktivitäts-Timeout (z.B. 10 Sekunden)
    let hideTimeout;
    function showSettingsBtn() {
        [settingsBtn, wakelockBtn].forEach((btn) => {
            if (!btn) return;
            btn.style.transition = "opacity 0.5s";
            btn.style.opacity = "1";
            btn.style.pointerEvents = "auto";
            btn.setAttribute("aria-hidden", "false");
        });
        clearTimeout(hideTimeout);
        hideTimeout = setTimeout(() => {
            [settingsBtn, wakelockBtn].forEach((btn) => {
                if (!btn) return;
                btn.style.opacity = "0";
                btn.style.pointerEvents = "none";
                btn.setAttribute("aria-hidden", "true");
            });
        }, INACTIVITY_DELAY); // 10 Sekunden
    }

    // Debounce utility function
    function debounce(func, delay) {
        let timeoutId;
        return function (...args) {
            clearTimeout(timeoutId);
            timeoutId = setTimeout(() => func.apply(this, args), delay);
        };
    }

    // Debounced version of showSettingsBtn
    const debouncedShowSettingsBtn = debounce(showSettingsBtn, DEBOUNCE_DELAY);

    // Bei Aktivität Button wieder anzeigen und Timer zurücksetzen
    ["mousemove", "keydown", "touchstart"].forEach((event) => {
        document.addEventListener(event, debouncedShowSettingsBtn);
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

    // Hilfsfunktion: Laufstatus anzeigen
    function updateLaufstatus(count) {
        const laufstatusTop = document.getElementById("laufstatus-top");
        const laufstatusBottom = document.getElementById("laufstatus-bottom");
        laufstatusTop.textContent = laufstatusBottom.textContent = "";
        if (count > 0) {
            laufstatusTop.textContent = "🔺".repeat(count);
            laufstatusBottom.textContent = "🔻".repeat(count);
        }
    }

    let ws = new WebSocket("ws://" + location.host + "/ws");
    // WebSocket-Handler erweitern
    ws.onmessage = function (event) {
        try {
            let msg = JSON.parse(event.data);
            if (msg.type === "lastTime") {
                zeitElement.textContent = formatDuration(Number(msg.value));
            }
            if (msg.type === "laufCount") {
                updateLaufstatus(Number(msg.value));
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

    // Initialen Laufstatus laden
    fetch("/api/lauf_count")
        .then((response) => response.json())
        .then((data) => updateLaufstatus(Number(data.count)))
        .catch(() => updateLaufstatus(0));
});

// --- Bildschirm-Wachhalten & Fullscreen (Wake Lock API) ---
// Button mit Blitz-Icon aktiviert Fullscreen und verhindert Sleep
let wakeLock = null;
async function requestWakeLock() {
    try {
        if ("wakeLock" in navigator && location.protocol === "https:") {
            wakeLock = await navigator.wakeLock.request("screen");
            wakeLock.addEventListener("release", () => {
                console.log("Wake Lock wurde freigegeben");
            });
            console.log("Wake Lock aktiviert");
            setWakelockBtnFeedback("success");
        } else {
            // Fallback: Dummy-Video
            startDummyVideo();
            setWakelockBtnFeedback("success");
            console.log("Dummy-Video-WakeLock aktiviert (Fallback)");
        }
    } catch (err) {
        setWakelockBtnFeedback("fail");
        console.error(`${err.name}, ${err.message}`);
    }
}

function setWakelockBtnFeedback(state) {
    if (!wakelockBtn) return;
    wakelockBtn.classList.remove("wakelock-success", "wakelock-fail");
    if (state === "success") {
        wakelockBtn.classList.add("wakelock-success");
        setTimeout(() => wakelockBtn.classList.remove("wakelock-success"), 800);
    } else if (state === "fail") {
        wakelockBtn.classList.add("wakelock-fail");
        setTimeout(() => wakelockBtn.classList.remove("wakelock-fail"), 800);
    }
}

async function releaseWakeLock() {
    if (wakeLock && typeof wakeLock.release === "function") {
        try {
            await wakeLock.release();
            wakeLock = null;
            console.log("Wake Lock deaktiviert");
        } catch (e) {
            console.error("Fehler beim Freigeben des Wake Lock:", e);
        }
    }
    // Dummy-Video stoppen (Fallback)
    stopDummyVideo();
}

let fullscreenActive = false;
async function toggleFullscreenAndWakeLock() {
    if (!fullscreenActive) {
        // Fullscreen aktivieren
        const docElm = document.documentElement;
        if (docElm.requestFullscreen) {
            await docElm.requestFullscreen();
        } else if (docElm.webkitRequestFullscreen) {
            await docElm.webkitRequestFullscreen();
        } else if (docElm.msRequestFullscreen) {
            await docElm.msRequestFullscreen();
        }
        // Wake Lock anfordern
        await requestWakeLock();
    } else {
        // Fullscreen verlassen
        if (
            document.fullscreenElement ||
            document.webkitFullscreenElement ||
            document.msFullscreenElement
        ) {
            if (document.exitFullscreen) {
                await document.exitFullscreen();
            } else if (document.webkitExitFullscreen) {
                await document.webkitExitFullscreen();
            } else if (document.msExitFullscreen) {
                await document.msExitFullscreen();
            }
        }
        // Wake Lock freigeben
        await releaseWakeLock();
    }
}

if (wakelockBtn) {
    wakelockBtn.addEventListener("click", toggleFullscreenAndWakeLock);
}

// Fullscreen-Status überwachen, Settings-Button im Fullscreen ausblenden
function updateFullscreenState() {
    fullscreenActive = !!(
        document.fullscreenElement ||
        document.webkitFullscreenElement ||
        document.msFullscreenElement
    );
    if (settingsBtn) {
        settingsBtn.style.display = fullscreenActive ? "none" : "";
    }
    // Optional: Symbol des wakelock-Buttons ändern
    if (wakelockBtn) {
        wakelockBtn.innerText = fullscreenActive ? "❌" : "⚡";
    }
}

document.addEventListener("fullscreenchange", updateFullscreenState);
document.addEventListener("webkitfullscreenchange", updateFullscreenState);
document.addEventListener("msfullscreenchange", updateFullscreenState);

// Wake Lock automatisch erneut anfordern, wenn die Seite wieder sichtbar wird
document.addEventListener("visibilitychange", () => {
    if (
        wakeLock !== null &&
        document.visibilityState === "visible" &&
        fullscreenActive
    ) {
        requestWakeLock();
    }
});

// --- Dummy-Video-Workaround für Wake Lock (z.B. für Firefox/HTTP) ---
let dummyVideo = null;
function startDummyVideo() {
    if (dummyVideo) return;
    // Canvas erzeugen
    const canvas = document.createElement("canvas");
    canvas.width = 80;
    canvas.height = 60;
    canvas.style.position = "fixed";
    canvas.style.left = "-9999px"; // Offscreen
    canvas.style.top = "-9999px";
    canvas.style.zIndex = "-1";
    canvas.style.opacity = "0";
    canvas.style.pointerEvents = "none";
    document.body.appendChild(canvas);

    // Animations-Loop: Farbe wechselt (nur 2 FPS)
    const ctx = canvas.getContext("2d");
    let hue = 0;
    let running = true;
    function draw() {
        if (!running) return;
        ctx.fillStyle = `hsl(${hue}, 100%, 50%)`;
        ctx.fillRect(0, 0, canvas.width, canvas.height);
        hue = (hue + 10) % 360;
        setTimeout(draw, 500); // 2 FPS
    }
    draw();

    // Canvas als Video-Stream (2 FPS reichen für WakeLock)
    const stream = canvas.captureStream(2); // 2 FPS
    dummyVideo = document.createElement("video");
    dummyVideo.muted = true;
    dummyVideo.playsInline = true;
    dummyVideo.srcObject = stream;
    dummyVideo.width = 80;
    dummyVideo.height = 60;
    dummyVideo.style.position = "fixed";
    dummyVideo.style.left = "-9999px"; // Offscreen
    dummyVideo.style.top = "-9999px";
    dummyVideo.style.width = "80px";
    dummyVideo.style.height = "60px";
    dummyVideo.style.opacity = "0";
    dummyVideo.style.pointerEvents = "none";
    dummyVideo.style.zIndex = "-1";
    document.body.appendChild(dummyVideo);
    dummyVideo.play().catch(() => {});

    // Stop-Logik für Canvas und Video
    dummyVideo._stopDummy = function () {
        running = false;
        canvas.remove();
        dummyVideo.pause();
        dummyVideo.remove();
        dummyVideo = null;
    };
}

function stopDummyVideo() {
    if (dummyVideo && typeof dummyVideo._stopDummy === "function") {
        dummyVideo._stopDummy();
    }
}
