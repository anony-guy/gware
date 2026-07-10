import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(("127.0.0.1", 8080))
s.sendall(b"GET / HTTP/1.1\r\n\r\n")
print(s.recv(1024).decode("utf-8"))
s.close()
