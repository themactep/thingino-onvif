#!/usr/bin/python3

import http.server
import os

class CGIServer(http.server.HTTPServer):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.cgi_directories = ["/onvif"]

class CGIHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith('/onvif/'):
            self.run_cgi()
        else:
            super().do_GET()

    def do_POST(self):
        if self.path.startswith('/onvif/'):
            self.run_cgi()
        else:
            self.send_error(501, "Unsupported method")

    def run_cgi(self):
        path = self.translate_path(self.path)
        if not os.path.exists(path):
            self.send_error(404, "File not found")
            return
        if not os.access(path, os.X_OK):
            self.send_error(403, "File not executable")
            return
        try:
            env = os.environ.copy()
            env["REQUEST_METHOD"] = self.command
            if self.command == "POST":
                content_length = int(self.headers.get("Content-Length", 0))
                env["CONTENT_LENGTH"] = str(content_length)
                env["CONTENT_TYPE"] = self.headers.get("Content-Type", "")
                stdin = self.rfile.read(content_length)
                with open(path, 'rb') as f:
                    script = f.read()
                from subprocess import Popen, PIPE
                process = Popen(['python3', path], env=env, stdin=PIPE, stdout=PIPE, stderr=PIPE)
                stdout, stderr = process.communicate(input=stdin if self.command == "POST" else None)
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                self.wfile.write(stdout)
                if stderr:
                    self.wfile.write(stderr)
            else:
                stdout = os.popen(f"python3 {path}").read()
                self.send_response(200)
                self.send_header("Content-type", "text/html")
                self.end_headers()
                self.wfile.write(stdout.encode())
        except Exception as e:
            self.send_error(500, f"CGI error: {str(e)}")

if __name__ == "__main__":
    server = CGIServer(("", 8080), CGIHandler)
    print("Serving at http://localhost:8080")
    server.serve_forever()
