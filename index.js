const sysproxy = require('./build/Release/sysproxy.node');

module.exports = {
  setProxy: sysproxy.setProxy,
  queryProxy: sysproxy.queryProxy,  
};
