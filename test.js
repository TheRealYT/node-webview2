const fs = require('node:fs');
const webview = require('./build/Debug/webview2');

require('node:util').inherits(webview.BrowserWindow, require('node:events').EventEmitter);

const buf = fs.readFileSync('C:\\Users\\yt040\\Downloads\\resource_hacker\\help\\rh_icon.png');
const browserWindow = new webview.BrowserWindow('Hello');

browserWindow.addListener('2', () => console.log('Exit Window'));

browserWindow.setRequestHandler((req, res) => {
    console.log(req, res);
    res.send(buf, 'image/png', 201, 'OK');
});

browserWindow.addListener('web', () => {
    console.log(browserWindow.addRequestFilter('*', 0));
});

webview.MessageLoop();

console.log('Done');