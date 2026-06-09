from pathlib import Path


Import("env")
project_dir = Path(env.subst("$PROJECT_DIR"))
pioenv = env.subst("$PIOENV")
alpaca_src = project_dir / ".pio" / "libdeps" / pioenv / "ESP32AlpacaDevices" / "src"

debug_h = alpaca_src / "AlpacaDebug.h"
server_cpp = alpaca_src / "AlpacaServer.cpp"

if debug_h.exists():
    text = debug_h.read_text()
    text = text.replace(
        "extern const char *const WebRequestMethod2Str(uint8_t method);",
        "extern const char *const WebRequestMethod2Str(uint8_t method);",
    )
    text = text.replace(
        "extern const char *const WebRequestMethod2Str(WebRequestMethodComposite method);",
        "extern const char *const WebRequestMethod2Str(uint8_t method);",
    )
    text = text.replace(
        "WebRequestMethod2Str(request->method())",
        "WebRequestMethod2Str((uint8_t)request->method())",
    )
    debug_h.write_text(text)

device_cpp = alpaca_src / "AlpacaDevice.cpp"
if device_cpp.exists():
    text = device_cpp.read_text()
    text = text.replace("WebRequestMethod2Str(type)", "WebRequestMethod2Str(0)")
    device_cpp.write_text(text)

for focuser_path in (alpaca_src / "AlpacaFocuser.h", alpaca_src / "AlpacaFocuser.cpp"):
    if not focuser_path.exists():
        continue

    text = focuser_path.read_text()
    text = text.replace("_alpacaGetAbsolutee", "_alpacaGetAbsolute")
    text = text.replace("_alpacaGetAbsolut(", "_alpacaGetAbsolute(")
    text = text.replace("_alpacaGetAbsolut)", "_alpacaGetAbsolute)")
    text = text.replace("_alpacaGetAbsolut;", "_alpacaGetAbsolute;")
    focuser_path.write_text(text)

if server_cpp.exists():
    text = server_cpp.read_text()
    old = """const char *const WebRequestMethod2Str(uint8_t method)
{
    int idx = 0;
    switch (method)
    {
    case HTTP_GET:
        idx = 0;
        break;
    case HTTP_POST:
        idx = 1;
        break;
    case HTTP_DELETE:
        idx = 2;
        break;
    case HTTP_PUT:
        idx = 3;
        break;
    case HTTP_PATCH:
        idx = 4;
        break;
    case HTTP_HEAD:
        idx = 5;
        break;
    case HTTP_OPTIONS:
        idx = 6;
        break;
    default:
        idx = 8;
        break;
    }
    return k_web_request_methode_str[idx];
}
"""
    new = """const char *const WebRequestMethod2Str(uint8_t method)
{
    int idx = 8;
    if (method == (uint8_t)HTTP_GET)
        idx = 0;
    else if (method == (uint8_t)HTTP_POST)
        idx = 1;
    else if (method == (uint8_t)HTTP_DELETE)
        idx = 2;
    else if (method == (uint8_t)HTTP_PUT)
        idx = 3;
    else if (method == (uint8_t)HTTP_PATCH)
        idx = 4;
    else if (method == (uint8_t)HTTP_HEAD)
        idx = 5;
    else if (method == (uint8_t)HTTP_OPTIONS)
        idx = 6;

    return k_web_request_methode_str[idx];
}
"""
    if old in text:
        server_cpp.write_text(text.replace(old, new))
    else:
        text = text.replace(
            "const char *const WebRequestMethod2Str(WebRequestMethodComposite method)",
            "const char *const WebRequestMethod2Str(uint8_t method)",
        )
        text = text.replace(
            """    case HTTP_ANY:
        idx = 7;
        break;
""",
            "",
        )
        server_cpp.write_text(text)
