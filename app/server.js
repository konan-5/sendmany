const express = require('express');
const bodyParser = require('body-parser');
const app = express();
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const { spawn } = require('child_process');

// Set the view engine to ejs
app.set('view engine', 'ejs');
// Serve static files from the 'public' directory
app.use(express.static('public'));
// Use body-parser to parse form data
app.use(bodyParser.urlencoded({ extended: true }));

let seedInfo = null;
let confirmSeeds = new Array(24).fill("");
let confirmStatus = false;

function reset() {
    seedInfo = null;
    confirmSeeds = new Array(24).fill("");
    confirmStatus = false;
}

app.get('/login', (req, res) => {
    if (confirmStatus) {
        res.redirect('dashboard')
    } else {
        if (seedInfo && seedInfo.result.result == 0 && seedInfo.result.seedpage == 0) {
            res.redirect('dashboard')
        } else if (seedInfo && seedInfo.result.result == 0 && seedInfo.result.seedpage == 1) {
            res.redirect('create')
        } else {
            reset()
            res.render('login')
        }
    }
})

app.get('/', (req, res) => {
    res.render('index')
})

app.get('/dashboard', (req, res) => {
    if (confirmStatus) {
        res.render('dashboard', seedInfo)
    } else {
        if (seedInfo && seedInfo.result.result == 0 && seedInfo.result.seedpage == 0) {
            res.render('dashboard', seedInfo)
        } else if (seedInfo && seedInfo.result.result == 0 && seedInfo.result.seedpage == 1) {
            res.redirect('create')
        } else {
            res.redirect('login')
        }
    }
})

app.get('/check', (req, res) => {
    if (confirmStatus) {
        res.redirect('dashboard')
    } else {
        if (seedInfo && seedInfo.result.result == 0 && seedInfo.result.seedpage == 0) {
            res.redirect('dashboard')
        } else if (seedInfo && seedInfo.result.result == 0 && seedInfo.result.seedpage == 1) {
            res.render('check', { confirmSeeds, password: seedInfo.password })
        } else {
            res.redirect('login')
        }
    }
})

app.get('/create', (req, res) => {
    if (confirmStatus) {
        res.redirect('dashboard')
    } else {
        if (seedInfo && seedInfo.result.result == 0 && seedInfo.result.seedpage == 0) {
            res.redirect('dashboard')
        } else if (seedInfo && seedInfo.result.result == 0 && seedInfo.result.seedpage == 1) {
            res.render('create', seedInfo)
        } else {
            res.redirect('login')
        }
    }
})

app.post('/create', (req, res) => {
    seedInfo = { ...req.body, result: JSON.parse(req.body.result) }
    console.log(seedInfo)
    res.redirect('create')
})

app.post('/check', (req, res) => {
    res.redirect('check')
})

app.post('/confirm', (req, res) => {
    const display = seedInfo.result.display.split(' ')
    let compare = true;

    if(seedInfo.password.startsWith('Q')) {
        confirmSeeds[0] = req.body['seed0']
        if(req.body['seed0'] != display[0]) {
            compare = false
        }
    } else {
        display.map((word, index) => {
            confirmSeeds[index] = req.body[`seed${index}`];
            if (req.body[`seed${index}`] != word) compare = false;
        })
    }

    confirmStatus = compare;
    if (compare) {
        confirmSeeds = new Array(24).fill("");
        res.redirect('dashboard')
    } else {
        res.redirect('check?status=notmatch')
    }
})

app.post('/dashboard', (req, res) => {
    seedInfo = { ...req.body, result: JSON.parse(req.body.result) }
    res.redirect('dashboard')
})

app.post('/logout', (req, res) => {
    reset()
    res.redirect('login')
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

        mainChild = spawn('node', ['./emscripten/command.js']);
        v1Child = spawn('node', ['./emscripten/v1request.js']);

        // Handle output from mainChild
        mainChild.stdout.on('data', (data) => {
            socket.emit('log', { value: data.toString(), flag: 'log' });
        });

        mainChild.stderr.on('data', (data) => {
            socket.emit('log', { value: `ERROR: ${data.toString()}`, flag: 'log' });
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
