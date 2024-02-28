const createModule = require('./public/a.out.js');
const io = require('socket.io-client');
const fs = require('fs').promises;

const args = process.argv.splice(2)

const Module = createModule()

// Connect to your WebSocket server
const socket = io('http://localhost:3000');

// Listen for the qwallet event
socket.on('qwallet', (message) => {
    const result = Module.ccall("qwallet", 'string', ['string'], [message])
    console.log(`_^_${result}_^_`)
});
