#!/usr/bin/python3
# usage python3 echoTcpServer.py [bind IP]  [bind PORT]

import socket
import sys

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# Bind the socket to the port
server_address = ((sys.argv[1]), (int(sys.argv[2])))
sock.bind(server_address)
# Listen for incoming connections
sock.listen(1)

while True:
    # Wait for a connection
    print('waiting for a connection')
    connection, client_address = sock.accept()
    try:
        print('connection from', client_address)

        # Receive the data in small chunks and retransmit it
        while True:
            data = connection.recv(1024)
            if not data:
                break
            print(f"{data=}")
            connection.sendall(data)

    except Exception as e:
        print(e)
    finally:
        # Clean up the connection
        connection.close()
