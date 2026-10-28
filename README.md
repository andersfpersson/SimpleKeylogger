# SimpleKeylogger

This repository contains an educational Windows key-capture example and a
separate, consent-based diagnostic uploader. Do not run the key-capture example
on a device or account without the owner's explicit permission. The uploader
does **not** read or transmit `log.txt`, keystrokes, passwords, or other user
content.

This project is purely for learning and educational purposes. It is not intended
for use on systems, accounts, or data without explicit authorization.

Any use of this project is entirely at your own risk. You are solely
responsible for how you use it and for complying with all applicable laws and
regulations. I am not in any way responsible or liable for any misuse,
unauthorized access, damage, or other consequences resulting from this project.

## Prerequisites

- Windows 10 or later for the C++ programs.
- Visual Studio 2022 with the **Desktop development with C++** workload.
- Python 3.9 or later for the HTTPS receiver.
- An HTTPS certificate and private key for the receiver. For production, use a
  certificate issued by a trusted CA; do not use a self-signed certificate
  outside a controlled test environment.

## Build

From a **Developer Command Prompt for VS 2022**:

```bat
cl /std:c++17 /EHsc diagnostic_uploader.cpp
```

The resulting `diagnostic_uploader.exe` uses Windows' built-in WinHTTP library,
so no third-party C++ packages are required.

## Start the diagnostic receiver

Set the same long, random token on the receiver and client. Keep it secret.

```powershell
$env:SIMPLEKEYLOGGER_DIAGNOSTIC_TOKEN = "replace-with-a-long-random-token"
$env:SIMPLEKEYLOGGER_CERT_FILE = "C:\path\server.crt"
$env:SIMPLEKEYLOGGER_KEY_FILE = "C:\path\server.key"
$env:PORT = "8443"
python diagnostic_server.py
```

The receiver accepts only `POST /report`, requires the bearer token, validates
the report type, and stores JSON files under `received_reports/`.

## Send a report

Run this in a visible terminal on the authorized Windows computer. The program
requires both an environment token and an interactive confirmation:

```powershell
$env:SIMPLEKEYLOGGER_DIAGNOSTIC_TOKEN = "replace-with-a-long-random-token"
.\diagnostic_uploader.exe --endpoint https://server.example:8443/report --consent
```

The report contains only the report type, UTC timestamp, computer name, and a
fixed diagnostic message. There is no background listener, autorun behavior,
hidden window, or automatic upload. Review and minimize collected fields before
using this in production; rotate tokens and restrict network access as needed.
