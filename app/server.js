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
    let mainChild = null;
    let v1Child = null;

    socket.on('start', (msg) => {
        try {
            mainChild.kill()
            v1Child.kill()
        } catch (error) {
        }
        mainChild = spawn('node', ['commander.js', ...msg.split(' ')]);
        v1Child = spawn('node', ['v1request.js']);

        mainChild.stdout.on('data', (chunk) => {
            // Emit the log data to the client
            socket.emit('log', chunk.toString());
        });

        mainChild.stderr.on('data', (chunk) => {
            // Emit errors to the client
            socket.emit('log', `ERROR: ${chunk.toString()}`);
        });

        mainChild.on('close', (code) => {
            // socket.emit('log', `Child process exited with code ${code}`);
        });

        // v1Child.stdout.on('data', (chunk) => {
        //     // Emit the log data to the client
        //     console.log('log', chunk.toString());
        // });

        // v1Child.stderr.on('data', (chunk) => {
        //     // Emit errors to the client
        //     console.log('log', `ERROR: ${chunk.toString()}`);
        // });

        // v1Child.on('close', (code) => {
        //     // socket.emit('log', `Child process exited with code ${code}`);
        // });
    })

    socket.on('run', (msg) => {
        socket.broadcast.emit('qwallet', msg);
    })

    socket.on('stop', () => {
        try {
            mainChild.kill()
        } catch (error) {
        }
    })

    // socket.on('v1status', () => {
    //     const run = async () => {
    //         const a = await axios.get('http://93.190.139.223:8080/v1/status')
    //         let data = JSON.stringify(a.data).replace(" ", "")
    //         data = JSON.stringify(a.data).replace("\n", "")
    //         console.log(data)
    //         socket.broadcast.emit('qwallet', data);
    //     }
    //     run()
    // })

    socket.on('broadcast', (message) => {
        console.log(message)
        socket.broadcast.emit(message.command, message.message)
    })

    // If the user disconnects, kill the child process
    socket.on('disconnect', () => {
        try {
            mainChild.kill();
            v1Child.kill();
        } catch (error) {
        }
    });
});

http.listen(3000, () => {
    console.log('listening on *:3000');
});
