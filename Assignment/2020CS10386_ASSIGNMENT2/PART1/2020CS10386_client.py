import socket
import threading
import time
import hashlib
import random

ServerIP="127.0.0.1"
ClientIP="127.0.0.1"

serverTCPport=50000
serverUDPport=55000
serverbroadcastport=60000

baseportTCP=35000
baseportUDP=40000
baseportbroadcast=45000

noClients=5
bufferSize=2048
maxChunks=0
datafile="./A2_small_file.txt"

class Client:
    def __init__(self, id):
        self.lock=threading.Lock()
        self.chunks=[]
        self.id=id
        self.noChunks=0
        self.rtt=[]
        #print("Client debug 1")
        self.TCP_socket=socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
        self.TCP_socket.bind((ServerIP,baseportTCP+self.id)) 
        self.TCP_socket.connect((ServerIP,serverTCPport+self.id))
        #print("Client debug 2")
        self.UDP_socket=socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
        self.UDP_socket.bind((ServerIP,baseportUDP+self.id))
        #print("Client debug 3")
        self.broadcastUDP_socket=socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
        self.broadcastUDP_socket.bind((ServerIP,baseportbroadcast+self.id))

    def sDataTCP(self,data):
        self.TCP_socket.send(data.encode())
        # print("Data sent")
        while(True):
                UDPmsg,_=self.UDP_socket.recvfrom(bufferSize)
                msg=UDPmsg.decode()
                if(msg=="Success"):
                    break;
    def rDataTCP(self):
        while(True):
            msg=self.TCP_socket.recv(bufferSize).decode('utf-8','ignore')
            # print("Message from server",msg)
            if(len(msg)==0):
                continue
            bytestoSend="Success"
            self.UDP_socket.sendto(bytestoSend.encode(),(ServerIP,serverUDPport+self.id))
            break
                # print(msg)
        return msg
    
    
    
    def sDataUDP(self ,data):
        self.UDP_socket.sendto(data.encode(),(ServerIP,serverUDPport+self.id))
        while(True):        
            UDPmsg,_=self.UDP_socket.recvfrom(bufferSize)
            msg=UDPmsg.decode()
            if(msg=="Success"):
                break;
                 
    def rDataUDP(self ):
        while(True):
            
            # print("Message from server",msg)
            
            msg,_=self.UDP_socket.recvfrom(bufferSize)
            msg=msg.decode()    
            bytestoSend="Success"
            self.UDP_socket.sendto(bytestoSend.encode(),(ServerIP,serverUDPport+self.id))
            break                
        return msg        
    def sDatabroad(self ,data):
        self.broadcastUDP_socket.sendto(data.encode(),(ServerIP,serverbroadcastport+self.id))
        while(True):
            # print(msg)    
            UDPmsg,_=self.broadcastUDP_socket.recvfrom(bufferSize)
            msg=UDPmsg.decode()
            if(msg=="Success"):
                break;
    
    def rDatabroad(self):
        # print("function call successfull")
        while(True):
            
            # print("Message from server",msg)
            
            msg,_=self.broadcastUDP_socket.recvfrom(bufferSize)
            msg=msg.decode()
                
            # print("message received from server",msg)
            bytestoSend="Success"
            self.broadcastUDP_socket.sendto(bytestoSend.encode(),(ServerIP,serverbroadcastport+self.id))
            break
                
        return msg
    

    
    def getChunk(self):
        b=True
        rtt1=time.time()
        # print("Getting chunks")
        while(True):
            if(b):
                UDPmsg=self.rDataUDP()
                if(len(UDPmsg)==0):
                    continue
                UDPmsg=UDPmsg.split(',')
                if(UDPmsg[0]=="Sending chunks"):
                    if(self.chunks==[]):
                        self.chunks=[None]*(int(UDPmsg[1]))
                        self.noChunks=int(UDPmsg[1])
                        self.rtt=[None]*self.noChunks
                        b=False
                        # print("Got control signal for receiving chunks")
            else:
                # print("getting data over TCP")
                UDPmsg=self.rDataUDP()
                pos=int(UDPmsg)
                if(pos==-1):
                    break   
                msg=self.rDataTCP()
                
                if(len(msg)!=0):
                    # print("Got data via TCP")
                    # if("Terminated" in msg):
                    #     print("Terminating connection")
                    #     break
                    # msg=msg.split('@')
                    # print("Chunk received from server",pos)
                    self.chunks[pos]=msg
                    self.rtt[pos]=time.time()-rtt1
                    # f= open("HEllo"+str(self.id)+".txt","a+")
                    # f.write(str(pos)+"\t"+msg+"\n")
                    # f.close()

    
    def handle_broadcast(self):
        while(True):
            # print("Broadcast started for client",self.id)
            msg=self.rDatabroad()
            # print(f"Broadcast at client{self.id} for chunk{msg}")
            if(len(msg)==0):
                continue
            if("EndOfTransfer" in msg):
                break
            if("Get" in msg):
                # print(f"get msg received for client{self.id}",msg)
                msg=msg.split("-")
                pos=int(msg[1])
                # print(f"Request to client{self.id}for packet at{pos}")
                if(self.chunks[pos] is None):
                    self.sDatabroad("NO")
                    # print("Chunk not found",self.id,pos)
                else:
                    # print(f"Request to client{self.id}for packet at{pos} by serevr")
                    self.sDatabroad("Sending")
                    self.sDataTCP(self.chunks[pos])
                    # print(f"Request to client{self.id}for packet at{pos} sending now")                
                    
    

    def getAllChunks(self):
        # randlist=[]
        # for i in range(self.noChunks):
        #     if(self.chunks[i] is None):
        #         randlist.append(i)
        # # print(self.noChunks)        
        # while(len(randlist)):
        #     # print("List is",self.id,randlist)
        #     if(len(randlist)==1):
        #         i=0
        #     else:    
        #         i=random.randint(0,len(randlist)-1)
        #     UDPmsg="Get-"+str(randlist[i])
            # print("Sent UDP message for getting chunk",i,self.id)
            
            # print("Sent TCP message for getting chunk",i,self.id)
            
            # with self.lock:
            #     self.sDataUDP(UDPmsg)
            #     chunk=self.rDataTCP()
            #     self.chunks[i]=chunk
            # randlist.remove(randlist[i])
            # for i in range(self.noChunks):
            #     if(self.chunks[i] is None):
            #         self.getAllChunks()
        # print(randlist)        
        for i in range(self.noChunks):
            if(self.chunks[i] is None):
                rtt1=time.time()
                UDPmsg="Get-"+str(i)
                # print("Sent UDP message for getting chunk",i,self.id)
                self.sDataUDP(UDPmsg)
                # print("Sent TCP message for getting chunk",i,self.id)
                chunk=self.rDataTCP()
                with self.lock:
                    self.chunks[i]=chunk
                    self.rtt[i]=time.time()-rtt1
                # f=open("hello"+str(self.id)+".txt","a+")
                # f.write(str(i)+"\t"+chunk+"\n")
                # f.close()    
                # print("got chunk for client ",i,self.id)    
        self.sDataUDP("Goodbye")
            
    def combine_chunks(self):
        filename="data_client"+str(self.id)+".txt"
        f= open(filename,"w")
        for i in range(self.noChunks):
            if(self.chunks[i]!=None):
                f.write(self.chunks[i])
        f.close()
    
    def end_connections(self):
        self.TCP_socket.close()
        self.UDP_socket.close();
        self.broadcastUDP_socket.close()    
        

if __name__=="__main__":
    time.sleep(10)
    start_time=time.time()
    print("Start time",start_time)
    threads=[]
    clients=[]
    for i in range(noClients):
        clients.append(Client(i))   
    for i in range(noClients):
        thread=threading.Thread(target=Client.getChunk,args=(clients[i],))
        threads.append(thread)
        threads[i].start()    
    for i in range(noClients):
        threads[i].join()
    print("get one chunk thread joined")    

    t=[]    
    for i in range(noClients):
        thread=threading.Thread(target=Client.handle_broadcast,args=(clients[i],))
        thread.start()
        t.append(thread)
    chunksthreads=[]
    
    for i in range(noClients):
        thread=threading.Thread(target=Client.getAllChunks,args=(clients[i],))
        chunksthreads.append(thread)
        thread.start()    
    
    for i in range(noClients):
        chunksthreads[i].join()
    print("Get chunk thread joined")
 
    for i in range(noClients):
        t[i].join()    
    
    threads=[]
    
    for i in range(noClients):
        thread=threading.Thread(target=Client.combine_chunks,args=(clients[i],))
        threads.append(thread)
        thread.start()
        
    for i in range(noClients):
        threads[i].join()
    
    hash = hashlib.md5(open(datafile, 'r').read().encode()).hexdigest()
    print("Server",hash)    
    for i in range(noClients):
        hash = hashlib.md5(open("data_client"+str(i)+".txt", 'r').read().encode()).hexdigest()
        print("Client",i,hash)     
    for i in range(noClients):
        clients[i].end_connections()   
    print("Client connections ended")
    # s=0    
    # for i in range(noClients):
    #     for j in range(clients[i].noChunks):
    #         s=s+clients[i].rtt[j]
    # print("Average RTT for part 1",s/(noClients*clients[0].noChunks))
    # for i in range(clients[0].noChunks):
    #     s=0
    #     for j in range(noClients):
    #         s=s+clients[j].rtt[i]
    #     print(f"Time for Chunk{i}:",s/noClients) 
    end_time=time.time()
    print("end time",end_time)
    print("time elaplsed",end_time-start_time)