let sysproxy;

if (process.platform === 'win32') {
  sysproxy = require('./build/Release/sysproxy_windows.node');
} else if (process.platform === 'darwin') {
  // sysproxy = require('./build/Release/sysproxy_macos.node');
  throw new Error('Not supported yet');
} else if (process.platform === 'linux') {
  // sysproxy = require('./build/Release/sysproxy_linux.node');
  throw new Error('Not supported yet');
} else {
  throw new Error('Not supported yet');
}

function setProxy({
  autoProxyDiscovery, // boolean
  autoProxyConfig, // boolean
  autoProxyConfigUrl, // string
  manualProxy, // boolean
  proxyServer, // string
  bypassList // string, comma separated, IP or CIDR format
}) {
  if (process.platform === 'win32') {
    let windowsFlag = 1;
    if (autoProxyDiscovery) {
      windowsFlag += 8;
    }
    if (autoProxyConfig) {
      windowsFlag += 4;
    }
    if (manualProxy) {
      windowsFlag += 2;
    }

    // convert bypassList from CIDR to 1.1.*.* format, and separate with ';'
    if (bypassList) {
      bypassList = convertBypassList(bypassList);
    }
    return sysproxy.setWindowsProxy({
      flags: windowsFlag,
      autoConfigUrl: autoProxyConfigUrl,
      proxyServer: proxyServer,
      bypassList: bypassList
    });
  } else if (process.platform === 'darwin') {
    throw new Error('Not supported yet');
  } else if (process.platform === 'linux') {
    throw new Error('Not supported yet');
  } else {
    throw new Error('Not supported yet');
  }
}

function convertBypassList(ipRangeString) {
  // 分割字符串为数组
  const elements = ipRangeString.split(',');

  // 转换每个元素
  const convertedElements = elements.map(element => {
    // 检查是否为IP段（是否包含'/'）
    if (element.includes('/')) {
      const [ip, range] = element.split('/');
      const ipParts = ip.split('.');

      // 根据子网掩码范围替换IP部分为通配符'*'
      const mask = parseInt(range, 10);
      if (mask < 8) { // 0-7
        return '*';
      } else if (mask < 16) { // 8-15
        return `${ipParts[0]}.*`;
      } else if (mask < 24) { // 16-23
        return `${ipParts[0]}.${ipParts[1]}.*`;
      } else if (mask < 32) { // 24-31
        return `${ipParts[0]}.${ipParts[1]}.${ipParts[2]}.*`;
      } else { // 32
        return element;
      }
    } else {
      // 如果不是IP段，直接返回原值
      return element;
    }
  });

  // 将转换后的数组元素用分号连接起来并返回
  return convertedElements.join(';');
}

function revertBypassList(wildcardString) {
  // 分割字符串为数组
  const elements = wildcardString.split(';');

  // 反转换每个元素
  const revertedElements = elements.map(element => {
    // 检查是否为通配符形式
    if (element.endsWith('.*')) {
      // 分割 IP 部分和通配符
      const ipParts = element.split('.');
      // 移除 '*' 部分
      ipParts.pop();
      
      // 根据剩余部分的长度确定子网掩码
      if (ipParts.length === 1) {
        return `${ipParts.join('.')}.0.0.0/8`;
      } else if (ipParts.length === 2) {
        return `${ipParts.join('.')}.0.0/16`;
      } else if (ipParts.length === 3) {
        return `${ipParts.join('.')}.0/24`;
      } else {
        throw new Error('Invalid wildcard string');
      }
    } else {
      // 如果没有通配符，直接返回原值
      return element;
    }
  });

  // 将反转换后的数组元素用逗号连接起来并返回
  return revertedElements.join(',');
}

// 返回格式：
// {
//   autoProxyDiscovery, // boolean
//   autoProxyConfig, // boolean
//   autoProxyConfigUrl, // string
//   manualProxy, // boolean
//   proxyServer, // string
//   bypassList // string, comma separated, IP or CIDR format
// }
// 如果有http\https\socks等多种代理，则只取http代理
function queryProxy() {
  if (process.platform === 'win32') {
    let finalResult = {
      autoProxyDiscovery: false,
      autoProxyConfig: false,
      autoProxyConfigUrl: '',
      manualProxy: false,
      proxyServer: '',
      bypassList: ''
    }
    const result = sysproxy.queryWindowsProxy();
    const flags = result.flags;
    const proxyServer = result.proxyServer;
    const bypassList = result.bypassList;

    finalResult.autoProxyConfigUrl = result.autoConfigUrl;
    finalResult.proxyServer = proxyServer;
    finalResult.bypassList = bypassList;
    
    if (flags & 2) {
      finalResult.manualProxy = true;
    } else if (flags & 4) {
      finalResult.autoProxyConfig = true;
    } else if (flags & 8) {
      finalResult.autoProxyDiscovery = true;
    }

    // 将bypassList转换为CIDR格式
    finalResult.bypassList = revertBypassList(finalResult.bypassList);

    return finalResult;
  } else if (process.platform === 'darwin') {
    throw new Error('Not supported yet');
  } else if (process.platform === 'linux') {
    throw new Error('Not supported yet');
  } else {
    throw new Error('Not supported yet');
  }
}

module.exports = {
  setProxy: setProxy,
  queryProxy: queryProxy,
};
