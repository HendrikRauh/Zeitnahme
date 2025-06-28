// Globale Variablen
let selfMac = "";
let selfRole = "";
let savedDevices = [];
let discoveredDevices = [];

// WebSocket Connection
let ws = new WebSocket('ws://' + location.host + '/ws');

// Initialisierung
document.addEventListener('DOMContentLoaded', function() {
  // Geräteinformationen laden
  loadDeviceInfo();
  
  // Event Listeners
  setupEventListeners();
  
  // Geräte automatisch suchen
  discoverDevices();
});

function loadDeviceInfo() {
  // Lade Device Info über API
  fetch('/api/device_info')
    .then(response => response.json())
    .then(data => {
      selfMac = data.selfMac;
      selfRole = data.selfRole;
      showAllDevices(); // Aktualisiere Anzeige wenn MAC bekannt ist
    })
    .catch(err => console.log('Fehler beim Laden der Device-Info:', err));

  fetch('/preferences')
    .then(response => response.text())
    .then(data => {
      // Parse die Antwort vom Format "S: {...}\nD: {...}\nR: ..."
      const lines = data.split('\n');
      const savedLine = lines.find(l => l.startsWith('S: '));
      const discoveredLine = lines.find(l => l.startsWith('D: '));
      
      if (savedLine) {
        try {
          savedDevices = JSON.parse(savedLine.substring(3));
        } catch(e) {
          savedDevices = [];
        }
      }
      
      if (discoveredLine) {
        try {
          discoveredDevices = JSON.parse(discoveredLine.substring(3));
        } catch(e) {
          discoveredDevices = [];
        }
      }
      
      showAllDevices(); // Aktualisiere Anzeige
    })
    .catch(err => console.log('Fehler beim Laden der Geräteinformationen:', err));
}

function setupEventListeners() {
  // WebSocket Events
  ws.onmessage = function(event) {
    let msg = {};
    try { 
      msg = JSON.parse(event.data); 
    } catch(e) {
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

  ws.onopen = function() {
    console.log('WebSocket connected');
  };

  ws.onclose = function() {
    console.log('WebSocket disconnected');
  };

  ws.onerror = function(error) {
    console.error('WebSocket error:', error);
  };

  // Rekalibrierung Button
  document.getElementById('recalibrateBtn').onclick = function() {
    const btn = this;
    const originalText = btn.textContent;
    btn.textContent = 'Kalibriere...';
    btn.disabled = true;
    btn.classList.remove('success');
    
    fetch('/recalibrate', {method: 'POST'})
      .then(response => response.text())
      .then(text => {
        loadBaseDistance();
        
        btn.textContent = '✓ Kalibriert';
        btn.disabled = true;
        btn.classList.add('success');
        
        setTimeout(() => {
          btn.textContent = originalText;
          btn.classList.remove('success');
          btn.disabled = false;
        }, 2000);
      })
      .catch(() => {
        btn.textContent = originalText;
        btn.classList.remove('success');
        btn.disabled = false;
      });
  };

  // Threshold Input
  const thresholdInput = document.getElementById('thresholdInput');
  thresholdInput.addEventListener('input', updateThresholdButton);
  thresholdInput.addEventListener('change', updateThresholdButton);

  // Threshold Save Button
  document.getElementById('saveThresholdBtn').onclick = function() {
    saveThreshold();
  };

  // Reset Button
  document.getElementById('resetBtn').onclick = function() {
    if (confirm("Wirklich alle Einstellungen löschen? Das Gerät muss danach neu konfiguriert werden!")) {
      fetch('/reset', {method: 'POST'})
        .then(r => r.text())
        .then(msg => {
          alert(msg);
          location.reload();
        });
    }
  };
}

function updateStatusDisplay(status) {
  let emoji = document.getElementById('statusEmoji');
  let text = document.getElementById('statusText');
  
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

// Threshold Management
let originalThreshold = 50.0;

function loadThreshold() {
  fetch('/get_threshold')
    .then(response => response.json())
    .then(data => {
      originalThreshold = data.threshold;
      document.getElementById('thresholdInput').value = data.threshold;
      updateThresholdButton();
    })
    .catch(err => console.log('Fehler beim Laden des Threshold-Werts:', err));
}

function loadBaseDistance() {
  fetch('/get_base_distance')
    .then(response => response.json())
    .then(data => {
      const baseDistanceElement = document.getElementById('baseDistanceValue');
      if (data.isMaxRange) {
        baseDistanceElement.textContent = 'Kein Hindernis';
        baseDistanceElement.style.color = '#888';
        baseDistanceElement.style.fontStyle = 'italic';
      } else {
        baseDistanceElement.textContent = data.baseDistance.toFixed(1) + ' cm';
        baseDistanceElement.style.color = '';
        baseDistanceElement.style.fontStyle = '';
      }
    })
    .catch(err => console.log('Fehler beim Laden der Basisdistanz:', err));
}

function updateThresholdButton() {
  const input = document.getElementById('thresholdInput');
  const button = document.getElementById('saveThresholdBtn');
  const currentValue = parseFloat(input.value);
  
  if (isNaN(currentValue) || currentValue === originalThreshold) {
    button.disabled = true;
    button.classList.remove('changed');
    button.textContent = 'Schwelle speichern';
  } else {
    button.disabled = false;
    button.classList.add('changed');
    button.textContent = 'Schwelle speichern';
  }
}

function saveThreshold() {
  let threshold = parseFloat(document.getElementById('thresholdInput').value);
  if (isNaN(threshold) || threshold <= 0 || threshold > 200) {
    const input = document.getElementById('thresholdInput');
    const originalBorder = input.style.borderColor;
    input.style.borderColor = '#e53935';
    input.style.boxShadow = '0 0 5px rgba(229, 57, 53, 0.3)';
    
    setTimeout(() => {
      input.style.borderColor = originalBorder;
      input.style.boxShadow = '';
    }, 2000);
    return;
  }
  
  const btn = document.getElementById('saveThresholdBtn');
  const originalText = btn.textContent;
  btn.textContent = 'Speichere...';
  btn.disabled = true;
  btn.classList.remove('changed');
  
  fetch('/set_threshold', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'threshold=' + threshold
  })
    .then(response => response.text())
    .then(text => {
      originalThreshold = threshold;
      
      btn.textContent = '✓ Gespeichert';
      btn.classList.remove('changed');
      btn.classList.add('success');
      btn.disabled = true;
      
      setTimeout(() => {
        btn.classList.remove('success');
        updateThresholdButton();
      }, 2000);
    })
    .catch(err => {
      btn.textContent = originalText;
      btn.classList.add('changed');
      updateThresholdButton();
    });
}

// Device Management
function getAllDevices() {
  let all = [...savedDevices];
  discoveredDevices.forEach(dev => {
    if (!all.some(d => d.mac === dev.mac)) {
      all.push(dev);
    }
  });
  return all;
}

function showAllDevices() {
  let container = document.getElementById('devicesContainer');
  container.innerHTML = '';

  // Eigenes Gerät zuerst (wenn bekannt)
  if (selfMac) {
    let selfDev = {mac: selfMac, role: selfRole};
    let otherDevs = getAllDevices().filter(d => d.mac !== selfMac);

    createDeviceItem(container, selfDev, true);
    otherDevs.forEach(dev => {
      createDeviceItem(container, dev, false);
    });
  } else {
    // Fallback: Alle Geräte anzeigen
    getAllDevices().forEach(dev => {
      createDeviceItem(container, dev, false);
    });
  }
}

function createDeviceItem(container, dev, isSelf) {
  let saved = savedDevices.find(d => d.mac === dev.mac);
  let role = saved ? saved.role : (isSelf ? dev.role : "-");
  let isSaved = !!saved || isSelf;
  let isOnline = discoveredDevices.some(d => d.mac === dev.mac) || isSelf;

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

  let roleOptions = (isSelf ? ['Start','Ziel'] : ['-','Start','Ziel']).map(opt =>
    `<option value="${opt}"${role===opt?' selected':''}>${opt}</option>`
  ).join('');

  let deviceItem = document.createElement('div');
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
  
  selectElement.classList.add('pending');
  selectElement.disabled = true;
  
  if (isSelf) {
    fetch('/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'role=' + encodeURIComponent(role)
    }).then(response => {
      if (response.ok) {
        setTimeout(() => {
          location.reload();
        }, 500);
      } else {
        selectElement.classList.remove('pending');
        selectElement.disabled = false;
        alert('Fehler beim Speichern der Rolle');
      }
    }).catch(() => {
      selectElement.classList.remove('pending');
      selectElement.disabled = false;
      alert('Fehler beim Speichern der Rolle');
    });
  } else {
    fetch('/change_device', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'mac=' + encodeURIComponent(mac) + '&role=' + encodeURIComponent(role)
    }).then(response => {
      selectElement.classList.remove('pending');
      selectElement.disabled = false;
      if (response.ok) {
        showAllDevices();
      } else {
        alert('Fehler beim Speichern des Geräts');
      }
    }).catch(() => {
      selectElement.classList.remove('pending');
      selectElement.disabled = false;
      alert('Fehler beim Speichern des Geräts');
    });
  }
}

function discoverDevices() {
  fetch('/discover', {method: 'POST'});
}

// Initialisierung nach DOM-Load
document.addEventListener('DOMContentLoaded', function() {
  loadThreshold();
  loadBaseDistance();
});
