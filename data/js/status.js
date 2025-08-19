/* ESP32-CAM Status Updates */

// Global variables
let statusUpdateTimeout = null;


// Status update function
function updateStatus() {

    fetch('/status')
        .then(r => r.json())
        .then(d => {
            const resolution = d.currentWidth && d.currentHeight ? 
                ' | Resolution: ' + d.currentWidth + 'x' + d.currentHeight : '';
            
            document.getElementById('status').textContent = 
                'Camera FPS: ' + d.cameraFPS + 
                ' | Frame: ' + d.frameSize + 'KB' + 
                ' | TCP: ' + d.clients + 
                ' | WiFi: ' + d.wifiRSSI + 'dBm Ch' + d.wifiChannel + 
                ' ' + d.wifiPHY + resolution;

            updateStatusLoop()
        })
        .catch(e => {
            document.getElementById('status').textContent = 'Status: Connection Error';
            updateStatusLoop()
        });

    function updateStatusLoop() {
        if (statusUpdateTimeout) clearTimeout(statusUpdateTimeout);
        statusUpdateTimeout = setTimeout(updateStatus, 5000);
    }
}

setTimeout(updateStatus, 100);

