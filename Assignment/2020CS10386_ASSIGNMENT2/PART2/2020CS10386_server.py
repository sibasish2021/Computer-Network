import socket
import threading
from cache import *
import time

chunkSize=1000
# noChunks=0
ServerIP="127.0.0.1"
ClientIP="127.0.0.1"

ServerportTCP=50000
ServerportUDP=55000
Serverportbroadcast=60000

clientBasePortUDP=40000
clientBasePortbroadcast=45000
clientBasePortTCP=35000

noClients=5
bufferSize=2048
datafile="./A2_small_file.txt"

class Server:
    def __init__(self):
        self.noChunks=0
        self.TCP_sockets=[None]*noClients
        self.UDP_sockets=[None]*noClients
        self.TCP_connection=[None]*noClients
        self.broadcastTCP_sockets=[None]*noClients
        self.broadcastTCP_connections=[None]*noClients
        self.lock=threading.Lock()
        self.chunks=[]
        self.LRU=LRUCache(noClients)
        for i in range(noClients):
           self.TCP_sockets[i]=socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM) 
           self.TCP_sockets[i].bind((ServerIP,ServerportTCP+i))
           self.TCP_sockets[i].listen(1)
           self.UDP_sockets[i]=socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
           self.UDP_sockets[i].bind((ServerIP,ServerportUDP+i))
           self.broadcastTCP_sockets[i]=socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
           self.broadcastTCP_sockets[i].bind((ServerIP,Serverportbroadcast+i))
        # print("TCP server up and listening")    
        for i in range(noClients):
            self.TCP_connection[i]=self.TCP_sockets[i].accept()[0]

    def sDataTCP(self,id,data):
        # if(data==None):
        #     data="0000"
        self.TCP_connection[id].send(data.encode())
        # print("Data sent")
        while(True):
            
            # print(msg)
            
        
            UDPmsg,_=self.UDP_sockets[id].recvfrom(bufferSize)
            msg=UDPmsg.decode()
            if("Success" in msg):
                break;
        
        # print(f"Confirmation for TCP connection received for client{id}")        
    def rDataTCP(self,id):
        
        msg=self.TCP_connection[id].recv(bufferSize).decode('utf-8','ignore')
        # print("Message from server",msg)
        
        bytestoSend="Success"
        self.UDP_sockets[id].sendto(bytestoSend.encode(),(ServerIP,clientBasePortUDP+id))
            # print(msg)
        return msg 

    def sDataUDP(self,id,data):
        # if(type(data)==NoneType):
        #     return
        self.UDP_sockets[id].sendto(data.encode(),(ServerIP,clientBasePortUDP+id))
        while(True):
                
                # print(msg)
            UDPmsg,_=self.UDP_sockets[id].recvfrom(bufferSize)
            msg=UDPmsg.decode()
            if(msg=="Success"):
                break;
                 
    def rDataUDP(self,id):
        while(True):
            
            # print("Message from server",msg)
            msg,_=self.UDP_sockets[id].recvfrom(bufferSize)
            msg=msg.decode()
            
            bytestoSend="Success"
            self.UDP_sockets[id].sendto(bytestoSend.encode(),(ServerIP,clientBasePortUDP+id))
            break
                      
        return msg        
    def sDatabroad(self,id,data):
        # print("Function call successfull")
        self.broadcastTCP_sockets[id].sendto(data.encode(),(ServerIP,clientBasePortbroadcast+id))
        while(True):
                
                # print(msg)
            UDPmsg,_=self.broadcastTCP_sockets[id].recvfrom(bufferSize)
            msg=UDPmsg.decode() 
            if("Success" in msg):
                break;
                   
    def rDatabroad(self,id):
        msg=""
        while(True):
            # print("Message from server",msg)
            msg,_=self.broadcastTCP_sockets[id].recvfrom(bufferSize)
            msg=msg.decode()
            bytestoSend="Success"
            self.broadcastTCP_sockets[id].sendto(bytestoSend.encode(),(ServerIP,clientBasePortbroadcast+id))
            break   
        return msg

    
    def make_chunks(self,filename):
        file_number = 0
        with open(filename) as f:
            chunk = f.read(chunkSize)
            while chunk:
                self.chunks.append(chunk)
                file_number += 1
                chunk = f.read(chunkSize)
            f.close()    
        self.noChunks=file_number        
        # print("Chunks made")        
        # print(filename,self.noChunks,file_number)
               
    
    def send_initial_chunk(self,id):
        # print(f"Sending Chunk to{id}")
        msgToSend="Sending chunks,"+str(self.noChunks)
        self.sDataTCP(id, msgToSend)
        # print("Sent control signals for sending chunks")    
        # print("LOOP",id,self.noChunks,noClients)
        for i in range(id,self.noChunks,noClients):
            self.sDataTCP(id,str(i))
            self.sDataUDP(id,self.chunks[i])
        # print(f"Terminating Chunks for client{id}")    
        msgToSend="-1"
        self.sDataTCP(id, msgToSend)
    
    
    
    def broadcastForChunk(self,pos):
        # print(f"broadcast for packet at{pos}")
        msg="Get-"+str(pos)
        for i in range(noClients):
            with self.lock:
                # print(f"broadcast for packet at{pos} starting at client{i}")
                self.sDatabroad(i,msg)
                # print(f"broadcast for packet at{pos} client{i} done")
                bmsg=self.rDatabroad(i)
                # print(f"broadcast for packet at{pos} client{i} msg")
            if(bmsg=="Sending"):
                # print(f"broadcast for packet at{pos} client{i} positive")
                chunk=self.rDataTCP(i)
                # print(f"broadcast for packet at{pos} client{i} success",chunk)
                
                    
                return chunk                              
        return -1

    def queries(self,id):
        while(True):
            msg=self.rDataUDP(id)
            if(len(msg)==0):
                continue
            if("Goodbye" in msg):
                break
            if("Get" in msg):
                
                msg,pos=msg.split("-",1)
                pos=int(pos)
                # print(f"Request by client{id}for packet at{pos}")
                chunk=self.LRU.get(pos)
                # print(f"Request by client{id}for packet at{pos}",chunk)
                while(chunk==-1):
                    chunk=self.broadcastForChunk(pos)
                with self.lock:
                    self.LRU.put(pos,chunk)    
                self.sDataTCP(id,chunk)

    def end_connections(self):
        for i in range(noClients):
            self.sDatabroad(i,"EndOfTransfer")
        for i in range(noClients):
               self.TCP_sockets[i].close()
        for i in range(noClients):
               self.TCP_connection[i].close()
        for i in range(noClients):
            self.UDP_sockets[i].close()
        for i in range(noClients):
            self.broadcastTCP_sockets[i].close()
        print("Sockets closed successfully")
        

if __name__=="__main__":
    s=Server()
       
    s.make_chunks(datafile)
    
    threads=[]
    for i in range(noClients):
        thread=threading.Thread(target=s.send_initial_chunk,args=(i,))
        threads.append(thread)
        thread.start()
    for i in range(noClients):
        threads[i].join()
        
                
    print("Threads joined") 
    s.chunks=[None]*s.noChunks
    threads=[]
    for i in range(noClients):
        thread=threading.Thread(target=s.queries,args=(i,))
        threads.append(thread)
        thread.start()
    for i in range(noClients):
        threads[i].join()
        
                
    print("Threads joined")
    print("Endin connections")
    s.end_connections()
    print("Sockets closed") 