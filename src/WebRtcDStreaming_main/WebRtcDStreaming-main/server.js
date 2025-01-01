const express = require('express');
const https = require('https');
const fs = require('fs');
const path = require('path');
const socketIO = require('socket.io');

const app = express();
const server = https.createServer({
  key: fs.readFileSync('server.key'),
  cert: fs.readFileSync('server.cert')
}, app);
const io = socketIO(server);

app.use(express.static('public'));

io.on('connection', (socket) => {
  console.log('A user connected');

  socket.on('offer', (offer) => {
    socket.broadcast.emit('offer', offer);
  });

  socket.on('answer', (answer) => {
    socket.broadcast.emit('answer', answer);
  });

  socket.on('candidate', (candidate) => {
    if (!candidate.candidate.includes('IP6')) {
      socket.broadcast.emit('candidate', candidate);
    }
  });


  socket.on('disconnect', () => {
    console.log('A user disconnected');
  });
});

const PORT = process.env.PORT || 3000;
const IP = '192.168.1.151'; // 使用指定的 IP 地址或 localhost
server.listen(PORT, IP, () => {
  console.log(`Server is running on https://${IP}:${PORT}`);
});
