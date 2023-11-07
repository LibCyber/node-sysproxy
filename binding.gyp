{
  "targets": [
    {
      "target_name": "sysproxy_windows_x64",
      "sources": [ "src/sysproxy_windows.cc" ],
      "include_dirs": ["<!@(node -p \"require('node-addon-api').include\")"],
      "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
      "libraries": ["Wininet.lib", "Rasapi32.lib"],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS","UNICODE", "_UNICODE" ],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "AdditionalOptions": [ "/utf-8" ]
            }
          },
          "target_arch": "x64"
        }]
      ],
    }
  ]
}
