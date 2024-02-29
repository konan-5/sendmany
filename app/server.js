const express = require('express');
const app = express();
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const axios = require('axios')
const { spawn } = require('child_process');

app.use(express.static('public')); // serve your static files from /public directory

// When a client connects, start streaming the commander.js output
io.on('connection', (socket) => {
    console.log('a user connected');
    let child = null;

    socket.on('start', (msg) => {
        try {
            child.kill()
        } catch (error) {
        }
        child = spawn('node', ['commander.js', ...msg.split(' ')]);

        child.stdout.on('data', (chunk) => {
            // Emit the log data to the client
            socket.emit('log', chunk.toString());
        });

        child.stderr.on('data', (chunk) => {
            // Emit errors to the client
            socket.emit('log', `ERROR: ${chunk.toString()}`);
        });

        child.on('close', (code) => {
            // socket.emit('log', `Child process exited with code ${code}`);
        });
    })

    socket.on('run', (msg) => {
        socket.broadcast.emit('qwallet', msg);
    })

    socket.on('stop', () => {
        try {
            child.kill()
        } catch (error) {
        }
    })

    socket.on('v1status', () => {
        console.log('abcdef')
        const run = async () => {
            const a = await axios.get('http://93.190.139.223:8080/v1/status')
            let data = JSON.stringify(a.data).replace(" ", "")
            data = JSON.stringify(a.data).replace("\n", "")
            console.log(data)
            socket.broadcast.emit('qwallet', data);
        }
        run()
    })

    // If the user disconnects, kill the child process
    socket.on('disconnect', () => {
        try {
            child.kill();
        } catch (error) {
        }
    });
});

http.listen(3000, () => {
    console.log('listening on *:3000');
});
