#!/usr/bin/python3
"""假 mpv：按 --input-ipc-server= 建 Unix 套接字并回显收到的数据。

环境变量：MPV_DELAY_MS 延迟创建套接字；MPV_NEVER=1 永不创建；MPV_PIDFILE 写入自身 pid。
"""
import os
import socket
import sys
import time

path = None
for arg in sys.argv[1:]:
    if arg.startswith("--input-ipc-server="):
        path = arg.split("=", 1)[1]
pidfile = os.environ.get("MPV_PIDFILE")
if pidfile:
    with open(pidfile, "w") as f:
        f.write("%d\n" % os.getpid())
time.sleep(int(os.environ.get("MPV_BEFORE_UNLINK_MS", "0")) / 1000.0)
if path:
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass
time.sleep(int(os.environ.get("MPV_DELAY_MS", "0")) / 1000.0)
if not path or os.environ.get("MPV_NEVER") == "1":
    while True:
        time.sleep(3600)
srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
srv.bind(path)
srv.listen(1)
conn, _ = srv.accept()
while True:
    data = conn.recv(4096)
    if not data:
        break
    conn.sendall(data)
