from chunks import *
import socket

chunkSize=1000
ServerIP="127.0.0.1"
ClientIP="127.0.0.1"

ServerportTCP=50000
ServerportUDP=55000

clientBasePortUDP=40000
clientBasePortTCP=35000
bufferSize=1024
noClients=1

class Server:
    def __init__(self):
        self.TCP_sockets=socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM) 
        self.TCP_sockets.bind((ServerIP,ServerportTCP))
        self.TCP_sockets.listen(1)
        self.TCP_connection=self.TCP_sockets.accept()[0]
        # print("Server setup successfull")
        
if __name__=="__main__":
    s=Server()
    while(True):
        msg=s.TCP_connection.recv(bufferSize).decode()
        if("Close" in msg):
            break
        chunk,hash=get_chunk(int(msg))
        msgToSend=chunk+"@"+hash
        s.TCP_connection.send(msgToSend.encode())
    s.TCP_sockets.close()
    s.TCP_connection.close()
    # print("Sockets closed")    