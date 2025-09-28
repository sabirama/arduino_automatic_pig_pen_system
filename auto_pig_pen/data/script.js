function feedNow() { 
    fetch('/feed', { method: 'POST' })
        .then(r => r.text())
        .then(text => {
            alert(text);
            getStatus(); // Update status after operation
        })
        .catch(err => alert('Error: ' + err));
}

function washNow() { 
    fetch('/wash', { method: 'POST' })
        .then(r => r.text())
        .then(text => {
            alert(text);
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
}

function tareScale() { 
    fetch('/tare', { method: 'POST' })
        .then(r => r.text())
        .then(text => {
            alert(text);
            getStatus();
        })
        .catch(err => alert('Error: ' + err));
}

function addFeedTime() {
    let time = document.getElementById('feedTime').value;
    if (time) {
        // Send as form data instead of query parameter
        const formData = new FormData();
        formData.append('time', time);
        
        fetch('/addFeed', { 
            method: 'POST',
            body: formData
        })
        .then(r => r.text())
        .then(text => {
            alert(text);
            loadFeedTimes(); // Reload schedules
            getStatus(); // Update status
        })
        .catch(err => alert('Error: ' + err));
    } else {
        alert('Please select a time');
    }
}

function removeFeedTime() {
    let time = document.getElementById('feedTime').value;
    if (time) {
        // Send as form data instead of query parameter
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
        // Send as form data instead of query parameter
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
        // Send as form data instead of query parameter
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
        // Send as form data instead of query parameter
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
    // Send as form data with optional message parameter
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
        // Send as form data instead of query parameter
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
        // Send as form data instead of query parameter
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
    }, 1000);
    
    // Load phone number from status
    fetch('/status')
        .then(r => r.text())
        .then(status => {
            // Updated to match the new status format
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
    
    console.log('Pet feeder control initialized');
});
