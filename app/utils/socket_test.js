const WebSocket = require('ws');
const crypto = require('crypto');

function connectAndRequestData(url) {
    // Initialize a WebSocket connection
    const ws = new WebSocket(url);

    // Event handler for successfully opening a connection
    ws.on('open', () => {
        console.log("Connected to the server");

        // Function to send a request to the server
        function sendRequest() {
            // Generate 4 random bytes
            const randBytes = crypto.randomBytes(4);
            const request = new Uint8Array([8, 0, 0, 27, ...randBytes]);
            ws.send(request); // Send the request
            console.log("Request sent:", request);
        }

        sendRequest();
        setInterval(sendRequest, 3000);
    });

    ws.on('error', (error) => {
        console.error(`WebSocket error: ${error}`);
    });

    ws.on('message', (data) => {
        console.log("Response received:", data);
        // Uncomment the following lines to process and log the response as a hexadecimal string
        // const response = new Uint8Array(data);
        // const hexString = [...response].map(b => b.toString(16).padStart(2, '0')).join('');
        // console.log("Response received in hex:", hexString);
    });

    ws.on('close', () => {
        console.log("Disconnected from the server");
    });
}

connectAndRequestData("wss://lrv.quorum.gr:22841");
