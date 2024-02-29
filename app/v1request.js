const io = require('socket.io-client');
const axios = require('axios')

const socket = io('http://localhost:3000');
setInterval(() => {
    socket.emit('broadcast', {command: 'v1request', message: 'testmessage'})
}, 1000);

// Listen for the qwallet event
socket.on('v1response', async (message) => {
    try {
        const endpoint = JSON.parse(message).display;
        const response = await axios.get(`http://93.190.139.223:8080/v1/${endpoint}`);
        const command = endpoint.replace("/", " ") + " " + JSON.stringify(response.data).replace(' ', '').replace('\n', '')
        socket.emit('broadcast', {command: 'qwalletwithv1', message: command})
        } catch (error) {
    }
});
