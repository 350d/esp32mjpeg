/* ESP32-CAM Control Panel JavaScript */

// Control functions
function setControl(variable, value) {
    fetch('/control?var=' + encodeURIComponent(variable) + '&val=' + encodeURIComponent(value))
        .then(r => r.text())
        .then(() => {})
        .catch(() => {
            document.getElementById('status').textContent = 'Status: Connection Error';
        });
}

function updateRangeValue(valueElement, value) {
    if (valueElement) {
        valueElement.textContent = valueElement.value;
    }
}

// System functions
function clearSettings() {
    if (confirm('Clear all saved settings from memory and reboot?')) {
        fetch('/reset').then(response => response.text()).then(data => alert(data));
    }
}

function rebootCamera() {
    if (confirm('Reboot the camera? This will disconnect all clients.')) {
        fetch('/reboot').then(response => response.text()).then(data => alert(data));
    }
}

document.querySelectorAll('[data-value]').forEach(el => {
    const dv = el.getAttribute('data-value');
    if (dv == null) return;
    if (el.type === 'checkbox') {
        el.checked = (dv === '1');
    } else if (el.tagName === 'SELECT') {
        el.value = dv;
    } else if (el.type === 'range' || el.tagName === 'INPUT') {
        // fallback for inputs that also carry data-value
        el.value = dv;
    }
});

document.querySelectorAll('input[type="range"]').forEach(input => {
    updateRangeValue(input);
    input.addEventListener('input', function() {
        updateRangeValue(this);
    });
});
