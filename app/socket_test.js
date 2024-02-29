const WebSocket = require('ws');
const crypto = require('crypto');

function connectAndRequestData(url) {
    const ws = new WebSocket(url);

    ws.on('open', function open(data) {
        console.log("Connected to the server");
        function sendRequest() {
            const randBytes = crypto.randomBytes(4);
            console.log(new Uint8Array([8, 0, 0, 27, ...randBytes]))
            const request = new Uint8Array([8, 0, 0, 27, ...randBytes]);
            ws.send(request);
            console.log("Request sent");
        }
        sendRequest();
        setInterval(sendRequest, 3000);
    });
    ws.on('error', function(error) {
        console.log(`WebSocket error: ${error}`);
    });

    ws.on('message', function(data) {
        console.log(data, 'aaaaaaa')
        // const response = new Uint8Array(data);
        // const hexString = [...response].map(b => b.toString(16).padStart(2, '0')).join('');
        // console.log("Response received in hex:", hexString);
    });

    ws.on('close', function() {
        console.log("Disconnected from the server");
    });
}
connectAndRequestData("wss://lrv.quorum.gr:22841");
