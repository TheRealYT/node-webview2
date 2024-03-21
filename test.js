const a = require('./build/Release/webview2');
require('node:util').inherits(a.BrowserWindow, require('node:events').EventEmitter);

const x = new a.BrowserWindow('Hello');
x.addListener('2', () => console.log('exit x'));
x.addListener('web', () => console.log('web x'));

const y = new a.BrowserWindow('Hello');
y.addListener('2', () => console.log('exit y'));
y.addListener('web', () => console.log('web y'));

a.MessageLoop();

console.log('Done');