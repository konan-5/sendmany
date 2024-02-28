const { spawn } = require('child_process');

function runCommander() {
  const child = spawn('node', ['commander.js']);

  // Listen to standard output
  child.stdout.on('data', (chunk) => {
    // Here you can process the log data
    console.log(`STDOUT: ${chunk.toString()}`);
  });

  // Listen to standard error
  child.stderr.on('data', (chunk) => {
    console.error(`STDERR: ${chunk.toString()}`);
  });

  // Handle the close event
  child.on('close', (code) => {
    console.log(`Child process exited with code ${code}`);
  });
}

runCommander();
