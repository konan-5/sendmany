const io = require('socket.io-client');
const fs = require('fs').promises;

async function writeFileAsync(filePath, content) {
    try {
        await fs.writeFile(filePath, content, 'utf8');
        console.log('File written successfully');
    } catch (error) {
        console.error('Error writing file:', error);
    }
}

// Connect to your WebSocket server
const socket = io('http://localhost:3000');

// Listen for the qwallet event
socket.on('qwallet', (message) => {
    writeFileAsync('example.txt', 'Hello, world!');

    console.log('Received qwallet event:', message);
});

// Optionally, you can emit events to the server as needed
socket.emit('test', 'your-command-here');

socket.on('connect', () => {
    console.log("connect")
})

socket.on('disconnect', () => {
    console.log("disconnected")
})
