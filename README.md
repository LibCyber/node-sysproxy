# Node Sysproxy


[![Build](https://github.com/LibCyber/node-sysproxy/actions/workflows/build.yaml/badge.svg)](https://github.com/LibCyber/node-sysproxy/actions/workflows/build.yaml)

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
  autoProxyDiscovery: false,
  autoProxyConfig: false,
  autoProxyConfigUrl: '',
  manualProxy: true,
  proxyServer: '127.0.0.1:8890',
  bypassList: 'localhost,127.0.0.0/8;10.0.0.0/8;172.16.0.0/16;172.17.0.0/16;172.18.0.0/16;172.19.0.0/16;172.20.0.0/16;192.168.0.0/16'
}))
console.log("Current proxy info:", sysproxy.queryProxy());
console.log("Reset proxy result:", sysproxy.setProxy({
  autoProxyDiscovery: false,
  autoProxyConfig: false,
  autoProxyConfigUrl: '',
  manualProxy: false,
  proxyServer: '',
  bypassList: ''
}))
```
