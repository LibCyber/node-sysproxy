const { queryProxy, setProxy } = require('./index.js');  // 确保路径正确

async function test() {
  console.log("Start testing...")
  try {
    const result = queryProxy();
    console.log('Query proxy successfully:', result);

    const result2 = setProxy({
      autoProxyDiscovery: true,
      autoProxyConfig: false,
      autoProxyConfigUrl: '',
      manualProxy: true,
      proxyServer: '127.0.0.1:8890',
      bypassList: 'localhost,127.0.0.0/8,10.0.0.0/8,172.16.0.0/16,172.17.0.0/16,172.18.0.0/16,172.19.0.0/16,172.20.0.0/16,192.168.0.0/16'
    })

    if (result2) {
      console.log(`Set proxy successfully, result: ${result2}`);
    }

    const result3 = queryProxy();
    console.log('Query proxy again:', result3);

    const result4 = setProxy({
      autoProxyDiscovery: false,
      autoProxyConfig: false,
      autoProxyConfigUrl: '',
      manualProxy: false,
      proxyServer: '',
      bypassList: ''
    })

    if (result4) {
      console.log(`Reset proxy successfully, result: ${result4}`);
    }

    const result5 = queryProxy();
    console.log('Query proxy finally:', result5);


  } catch (error) {
    console.error('Error setting proxy:', error);
  }

  console.log("End testing...")
}

test();
