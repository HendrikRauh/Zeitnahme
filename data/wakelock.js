let wakeLock = null;
async function requestWakeLock() {
    try {
        if ("wakeLock" in navigator) {
            wakeLock = await navigator.wakeLock.request("screen");
            wakeLock.addEventListener("release", () => {
                console.log("Wake Lock wurde freigegeben");
            });
            console.log("Wake Lock aktiviert");
        } else {
            console.log("Wake Lock API nicht unterstützt");
        }
    } catch (err) {
        console.error(`${err.name}, ${err.message}`);
    }
}

async function enableFullscreenAndWakeLock() {
    // Fullscreen
    const docElm = document.documentElement;
    if (docElm.requestFullscreen) {
        await docElm.requestFullscreen();
    } else if (docElm.webkitRequestFullscreen) {
        /* Safari */
        await docElm.webkitRequestFullscreen();
    } else if (docElm.msRequestFullscreen) {
        /* IE11 */
        await docElm.msRequestFullscreen();
    }
    // Wake Lock
    await requestWakeLock();
}

document
    .getElementById("wakelock-btn")
    .addEventListener("click", enableFullscreenAndWakeLock);

// Optional: Wake Lock automatisch erneut anfordern, wenn die Seite sichtbar wird
document.addEventListener("visibilitychange", () => {
    if (wakeLock !== null && document.visibilityState === "visible") {
        requestWakeLock();
    }
});
