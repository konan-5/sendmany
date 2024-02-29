const createModule = require('./public/a.out.js');
const io = require('socket.io-client');
const fs = require('fs').promises;

const args = process.argv.splice(2)

const Module = createModule()

// Connect to your WebSocket server
const socket = io('http://localhost:3000');

const callQwallet = async (command) => {
    const result = await Module.ccall("qwallet", 'string', ['string'], [command])
    return result
}

// Listen for the qwallet event
socket.on('qwallet', async (command) => {
    const result = await callQwallet(command)
    console.log(`_^_${result}_^_`)
});

socket.on('qwalletwithv1', async (command) => {
    const result = await callQwallet(command)
});

socket.on('v1request', async (command) => {
    try {
        const result = await callQwallet("v1request")
        if(JSON.parse(result).result == 0){
            socket.emit('broadcast', {command: 'v1response', message: result})
        } else {
        }
    } catch (error) {
        
    }
});
