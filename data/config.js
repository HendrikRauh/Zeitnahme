// Globale Variablen
let selfMac = "";
let selfRole = "";
let savedDevices = [];
let discoveredDevices = [];

// WebSocket Connection
let ws = new WebSocket("ws://" + location.host + "/ws");

// Initialisierung
document.addEventListener("DOMContentLoaded", function () {
    // Geräteinformationen laden
    loadDeviceInfo();

    // Event Listeners
    setupEventListeners();

    // Geräte automatisch suchen
    discoverDevices();

    // Version-Anzeige
    showVersion();
});

function showVersion() {
    const versionDiv = document.createElement("div");
    versionDiv.className = "version-info";
    versionDiv.innerHTML = '<span id="version-text">Lädt...</span>';
    document.body.appendChild(versionDiv);

    // Firmware-Hash aus device_info holen
    fetch("/api/device_info")
        .then((response) => response.json())
        .then((data) => {
            const fwHash = data.firmware_hash
                ? data.firmware_hash.substring(0, 8)
                : "unknown";
            const fsHash = data.filesystem_hash
                ? data.filesystem_hash.substring(0, 8)
                : "unknown";
            document.getElementById(
                "version-text"
            ).textContent = `FW: ${fwHash} | FS: ${fsHash}`;
        })
        .catch(() => {
            document.getElementById("version-text").textContent = "FW: error";
        });
}

function loadDeviceInfo() {
    // Lade Device Info über API
    fetch("/api/device_info")
        .then((response) => response.json())
        .then((data) => {
            selfMac = data.selfMac;
            selfRole = data.selfRole;
            showAllDevices(); // Aktualisiere Anzeige wenn MAC bekannt ist
        })
        .catch((err) => console.log("Fehler beim Laden der Device-Info:", err));

    // Lade Distanz-Einstellungen
    loadDistanceSettings();

    // Lade Gerätepräferenzen
    loadDevicePreferences();
}

function loadDevicePreferences() {
    fetch("/preferences")
        .then((response) => response.text())
        .then((data) => {
            // Parse die Antwort vom Format "S: {...}\nD: {...}\nR: ..."
            const lines = data.split("\n");
            const savedLine = lines.find((l) => l.startsWith("S: "));
            const discoveredLine = lines.find((l) => l.startsWith("D: "));

            if (savedLine) {
                try {
                    savedDevices = JSON.parse(savedLine.substring(3));
                } catch (e) {
                    savedDevices = [];
                }
            }

            if (discoveredLine) {
                try {
                    discoveredDevices = JSON.parse(discoveredLine.substring(3));
                } catch (e) {
                    discoveredDevices = [];
                }
            }

            showAllDevices(); // Aktualisiere Anzeige
        })
        .catch((err) =>
            console.log("Fehler beim Laden der Geräteinformationen:", err)
        );
}

function setupEventListeners() {
    // WebSocket Events
    ws.onmessage = function (event) {
        let msg = {};
        try {
            msg = JSON.parse(event.data);
        } catch (e) {
            return;
        }

        if (msg.type === "saved_devices") {
            savedDevices = msg.data;
            showAllDevices();
        } else if (msg.type === "discovered_devices") {
            discoveredDevices = msg.data;
            showAllDevices();
        } else if (msg.type === "status") {
            updateStatusDisplay(msg.status);
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

    // Min Distance Input and Button
    const minDistanceInput = document.getElementById("minDistanceInput");
    minDistanceInput.addEventListener("input", updateMinDistanceButton);
    minDistanceInput.addEventListener("change", updateMinDistanceButton);

    document.getElementById("saveMinDistanceBtn").onclick = function () {
        saveMinDistance();
    };

    // Max Distance Input and Button
    const maxDistanceInput = document.getElementById("maxDistanceInput");
    maxDistanceInput.addEventListener("input", updateMaxDistanceButton);
    maxDistanceInput.addEventListener("change", updateMaxDistanceButton);

    document.getElementById("saveMaxDistanceBtn").onclick = function () {
        saveMaxDistance();
    };

    // Reset Button
    document.getElementById("resetBtn").onclick = function () {
        if (
            confirm(
                "Wirklich alle Einstellungen löschen? Das Gerät muss danach neu konfiguriert werden!"
            )
        ) {
            fetch("/reset", { method: "POST" })
                .then((r) => r.text())
                .then((msg) => {
                    alert(msg);
                    location.reload();
                });
        }
    };

    // Refresh Devices Button
    document.getElementById("refreshDevicesBtn").onclick = function () {
        // Neue Geräte suchen
        discoverDevices();
        // Gespeicherte Geräte neu laden
        loadDevicePreferences();
    };
}

function updateStatusDisplay(status) {
    let emoji = document.getElementById("statusEmoji");
    let text = document.getElementById("statusText");

    switch (status) {
        case "normal":
            emoji.textContent = "🟢";
            emoji.className = "not-triggered";
            text.textContent = "Bereit";
            break;
        case "triggered":
            emoji.textContent = "🔴";
            emoji.className = "triggered";
            text.textContent = "Ausgelöst";
            break;
        case "cooldown":
            emoji.textContent = "🟡";
            emoji.className = "cooldown";
            text.textContent = "Cooldown";
            break;
        case "triggered_in_cooldown":
            emoji.textContent = "🟠";
            emoji.className = "cooldown";
            text.textContent = "Ausgelöst (Cooldown)";
            break;
        default:
            emoji.textContent = "⚪";
            emoji.className = "unknown";
            text.textContent = "Unbekannt";
    }
}

// Distance Management
let originalMinDistance = 2.0;
let originalMaxDistance = 100.0;

function loadDistanceSettings() {
    fetch("/get_distance_settings")
        .then((response) => response.json())
        .then((data) => {
            originalMinDistance = data.minDistance;
            originalMaxDistance = data.maxDistance;
            document.getElementById("minDistanceInput").value =
                data.minDistance;
            document.getElementById("maxDistanceInput").value =
                data.maxDistance;
            updateMinDistanceButton();
            updateMaxDistanceButton();
        })
        .catch((err) =>
            console.log("Fehler beim Laden der Distanz-Einstellungen:", err)
        );
}

function updateMinDistanceButton() {
    const input = document.getElementById("minDistanceInput");
    const button = document.getElementById("saveMinDistanceBtn");
    const currentValue = parseFloat(input.value);

    if (isNaN(currentValue) || currentValue === originalMinDistance) {
        button.disabled = true;
        button.classList.remove("changed");
        button.textContent = "Speichern";
    } else {
        button.disabled = false;
        button.classList.add("changed");
        button.textContent = "Speichern";
    }
}

function updateMaxDistanceButton() {
    const input = document.getElementById("maxDistanceInput");
    const button = document.getElementById("saveMaxDistanceBtn");
    const currentValue = parseFloat(input.value);

    if (isNaN(currentValue) || currentValue === originalMaxDistance) {
        button.disabled = true;
        button.classList.remove("changed");
        button.textContent = "Speichern";
    } else {
        button.disabled = false;
        button.classList.add("changed");
        button.textContent = "Speichern";
    }
}

function saveMinDistance() {
    let minDistance = parseFloat(
        document.getElementById("minDistanceInput").value
    );
    let maxDistance = parseFloat(
        document.getElementById("maxDistanceInput").value
    );

    if (isNaN(minDistance) || minDistance < 2 || minDistance > 200) {
        showInputError("minDistanceInput");
        return;
    }

    if (minDistance >= maxDistance) {
        alert("Minimal-Distanz muss kleiner als Maximal-Distanz sein!");
        showInputError("minDistanceInput");
        return;
    }

    const btn = document.getElementById("saveMinDistanceBtn");
    const originalText = btn.textContent;
    btn.textContent = "Speichere...";
    btn.disabled = true;
    btn.classList.remove("changed");

    fetch("/set_min_distance", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "minDistance=" + minDistance,
    })
        .then((response) => response.text())
        .then((text) => {
            originalMinDistance = minDistance;

            btn.textContent = "✓ Gespeichert";
            btn.classList.remove("changed");
            btn.classList.add("success");
            btn.disabled = true;

            setTimeout(() => {
                btn.classList.remove("success");
                updateMinDistanceButton();
            }, 2000);
        })
        .catch((err) => {
            btn.textContent = originalText;
            btn.classList.add("changed");
            updateMinDistanceButton();
        });
}

function saveMaxDistance() {
    let minDistance = parseFloat(
        document.getElementById("minDistanceInput").value
    );
    let maxDistance = parseFloat(
        document.getElementById("maxDistanceInput").value
    );

    if (isNaN(maxDistance) || maxDistance < 2 || maxDistance > 200) {
        showInputError("maxDistanceInput");
        return;
    }

    if (maxDistance <= minDistance) {
        alert("Maximal-Distanz muss größer als Minimal-Distanz sein!");
        showInputError("maxDistanceInput");
        return;
    }

    const btn = document.getElementById("saveMaxDistanceBtn");
    const originalText = btn.textContent;
    btn.textContent = "Speichere...";
    btn.disabled = true;
    btn.classList.remove("changed");

    fetch("/set_max_distance", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: "maxDistance=" + maxDistance,
    })
        .then((response) => response.text())
        .then((text) => {
            originalMaxDistance = maxDistance;

            btn.textContent = "✓ Gespeichert";
            btn.classList.remove("changed");
            btn.classList.add("success");
            btn.disabled = true;

            setTimeout(() => {
                btn.classList.remove("success");
                updateMaxDistanceButton();
            }, 2000);
        })
        .catch((err) => {
            btn.textContent = originalText;
            btn.classList.add("changed");
            updateMaxDistanceButton();
        });
}

function showInputError(inputId) {
    const input = document.getElementById(inputId);
    const originalBorder = input.style.borderColor;
    input.style.borderColor = "#e53935";
    input.style.boxShadow = "0 0 5px rgba(229, 57, 53, 0.3)";

    setTimeout(() => {
        input.style.borderColor = originalBorder;
        input.style.boxShadow = "";
    }, 2000);
}

// Device Management
function getAllDevices() {
    let all = [...savedDevices];
    discoveredDevices.forEach((dev) => {
        if (!all.some((d) => d.mac === dev.mac)) {
            all.push(dev);
        }
    });
    return all;
}

function showAllDevices() {
    let container = document.getElementById("devicesContainer");
    container.innerHTML = "";

    // Eigenes Gerät zuerst (wenn bekannt)
    if (selfMac) {
        let selfDev = { mac: selfMac, role: selfRole };
        let otherDevs = getAllDevices().filter((d) => d.mac !== selfMac);

        createDeviceItem(container, selfDev, true);
        otherDevs.forEach((dev) => {
            createDeviceItem(container, dev, false);
        });
    } else {
        // Fallback: Alle Geräte anzeigen
        getAllDevices().forEach((dev) => {
            createDeviceItem(container, dev, false);
        });
    }
}

function createDeviceItem(container, dev, isSelf) {
    let saved = savedDevices.find((d) => d.mac === dev.mac);
    let role = saved ? saved.role : isSelf ? dev.role : "-";
    let isSaved = !!saved || isSelf;
    let isOnline = discoveredDevices.some((d) => d.mac === dev.mac) || isSelf;

    let deviceClass = "device-item";
    let statusIcon = "";
    let statusText = "";

    if (isSelf) {
        deviceClass += " self";
        statusIcon = "📱";
        statusText = "Dieses Gerät";
    } else if (isSaved && isOnline) {
        deviceClass += " saved-online";
        statusIcon = "🟢";
        statusText = "Gespeichert & Online";
    } else if (isSaved && !isOnline) {
        deviceClass += " saved-offline";
        statusIcon = "🔴";
        statusText = "Gespeichert & Offline";
    } else if (!isSaved && isOnline) {
        deviceClass += " discovered";
        statusIcon = "🟡";
        statusText = "Entdeckt";
    } else {
        statusIcon = "⚪";
        statusText = "Unbekannt";
    }

    let roleOptions = (isSelf ? ["Start", "Ziel"] : ["-", "Start", "Ziel"])
        .map(
            (opt) =>
                `<option value="${opt}"${
                    role === opt ? " selected" : ""
                }>${opt}</option>`
        )
        .join("");

    let deviceItem = document.createElement("div");
    deviceItem.className = deviceClass;
    deviceItem.dataset.mac = dev.mac;
    deviceItem.innerHTML = `
    <div class="device-info">
      <div class="device-mac">${dev.mac}</div>
      <div class="device-status">
        <span class="device-status-icon">${statusIcon}</span>
        <span>${statusText}</span>
      </div>
    </div>
    <select class="device-role-select" onchange="saveDeviceRole(this, '${dev.mac}', ${isSelf})">${roleOptions}</select>
  `;
    container.appendChild(deviceItem);
}

function saveDeviceRole(selectElement, mac, isSelf) {
    let role = selectElement.value;

    selectElement.classList.add("pending");
    selectElement.disabled = true;

    if (isSelf) {
        fetch("/config", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: "role=" + encodeURIComponent(role),
        })
            .then((response) => {
                if (response.ok) {
                    setTimeout(() => {
                        location.reload();
                    }, 500);
                } else {
                    selectElement.classList.remove("pending");
                    selectElement.disabled = false;
                    alert("Fehler beim Speichern der Rolle");
                }
            })
            .catch(() => {
                selectElement.classList.remove("pending");
                selectElement.disabled = false;
                alert("Fehler beim Speichern der Rolle");
            });
    } else {
        fetch("/change_device", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body:
                "mac=" +
                encodeURIComponent(mac) +
                "&role=" +
                encodeURIComponent(role),
        })
            .then((response) => {
                selectElement.classList.remove("pending");
                selectElement.disabled = false;
                if (response.ok) {
                    showAllDevices();
                } else {
                    alert("Fehler beim Speichern des Geräts");
                }
            })
            .catch(() => {
                selectElement.classList.remove("pending");
                selectElement.disabled = false;
                alert("Fehler beim Speichern des Geräts");
            });
    }
}

function discoverDevices() {
    fetch("/discover", { method: "POST" });
}
