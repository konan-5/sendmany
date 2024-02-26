const importModule = require('./a.out.js');
const express = require('express')
const app = express()
const path = require('path');
const port = 3000

app.use(express.static('public'));
app.use(express.json())

app.use(express.urlencoded({ extended: true }));

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public/index.html'));
});

app.post('/command', (req, res) => {
    const cmd = req.body.command;
    const cmdArray = cmd.split(" ")
    const functionName = cmdArray[0]
    console.log(functionName)
    let statusCode = 200
    function command () {
        try {
            const result = importModule.ccall(functionName, 'number', ['string', 'string'], ["wecome to qubic", 'b']);
            return `${result}`
        } catch (error) {
            statusCode = 500;
            return `${error}`;
        }
    };
    const result = command()
    console.log(result)
    res.status(statusCode).send(result)
})

app.post('/test', (req, res) => {
    res.status(200).send('hello')
})

app.listen(port, () => {
    console.log(`Example app listening on port ${port}`)
})
