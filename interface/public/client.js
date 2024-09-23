const ws = new WebSocket('ws://localhost:3000');

ws.onopen = () => {
    console.log("WebSocket connection established.");
};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    const topic = data.topic;
    const message = JSON.parse(data.message);

    switch (topic) {
        case 'Camera_status':
            updateCameraStatus(message.cameraStatus);
            break;
        case 'AC_status':
            updateACStatus(message.AC_status);
            break;
        case 'DC_status':
            updateDCStatus(message.DC_status);
            break;
    }
};

// Update Camera Status
function updateCameraStatus(cameras) {
    const cameraStatusContainer = document.getElementById('camera-status');
    cameraStatusContainer.innerHTML = ''; // Clear previous entries

    cameras.forEach((cam) => {
        const cameraBox = document.createElement('div');
        cameraBox.className = `icon-box ${cam.isAlive ? 'green' : 'red'}`;

        cameraBox.innerHTML = `
            <i class="fas fa-video"></i>
            <p>${cam.ip}</p>
        `;
        cameraStatusContainer.appendChild(cameraBox);
    });
}

// Update Traffic Light Status
function updateACStatus(acStatus) {
    const acStatusContainer = document.getElementById('ac-status');
    acStatusContainer.innerHTML = ''; // Clear previous entries

    acStatus.forEach((ac) => {
        const trafficLightBox = document.createElement('div');
        trafficLightBox.className = 'icon-box';

        // Create traffic light container with red and green circles
        trafficLightBox.innerHTML = `
            <div class="traffic-light-container">
                <div class="traffic-light-circle ${ac.red_state ? 'red' : 'gray'}"></div>
                <div class="traffic-light-circle ${ac.green_state ? 'green' : 'gray'}"></div>
            </div>
            <p>Phase ${ac.phase}</p>
        `;

        acStatusContainer.appendChild(trafficLightBox);
    });
}

// Update Pedestrian Button Status
function updateDCStatus(dcStatus) {
    const dcStatusContainer = document.getElementById('dc-status');
    dcStatusContainer.innerHTML = ''; // Clear previous entries

    dcStatus.forEach((button) => {
        const buttonBox = document.createElement('div');
        buttonBox.className = `icon-box ${button.isPressed ? 'green' : 'red'}`;

        buttonBox.innerHTML = `
            <i class="fas fa-hand-paper hand-icon"></i>
            <p>Push Button ${button.push_button}</p>
        `;
        dcStatusContainer.appendChild(buttonBox);
    });
}
