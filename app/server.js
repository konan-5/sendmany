const express = require('express');
const app = express();
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const { spawn } = require('child_process');

// Set the view engine to ejs
app.set('view engine', 'ejs');

// Serve static files from the 'public' directory
app.use(express.static('public'));

app.get('/login', (req, res) => {
    res.render('login')
})

app.get('/create', (req, res) => {
    res.render('create')
})

app.get('/', (req, res) => {
    res.render('index')
})

// Establish socket connection
io.on('connection', (socket) => {
    console.log('A user connected');
    let mainChild = null;
    let v1Child = null;

    // Function to kill child processes safely
    const killChildProcesses = () => {
        if (mainChild) {
            mainChild.kill();
            mainChild = null;
        }
        if (v1Child) {
            v1Child.kill();
            v1Child = null;
        }
    };

    // Handler for 'start' event
    socket.on('start', (msg) => {
        killChildProcesses();

        mainChild = spawn('node', ['./emscripten/command.js', ...msg.split(' ')]);
        v1Child = spawn('node', ['./emscripten/v1request.js']);

        // Handle output from mainChild
        mainChild.stdout.on('data', (data) => {
            socket.emit('log', data.toString());
        });

        mainChild.stderr.on('data', (data) => {
            socket.emit('log', `ERROR: ${data.toString()}`);
        });

        mainChild.on('close', (code) => {
            // socket.emit('log', `Child process exited with code ${code}`);
        });

    });

    // Handler for 'run' event
    socket.on('run', (msg) => {
        socket.broadcast.emit('qwallet', msg);
    });

    // Handler for 'stop' event
    socket.on('stop', () => {
        killChildProcesses();
    });

    // Handler for 'broadcast' event
    socket.on('broadcast', (message) => {
        socket.broadcast.emit(message.command, message.message);
    });

    // Handle disconnection
    socket.on('disconnect', () => {
        killChildProcesses();
    });
});

// Start the server
http.listen(3000, () => {
    console.log('Server listening on port 3000');
});
