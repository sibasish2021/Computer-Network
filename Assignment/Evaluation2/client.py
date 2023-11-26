from chunks import *
import socket
import time
ServerIP="127.0.0.1"
ClientIP="127.0.0.1"

serverTCPport=50000
baseportTCP=35000

bufferSize=1024

class Client:
    def __init__(self):
        self.TCP_socket=socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
        self.TCP_socket.bind((ServerIP,baseportTCP)) 
        self.TCP_socket.connect((ServerIP,serverTCPport))
        
if __name__=="__main__":
    time.sleep(3)
    c=Client()
    # print("Client created")
    id=1
    chunks=[]
    while(id<=10):
        data=str(id)
        c.TCP_socket.send(data.encode())
        msg=c.TCP_socket.recv(bufferSize).decode()
        msg=msg.split("@")
        chunk=msg[0]
        hash=msg[1]
        if(hash==get_hash(chunk)):
            id=id+1
            chunks.append(chunk)
    data="Close"        
    c.TCP_socket.send(data.encode()) 
    filename="output.txt"
    f=open(filename,"w")   
    for chunk in chunks:
        f.write(chunk)
    f.close()           
    c.TCP_socket.close()
    # print("Closed connection")        