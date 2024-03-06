const WebSocket = require('ws');
const crypto = require('crypto');

function connectAndRequestData(url) {
    // Initialize a WebSocket connection
    const ws = new WebSocket(url);

    // Event handler for successfully opening a connection
    ws.on('open', () => {
        console.log("Connected to the server");

        function sendRequest() {
            ws.send("ZWUXZSKJDTKHGHZHIRXSPIAMFUMAWSEMTVQMSHUXJHUAYKPWJAMCNCNDOLIM");
        }

        sendRequest();
    });

    ws.on('error', (error) => {
        console.error(`WebSocket error: ${error}`);
    });

    ws.on('message', (data) => {
        console.log("Response received:", data);
        // // Uncomment the following lines to process and log the response as a hexadecimal string
        const response = new Uint8Array(data);
        const hexString = [...response].map(b => b.toString(16).padStart(2, '0')).join('');
        console.log("Response received in hex:", hexString);
    });

    ws.on('close', () => {
        console.log("Disconnected from the server");
    });
}

connectAndRequestData("ws://93.190.139.223:4444");
