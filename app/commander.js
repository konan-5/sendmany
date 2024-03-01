const createModule = require('./public/a.out.js');
const io = require('socket.io-client');

// Remove the first two elements (node, filename)
const args = process.argv.splice(2);

// Initialize the WebAssembly module
const Module = createModule();

// Connect to the WebSocket server
const socket = io('http://localhost:3000');

// Function to call the 'qwallet' function from the WebAssembly module
const callQwallet = async (command) => {
    const result = await Module.ccall('qwallet', 'string', ['string'], [command]);
    return result;
};

// Event listener for 'qwallet'
socket.on('qwallet', async (command) => {
    const result = await callQwallet(command);
    console.log(`_^_${result}_^_`); // Log the result
});

// Event listener for 'qwalletwithv1'
socket.on('qwalletwithv1', async (command) => {
    await callQwallet(command); // Process the command but no need to log or emit
});

// Event listener for 'v1request'
socket.on('v1request', async () => {
    try {
        const result = await callQwallet("v1request");
        const parsedResult = JSON.parse(result);
        if (parsedResult.result === 0) {
            // Emit 'v1response' event with the result if the 'result' property is 0
            socket.emit('broadcast', { command: 'v1response', message: result });
        }
        // No action is taken if the result is not 0
    } catch (error) {
        // console.error('An error occurred:', error);
        // Error handling logic can be expanded here
    }
});
