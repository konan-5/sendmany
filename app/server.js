const express = require('express');
const bodyParser = require('body-parser');
const app = express();
const http = require('http').createServer(app);
const io = require('socket.io')(http);
const userRoutes = require('./routes/userRoutes');
const mainRoutes = require('./routes/mainRoutes');

const socketController = require('./controllers/socketController')(io);

// Set the view engine to ejs
app.set('view engine', 'ejs');

// Serve static files from the 'public' directory
app.use(express.static('public'));

// Use body-parser to parse form data
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

// Setup routes
app.use('/api', userRoutes);
app.use('/', mainRoutes);

// Start the server
http.listen(3000, () => {
    console.log('Server listening on port 3000');
});
