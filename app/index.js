const express = require('express')
const app = express()
const path = require('path');
const port = 3000

app.use(express.static('public'));
app.use(express.json())

app.use(express.urlencoded({ extended: true }));

app.get('/', (req, res) => {
    res.send("QUBIC")
});

app.get('/test', (req, res) => {
    console.log('sssssssssssssss')
    res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
    res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    console.log('bbbbbbbbbbbbbb')
    res.sendFile(path.join(__dirname, 'public/index.html'));
})

app.listen(port, () => {
    console.log(`Example app listening on port ${port}`)
})
