{
    "targets": [{
        "target_name": "webview2",
        "cflags": [ "/D_UNICODE", "/DUNICODE", "/DWIN32", "/D_WINDOWS", "/ZI" ],
        "sources": [
            "src/main.cpp"
        ],
        'include_dirs': [
            "<!@(node -p \"require('node-addon-api').include\")",
            "packages\\Microsoft.Windows.ImplementationLibrary.1.0.240122.1\\include",
            "packages\\Microsoft.Web.WebView2.1.0.2365.46\\build\\native\\include",
            "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.22621.0\\winrt"
        ],
        'libraries': [
	    "..\\packages\\Microsoft.Web.WebView2.1.0.2365.46\\build\\native\\x86\\WebView2LoaderStatic.lib"
        ],
        'dependencies': [
            "<!(node -p \"require('node-addon-api').gyp\")"
        ],
        'defines': [ 'NAPI_CPP_EXCEPTIONS','WIL_ENABLE_EXCEPTIONS' ]
    }]
}