#!/usr/bin/env python3
"""Receive explicitly approved SimpleKeylogger diagnostic reports over HTTPS."""

import json
import os
import ssl
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path


TOKEN = os.environ.get("SIMPLEKEYLOGGER_DIAGNOSTIC_TOKEN")
OUTPUT_DIR = Path(os.environ.get("SIMPLEKEYLOGGER_REPORT_DIR", "received_reports"))


class ReportHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != "/report":
            self.send_error(404)
            return

        expected = f"Bearer {TOKEN}" if TOKEN else ""
        if self.headers.get("Authorization") != expected:
            self.send_error(401)
            return

        try:
            length = int(self.headers["Content-Length"])
            report = json.loads(self.rfile.read(length))
            if report.get("report_type") != "simplekeylogger-diagnostic":
                raise ValueError("unexpected report type")
        except (KeyError, ValueError, json.JSONDecodeError):
            self.send_error(400)
            return

        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        destination = OUTPUT_DIR / f"report-{len(list(OUTPUT_DIR.glob('report-*.json'))):06d}.json"
        destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        self.send_response(201)
        self.end_headers()
        self.wfile.write(b"accepted\n")

    def log_message(self, format, *args):
        print(format % args)


def main():
    if not TOKEN:
        raise SystemExit("Set SIMPLEKEYLOGGER_DIAGNOSTIC_TOKEN first.")
    certfile = os.environ.get("SIMPLEKEYLOGGER_CERT_FILE")
    keyfile = os.environ.get("SIMPLEKEYLOGGER_KEY_FILE")
    if not certfile or not keyfile:
        raise SystemExit("Set SIMPLEKEYLOGGER_CERT_FILE and SIMPLEKEYLOGGER_KEY_FILE.")

    server = HTTPServer(("0.0.0.0", int(os.environ.get("PORT", "8443"))), ReportHandler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile, keyfile)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    print("Listening for diagnostic reports on HTTPS /report")
    server.serve_forever()


if __name__ == "__main__":
    main()
