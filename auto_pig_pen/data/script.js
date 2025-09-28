function feedNow() { 
    fetch('/feed', { method: 'POST' })
        .then(r => r.text())
        .then(text => {
            alert(text);
            getStatus();
            updateWeightDropStatus();
        })
        .catch(err => alert('Error: ' + err));
}

function washNow() { 
    fetch('/wash', { method: 'POST' })
        .then(r => r.text())
        .then(text => {
            alert(text);
            getStatus();
            updateWashTimerStatus();
        })
        .catch(err => alert('Error: ' + err));
}

function stopWash() {
    if (confirm('Stop wash operation?')) {
        fetch('/stopWash', { method: 'POST' })
            .then(r => r.text())
            .then(text => {
                alert(text);
                getStatus();
                updateWashTimerStatus();
            })
            .catch(err => alert('Error: ' + err));
    }
}

function setWashDuration() {
    let duration = document.getElementById('washDuration').value;
    if (duration && duration > 0 && duration <= 3600) {
        const formData = new FormData();
        formData.append('duration', duration);
        
        fetch('/setWashDuration', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            updateWashTimerStatus();
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    } else {
        alert('Please enter a valid duration (1-3600 seconds)');
    }
}

function updateWashTimerStatus() {
    fetch('/getWashDuration')
        .then(r => r.json())
        .then(data => {
            const statusDiv = document.getElementById('washTimerStatus');
            let html = '<h3>Wash Timer</h3>';
            html += `<p><strong>Duration:</strong> ${data.washDuration} seconds</p>`;
            html += `<p><strong>Status:</strong> <span style="color: ${data.isWashActive ? '#e74c3c' : '#27ae60'}">`;
            html += data.isWashActive ? 'ACTIVE - Wash in progress' : 'Inactive';
            html += '</span></p>';
            
            if (data.isWashActive) {
                html += `<p><strong>Elapsed:</strong> ${data.elapsedTime}s</p>`;
                html += `<p><strong>Remaining:</strong> ${data.remainingTime}s</p>`;
                html += '<button onclick="stopWash()" class="stop-btn">Stop Wash</button>';
            }
            
            statusDiv.innerHTML = html;
        })
        .catch(err => {
            console.error('Error updating wash timer status:', err);
        });
}

function tareScale() { 
    fetch('/tare', { method: 'POST' })
        .then(r => r.text())
        .then(text => {
            alert(text);
            getStatus();
            updateWeightDropStatus();
        })
        .catch(err => alert('Error: ' + err));
}

function setWeightDrop() {
    let weight = document.getElementById('weightDrop').value;
    if (weight && weight >= 0) {
        const formData = new FormData();
        formData.append('weight', weight);
        
        fetch('/setWeightDrop', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            updateWeightDropStatus();
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    } else {
        alert('Please enter a valid weight (grams)');
    }
}

function stopWeightMonitoring() {
    if (confirm('Stop weight drop monitoring?')) {
        fetch('/stopMonitoring', { method: 'POST' })
            .then(r => r.text())
            .then(text => {
                alert(text);
                updateWeightDropStatus();
                getStatus();
            })
            .catch(err => alert('Error: ' + err));
    }
}

function updateWeightDropStatus() {
    fetch('/getWeightDrop')
        .then(r => r.json())
        .then(data => {
            const statusDiv = document.getElementById('weightDropStatus');
            let html = '<h3>Weight Drop Monitoring</h3>';
            html += `<p><strong>Target Drop:</strong> ${data.targetWeightDrop}g</p>`;
            html += `<p><strong>Current Weight:</strong> ${data.currentWeight}g</p>`;
            html += `<p><strong>Status:</strong> <span style="color: ${data.isMonitoring ? '#e74c3c' : '#27ae60'}">`;
            html += data.isMonitoring ? 'ACTIVE - Monitoring weight drop' : 'Inactive';
            html += '</span></p>';
            
            if (data.isMonitoring) {
                html += '<button onclick="stopWeightMonitoring()" class="stop-btn">Stop Monitoring</button>';
            }
            
            statusDiv.innerHTML = html;
        })
        .catch(err => {
            console.error('Error updating weight drop status:', err);
        });
}

function addFeedTime() {
    let time = document.getElementById('feedTime').value;
    if (time) {
        const formData = new FormData();
        formData.append('time', time);
        
        fetch('/addFeed', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            loadFeedTimes();
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    } else {
        alert('Please select a time');
    }
}

function removeFeedTime() {
    let time = document.getElementById('feedTime').value;
    if (time) {
        const formData = new FormData();
        formData.append('time', time);
        
        fetch('/removeFeed', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            loadFeedTimes();
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    } else {
        alert('Please select a time');
    }
}

function addWashTime() {
    let time = document.getElementById('washTime').value;
    if (time) {
        const formData = new FormData();
        formData.append('time', time);
        
        fetch('/addWash', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            loadWashTimes();
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    } else {
        alert('Please select a time');
    }
}

function removeWashTime() {
    let time = document.getElementById('washTime').value;
    if (time) {
        const formData = new FormData();
        formData.append('time', time);
        
        fetch('/removeWash', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            loadWashTimes();
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    } else {
        alert('Please select a time');
    }
}

function setPhoneNumber() {
    let number = document.getElementById('phoneNumber').value;
    if (number) {
        const formData = new FormData();
        formData.append('number', number);
        
        fetch('/setPhone', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    } else {
        alert('Please enter a phone number');
    }
}

function sendTestSMS() {
    const formData = new FormData();
    formData.append('message', 'Test message from pet feeder');
    
    fetch('/sendSMS', { 
        method: 'POST',
        body: formData
    })
    .then(r => r.text())
    .then(text => {
        alert(text);
        getStatus();
    })
    .catch(err => alert('Error: ' + err));
}

function getStatus() {
    fetch('/status')
        .then(r => r.text())
        .then(t => {
            document.getElementById('status').innerHTML = t.replace(/\n/g, '<br>');
        })
        .catch(err => {
            document.getElementById('status').innerHTML = 'Error fetching status: ' + err;
        });
}

function loadFeedTimes() {
    fetch('/getFeedTimes')
        .then(r => {
            if (!r.ok) throw new Error('Network response was not ok');
            return r.json();
        })
        .then(times => {
            const container = document.getElementById('feedSchedules');
            if (times && times.length > 0) {
                container.innerHTML = '<h3>Current Feed Times:</h3><ul>' + 
                    times.map(time => `<li>${time} <button onclick="removeSpecificFeedTime('${time}')">Remove</button></li>`).join('') + '</ul>';
            } else {
                container.innerHTML = '<p>No feed times scheduled</p>';
            }
        })
        .catch(err => {
            console.error('Error loading feed times:', err);
            document.getElementById('feedSchedules').innerHTML = '<p>Error loading feed times</p>';
        });
}

function loadWashTimes() {
    fetch('/getWashTimes')
        .then(r => {
            if (!r.ok) throw new Error('Network response was not ok');
            return r.json();
        })
        .then(times => {
            const container = document.getElementById('washSchedules');
            if (times && times.length > 0) {
                container.innerHTML = '<h3>Current Wash Times:</h3><ul>' + 
                    times.map(time => `<li>${time} <button onclick="removeSpecificWashTime('${time}')">Remove</button></li>`).join('') + '</ul>';
            } else {
                container.innerHTML = '<p>No wash times scheduled</p>';
            }
        })
        .catch(err => {
            console.error('Error loading wash times:', err);
            document.getElementById('washSchedules').innerHTML = '<p>Error loading wash times</p>';
        });
}

function removeSpecificFeedTime(time) {
    if (confirm('Remove feed time ' + time + '?')) {
        const formData = new FormData();
        formData.append('time', time);
        
        fetch('/removeFeed', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            loadFeedTimes();
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    }
}

function removeSpecificWashTime(time) {
    if (confirm('Remove wash time ' + time + '?')) {
        const formData = new FormData();
        formData.append('time', time);
        
        fetch('/removeWash', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            loadWashTimes();
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
    }
}

// Enhanced initialization
document.addEventListener('DOMContentLoaded', function() {
    console.log('Initializing pet feeder control...');
    
    // Load initial data with retry mechanism
    setTimeout(() => {
        loadFeedTimes();
        loadWashTimes();
        getStatus();
        updateWeightDropStatus();
        updateWashTimerStatus();
    }, 1000);
    
    // Load phone number from status
    fetch('/status')
        .then(r => r.text())
        .then(status => {
            const phoneLine = status.split('\n').find(line => line.includes('Phone:'));
            if (phoneLine) {
                const phoneNumber = phoneLine.split(':')[1]?.trim();
                if (phoneNumber && phoneNumber !== 'null' && phoneNumber !== '') {
                    document.getElementById('phoneNumber').value = phoneNumber;
                }
            }
        })
        .catch(err => console.error('Error loading phone number:', err));
    
    // Set up periodic updates
    setInterval(getStatus, 30000);
    setInterval(loadFeedTimes, 15000);
    setInterval(loadWashTimes, 15000);
    setInterval(updateWeightDropStatus, 10000);
    setInterval(updateWashTimerStatus, 2000);
    
    console.log('Pet feeder control initialized');
});