import WSManager from "./wsManager.js";
// Globale Variablen
let selfMac = "";
let selfRole = "";
let savedDevices = [];
let discoveredDevices = [];
let wsManager;

window.saveDeviceRole = saveDeviceRole;

// Initialisierung
document.addEventListener("DOMContentLoaded", function () {
    // Felder und Buttons initial leeren und deaktivieren
    const minInput = document.getElementById("minDistanceInput");
    const maxInput = document.getElementById("maxDistanceInput");
    const minBtn = document.getElementById("saveMinDistanceBtn");
    const maxBtn = document.getElementById("saveMaxDistanceBtn");
    if (minInput) {
        minInput.value = "";
        minInput.placeholder = "...";
    }
    if (maxInput) {
        maxInput.value = "";
        maxInput.placeholder = "...";
    }
    if (minBtn) minBtn.disabled = true;
    if (maxBtn) maxBtn.disabled = true;

    // WebSocket zentral initialisieren
    wsManager = new WSManager(
        "ws://" + location.host + "/ws",
        function (event) {
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
            } else if (msg.type === "device_role_changed") {
                handleDeviceRoleChanged(msg.data);
            }
        },
        function () {
            console.log("WebSocket connected");
        },
        function () {
            console.log("WebSocket disconnected");
        },
        function (error) {
            console.error("WebSocket error:", error);
        }
    );

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
    const versionSpan = document.createElement("span");
    versionSpan.id = "version-text";
    versionSpan.textContent = "Lädt...";
    versionDiv.appendChild(versionSpan);
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
            document.getElementById("version-text").textContent =
                "FW: error | FS: error";
        });
}

function loadDeviceInfo() {
    // Lade Device Info über API
    fetch("/api/device_info")
        .then((response) => response.json())
        .then((data) => {
            selfMac = data.selfMac;
            selfRole = data.selfRole;
            // Show/hide settings based on role
            updateUIForRole(selfRole);
            // Lade rollenspezifische Einstellungen NACH dem Setzen der Rolle
            loadRoleSpecificSettings();
            showAllDevices(); // Aktualisiere Anzeige wenn MAC bekannt ist
        })
        .catch((err) => console.log("Fehler beim Laden der Device-Info:", err));

    // Lade Gerätepräferenzen
    loadDevicePreferences();
}

function updateUIForRole(role) {
    const sensorSettings = document.getElementById("sensorSettings");
    const brightnessSettings = document.getElementById("brightnessSettings");

    if (role === "Start" || role === "Ziel") {
        // Show sensor settings for Start/Ziel devices
        sensorSettings.classList.remove("hidden");
        brightnessSettings.classList.add("hidden");
    } else if (role === "Anzeige") {
        // Show brightness settings for display devices
        sensorSettings.classList.add("hidden");
        brightnessSettings.classList.remove("hidden");
    } else {
        // Hide both for unknown/ignore devices
        sensorSettings.classList.add("hidden");
        brightnessSettings.classList.add("hidden");
    }
}

function loadRoleSpecificSettings() {
    // Lade Distanz-Einstellungen für Start/Ziel
    if (selfRole === "Start" || selfRole === "Ziel") {
        loadDistanceSettings();
    }

    // Lade Helligkeit-Einstellungen für Anzeige
    if (selfRole === "Anzeige") {
        loadBrightnessSettings();
    }
}

function loadBrightnessSettings() {
    fetch("/get_brightness")
        .then((response) => response.json())
        .then((data) => {
            const brightnessInput = document.getElementById("brightnessInput");
            const brightnessValue = document.getElementById("brightnessValue");
            if (brightnessInput && brightnessValue) {
                brightnessInput.value = data.brightness;
                brightnessValue.textContent = data.brightness;
            }
        })
        .catch((err) => console.log("Fehler beim Laden der Helligkeit:", err));
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

    // Brightness Input with auto-save
    const brightnessInput = document.getElementById("brightnessInput");
    const brightnessValue = document.getElementById("brightnessValue");
    let brightnessTimeout = null; // Für debounced saving

    if (brightnessInput && brightnessValue) {
        brightnessInput.addEventListener("input", function () {
            // Update display value
            brightnessValue.textContent = brightnessInput.value;

            // Clear existing timeout
            if (brightnessTimeout) {
                clearTimeout(brightnessTimeout);
            }

            // Set new timeout for auto-save after 500ms of no changes
            brightnessTimeout = setTimeout(() => {
                saveBrightnessAutomatic(brightnessInput.value);
            }, 500);
        });

        brightnessInput.addEventListener("change", function () {
            // Immediate save on change (when user releases slider)
            if (brightnessTimeout) {
                clearTimeout(brightnessTimeout);
            }
            saveBrightnessAutomatic(brightnessInput.value);
        });
    }

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

    // Reset ESP Button
    const resetEspBtn = document.getElementById("resetEspBtn");
    if (resetEspBtn) {
        resetEspBtn.onclick = function () {
            if (confirm("ESP wirklich neustarten?")) {
                resetEsp();
            }
        };
    }
    // ESP Reset Funktion
    function resetEsp() {
        fetch("/reset_esp", { method: "POST" })
            .then((r) => r.text())
            .then((msg) => {
                alert(msg || "ESP wird neugestartet.");
            })
            .catch(() => {
                alert("Fehler beim Neustart des ESP.");
            });
    }
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
let originalMinDistance = 2;
let originalMaxDistance = 100;

function loadDistanceSettings() {
    fetch("/get_distance_settings")
        .then((response) => response.json())
        .then((data) => {
            originalMinDistance = data.minDistance;
            originalMaxDistance = data.maxDistance;
            const minInput = document.getElementById("minDistanceInput");
            const maxInput = document.getElementById("maxDistanceInput");
            minInput.value = data.minDistance;
            maxInput.value = data.maxDistance;
            minInput.placeholder = data.minDistance;
            maxInput.placeholder = data.maxDistance;
            document.getElementById("saveMinDistanceBtn").disabled = false;
            document.getElementById("saveMaxDistanceBtn").disabled = false;
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
    const currentValue = parseInt(input.value);

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
    const currentValue = parseInt(input.value);

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
    let minDistance = parseInt(
        document.getElementById("minDistanceInput").value
    );
    let maxDistance = parseInt(
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
    let minDistance = parseInt(
        document.getElementById("minDistanceInput").value
    );
    let maxDistance = parseInt(
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

    // Sammle alle pending changes bevor wir die Liste neu erstellen
    let pendingChanges = {};
    container
        .querySelectorAll('select[data-pending-change="true"]')
        .forEach((select) => {
            let mac = select.closest("[data-mac]").dataset.mac;
            pendingChanges[mac] = {
                backgroundColor: select.style.backgroundColor,
                color: select.style.color,
            };
        });

    container.innerHTML = "";

    // Debug: Zeige verfügbare Geräte in der Konsole
    console.log("Available devices:", {
        savedDevices: savedDevices,
        discoveredDevices: discoveredDevices,
        selfMac: selfMac,
    });

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

    // Stelle pending changes wieder her
    Object.keys(pendingChanges).forEach((mac) => {
        let selectElement = container.querySelector(
            `[data-mac="${mac}"] select`
        );
        if (selectElement) {
            selectElement.style.backgroundColor =
                pendingChanges[mac].backgroundColor;
            selectElement.style.color = pendingChanges[mac].color;
            selectElement.setAttribute("data-pending-change", "true");
        }
    });
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

    let roleOptions = (
        isSelf
            ? ["Start", "Ziel", "Anzeige"]
            : ["-", "Start", "Ziel", "Anzeige"]
    )
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

    console.log("Saving device role:", { mac, role, isSelf });

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
        const requestBody =
            "mac=" +
            encodeURIComponent(mac) +
            "&role=" +
            encodeURIComponent(role);
        console.log("Request body:", requestBody);

        fetch("/change_device", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: requestBody,
        })
            .then((response) => {
                if (response.ok) {
                    return response.text().then((message) => {
                        selectElement.classList.remove("pending");
                        selectElement.disabled = false;

                        if (message.includes("Anfrage gesendet")) {
                            // Zeige temporär an, dass auf Bestätigung gewartet wird
                            selectElement.style.backgroundColor = "#fff3cd";
                            selectElement.style.color = "#856404";
                            selectElement.setAttribute(
                                "data-pending-change",
                                "true"
                            );
                            console.log(
                                "Rollenänderung angefragt, warte auf Bestätigung..."
                            );

                            // Keine automatische Zurücksetzung - warte auf WebSocket Update
                        } else {
                            showAllDevices();
                        }
                    });
                } else {
                    // Zeige die tatsächliche Fehlermeldung vom Server an
                    response
                        .text()
                        .then((errorMsg) => {
                            selectElement.classList.remove("pending");
                            selectElement.disabled = false;
                            console.error("Server error:", errorMsg);
                            alert(
                                "Fehler beim Speichern des Geräts: " + errorMsg
                            );
                        })
                        .catch(() => {
                            selectElement.classList.remove("pending");
                            selectElement.disabled = false;
                            alert(
                                "Fehler beim Speichern des Geräts (unbekannter Fehler)"
                            );
                        });
                }
            })
            .catch((error) => {
                selectElement.classList.remove("pending");
                selectElement.disabled = false;
                console.error("Network error:", error);
                alert("Netzwerkfehler beim Speichern des Geräts");
            });
    }
}

function discoverDevices() {
    fetch("/discover", { method: "POST" });
}

// Funktion zum Behandeln von Änderungen der Geräte-Rolle über WebSocket
function handleDeviceRoleChanged(data) {
    console.log("Device role changed confirmed:", data);

    // Update the device in both saved and discovered devices arrays
    const mac = data.mac;
    const newRole = data.role;

    // Update in savedDevices array
    const savedDevice = savedDevices.find((device) => device.mac === mac);
    if (savedDevice) {
        savedDevice.role = newRole;
    }

    // Update in discoveredDevices array
    const discoveredDevice = discoveredDevices.find(
        (device) => device.mac === mac
    );
    if (discoveredDevice) {
        discoveredDevice.role = newRole;
    }

    // Remove pending state from the corresponding select element
    const selectElement = document.querySelector(`[data-mac="${mac}"] select`);
    if (selectElement) {
        selectElement.style.backgroundColor = "";
        selectElement.style.color = "";
        selectElement.removeAttribute("data-pending-change");
        console.log("Pending state removed for device:", mac);
    }

    // Refresh the UI to show the updated role
    showAllDevices();
}

// Brightness functions with automatic saving
function saveBrightnessAutomatic(brightness) {
    const value = parseInt(brightness);
    if (value < 1 || value > 15) {
        console.error("Ungültiger Helligkeitswert:", value);
        return;
    }

    const formData = new FormData();
    formData.append("brightness", value);

    fetch("/set_brightness", {
        method: "POST",
        body: formData,
    })
        .then((response) => response.text())
        .then((message) => {
            console.log("Helligkeit automatisch gespeichert:", message);
            // Kein Alert - stille Speicherung
        })
        .catch((error) => {
            console.error(
                "Fehler beim automatischen Speichern der Helligkeit:",
                error
            );
            // Kein Alert - nur Console-Log für Debug-Zwecke
        });
}
