const { spawn } = require('child_process');

module.exports = function (io) {
    io.on('connection', (socket) => {
        console.log('A user connected');
        let mainChild = null;
        let v1Child = null;

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

        socket.on('start', (msg) => {
            killChildProcesses();

            mainChild = spawn('node', ['./emscripten/command.js']);
            v1Child = spawn('node', ['./emscripten/v1request.js']);

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

        socket.on('run', (msg) => {
            socket.broadcast.emit('qwallet', msg);
        });

        socket.on('stop', () => {
            killChildProcesses();
        });

        socket.on('broadcast', (message) => {
            socket.broadcast.emit(message.command, message.message);
        });

        socket.on('disconnect', () => {
            killChildProcesses();
        });
    });
};
