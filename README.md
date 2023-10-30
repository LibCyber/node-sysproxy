# Node Sysproxy

A tool to setup system proxy in a more native way.

## Usage

### Install dependency
```bash
npm install --save sysproxy
```

### Use in Javascript codes
```javascript
const sysproxy = require('sysproxy');

console.log("Current proxy info:", sysproxy.queryProxy());
console.log("Set proxy result:", sysproxy.setProxy({
    flags: 3,
    autoConfigUrl: '',
    proxyServer: '127.0.0.1:8890',
    bypassList: 'localhost;127.*;192.168.*;10.*'
}))
console.log("Reset proxy result:", sysproxy.setProxy({
    flags: 3,
    autoConfigUrl: '',
    proxyServer: '',
    bypassList: ''
}))
```
