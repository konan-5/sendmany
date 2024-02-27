const express = require('express');
const app = express();
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const { spawn } = require('child_process');

app.use(express.static('public')); // serve your static files from /public directory

// When a client connects, start streaming the commander.js output
io.on('connection', (socket) => {
    console.log('a user connected');
    let child = null;

    socket.on('run', (msg) => {
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

    socket.on('stop', () => {
        try {
            child.kill()
        } catch (error) {
        }
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
