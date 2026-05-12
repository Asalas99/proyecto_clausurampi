#!/usr/bin/env python3
"""Servidor HTTP local que sirve archivos con latencia simulada,
para emular descargas reales de Project Gutenberg en pruebas."""
import http.server, socketserver, time, sys, os

DELAY_S = float(os.environ.get("BOW_DELAY", "1.5"))  # 1.5 s por petición
PORT    = int(os.environ.get("BOW_PORT", "8765"))

class SlowHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        time.sleep(DELAY_S)
        return super().do_GET()
    def log_message(self, *a, **kw): pass  # silencio

class ReusableTCPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

os.chdir(sys.argv[1] if len(sys.argv) > 1 else ".")
with ReusableTCPServer(("127.0.0.1", PORT), SlowHandler) as httpd:
    print(f"Sirviendo {os.getcwd()} en http://127.0.0.1:{PORT}/  (delay={DELAY_S}s)")
    httpd.serve_forever()
