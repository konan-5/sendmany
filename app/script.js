document.getElementById('send-command').addEventListener('click', function() {
    const commandInput = document.getElementById('command-input');
    const command = commandInput.value.trim();
    commandInput.value = ''; // Clear input after getting the command

    if (!command) {
        displayError('Please enter a command.');
        return;
    }

    // Assuming the backend API endpoint for qwallet is /api/qwallet
    // This is a mock function to simulate sending command and receiving a response
    mockSendCommand(command).then(response => {
        if (response.status < 0) {
            displayError(`Error ${response.status}: ${response.details}`);
        } else if (response.status === 0) {
            displayConsole(`Operation completed successfully.`);
        } else {
            // If status is > 0, poll for completion
            pollStatus(response.tasknum);
        }
    });
});

function mockSendCommand(command) {
    // Mock sending command to qwallet and receiving a response
    // Replace this with actual AJAX/Fetch API call to backend
    return new Promise(resolve => {
        setTimeout(() => {
            const mockResponse = command === 'status' ? { status: 0 } : { status: 1, tasknum: 123 };
            resolve(mockResponse);
        }, 1000);
    });
}

function pollStatus(tasknum) {
    const interval = setInterval(() => {
        mockSendCommand('status').then(response => {
            if (response.status <= 0) {
                clearInterval(interval);
                if (response.status < 0) {
                    displayError(`Error ${response.status}: Task failed.`);
                } else {
                    displayConsole(`Task ${tasknum} completed successfully.`);
                }
            }
        });
    }, 10);
}

function displayConsole(message) {
    const consoleOutput = document.getElementById('console-output');
    consoleOutput.textContent += message + '\n';
}

function displayError(message) {
    const errorOutput = document.getElementById('error-output');
    errorOutput.textContent += message + '\n';
}
