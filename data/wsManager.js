// wsManager.js
// Zentrale WebSocket-Verwaltung für die Anwendung

class WSManager {
    constructor(url, onMessage, onOpen, onClose, onError) {
        this.url = url;
        this.onMessage = onMessage;
        this.onOpen = onOpen;
        this.onClose = onClose;
        this.onError = onError;
        this.ws = null;
        this.reconnectDelay = 2000;
        this.maxReconnectDelay = 30000;
        this.reconnectAttempts = 0;
        this.isUnloading = false;
        window.addEventListener("beforeunload", () => {
            this.isUnloading = true;
        });
        this.connect();
    }

    connect() {
        this.ws = new WebSocket(this.url);
        this.ws.onopen = (event) => {
            this.reconnectAttempts = 0;
            this.hideError();
            if (this.onOpen) this.onOpen(event);
        };
        this.ws.onmessage = (event) => {
            if (this.onMessage) this.onMessage(event);
        };
        this.ws.onclose = (event) => {
            if (!this.isUnloading) {
                this.showError(
                    "Verbindung zum Server verloren. Versuche erneut..."
                );
                this.scheduleReconnect();
            }
            if (this.onClose) this.onClose(event);
        };
        this.ws.onerror = (event) => {
            this.showError("WebSocket Fehler. Versuche erneut...");
            if (this.onError) this.onError(event);
            this.ws.close();
        };
    }

    send(data) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(data);
            return true;
        } else {
            return false;
        }
    }

    scheduleReconnect() {
        this.reconnectAttempts = Math.min(this.reconnectAttempts + 1, 5); // Cap at 5 attempts
        let delay = Math.min(
            this.reconnectDelay * 2 ** this.reconnectAttempts,
            this.maxReconnectDelay
        );
        setTimeout(() => {
            this.connect();
        }, delay);
    }

    showError(msg) {
        let errorDiv = document.getElementById("ws-error-message");
        if (!errorDiv) {
            errorDiv = document.createElement("div");
            errorDiv.id = "ws-error-message";
            document.body.appendChild(errorDiv);
        }
        errorDiv.textContent = msg;
    }

    hideError() {
        let errorDiv = document.getElementById("ws-error-message");
        if (errorDiv) {
            errorDiv.remove();
        }
    }
}

export default WSManager;
