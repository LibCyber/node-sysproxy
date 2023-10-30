const { queryProxy, setProxy } = require('./index.js');  // 确保路径正确

async function test() {
  try {
    const result = queryProxy();
    console.log('Query proxy successfully:', result);

    const result2 = setProxy({
      flags: 3,
      autoConfigUrl: '',
      proxyServer: '127.0.0.1:8890',
      bypassList: 'localhost;127.*;192.168.*;10.*'
    })

    if (result2) {
      console.log('Set proxy successfully');
    }

    const result3 = queryProxy();
    console.log('Query proxy again:', result3);

    const result4 = setProxy({
      flags: 0,
      autoConfigUrl: '',
      proxyServer: '',
      bypassList: ''
    })

    if (result4) {
      console.log('Reset proxy successfully');
    }

    const result5 = queryProxy();
    console.log('Query proxy finally:', result5);

    
  } catch (error) {
    console.error('Error setting proxy:', error);
  }
}

test();
