/* -- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include<bits/stdc++.h>
#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;
uint32_t maxwindow=0;
uint32_t noPacket=0;
// std::vector<std::pair<double,int>> values;
NS_LOG_COMPONENT_DEFINE ("SeventhScriptExample");

// ===========================================================================
//
//         node 0                 node 1
//   +----------------+    +----------------+
//   |    ns-3 TCP    |    |    ns-3 TCP    |
//   +----------------+    +----------------+
//   |    10.1.1.1    |    |    10.1.1.2    |
//   +----------------+    +----------------+
//   | point-to-point |    | point-to-point |
//   +----------------+    +----------------+
//           |                     |
//           +---------------------+
//                5 Mbps, 2 ms
//
//
// We want to look at changes in the ns-3 TCP congestion window.  We need
// to crank up a flow and hook the CongestionWindow attribute on the socket
// of the sender.  Normally one would use an on-off application to generate a
// flow, but this has a couple of problems.  First, the socket of the on-off
// application is not created until Application Start time, so we wouldn't be
// able to hook the socket (now) at configuration time.  Second, even if we
// could arrange a call after start time, the socket is not public so we
// couldn't get at it.
//
// So, we can cook up a simple version of the on-off application that does what
// we want.  On the plus side we don't need all of the complexity of the on-off
// application.  On the minus side, we don't have a helper, so we have to get
// a little more involved in the details, but this is trivial.
//
// So first, we create a socket and do the trace connect on it; then we pass
// this socket into the constructor of our simple application which we then
// install in the source node.
// ===========================================================================
//
class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();


  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<MyApp> ()
    ;
  return tid;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      m_socket->Bind ();
    }
  else
    {
      m_socket->Bind6 ();
    }
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  
  maxwindow=std::max(maxwindow,newCwnd);
//   values.push_back(std::make_pair(Simulator::Now ().GetSeconds (),newCwnd));
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << std::endl;
}

static void
RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
  noPacket+=1;
  file->Write (Simulator::Now (), p);
}

int
main (int argc, char *argv[])
{
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewRenoPlus"));
  bool useV6 = false;
  CommandLine cmd;
  cmd.AddValue ("useIpv6", "Use Ipv6", useV6);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3);
  NodeContainer n13,n23;
  n13=NodeContainer(nodes.Get(0),nodes.Get(2));
  n23=NodeContainer(nodes.Get(1),nodes.Get(2));
  
  PointToPointHelper p2p1,p2p2 ;
  p2p1.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p1.SetChannelAttribute ("Delay", StringValue ("3ms"));
  p2p2.SetDeviceAttribute ("DataRate", StringValue ("9Mbps"));
  p2p2.SetChannelAttribute ("Delay", StringValue ("3ms"));
  NetDeviceContainer device1,device2;
  device1=p2p1.Install(n13);
  device2=p2p2.Install(n23);
//   for(int i=0;i<4;i++)
//   {
//     devices[i] = p2p.Install (n[i]);
//   }
  Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel> ();
  em1->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  device1.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em1));

  Ptr<RateErrorModel> em2 = CreateObject<RateErrorModel> ();
  em2->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  device2.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em2));

  InternetStackHelper stack;
  stack.Install (nodes);
  
  Ipv4AddressHelper IPAddress13,IPAddress23;

  
  std::string probeType;
  std::string tracePath;
  Ipv4InterfaceContainer interface13,interface23;
  IPAddress13.SetBase("10.1.1.0", "255.255.255.0");
  IPAddress23.SetBase("10.1.2.0", "255.255.255.0");
//   IPAddress[2].SetBase("10.1.3.0", "255.255.255.0");
//   IPAddress[3].SetBase("10.1.4.0", "255.255.255.0");
//   for(int i=0;i<2;i++)
//   {
    
//     interfaces[i]=IPAddress[i].Assign(devices[i]);
//   }
  interface13=IPAddress13.Assign(device1);
  interface23=IPAddress23.Assign(device2);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  

  uint16_t TCPsinkPort1 = 8080;
  uint16_t TCPsinkPort2 = 15000;
  uint16_t TCPsinkPort3 = 20000;
  
  
  PacketSinkHelper packetSinkHelperTCP1 ("ns3::TcpSocketFactory", InetSocketAddress (interface13.GetAddress(1), TCPsinkPort1));
  ApplicationContainer sinkApps1 = packetSinkHelperTCP1.Install (nodes.Get (2));
  sinkApps1.Start (Seconds (1.));
  sinkApps1.Stop (Seconds (20.));

  Ptr<Socket> ns3TcpSocket1 = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());

  Ptr<MyApp> TCPapp1 = CreateObject<MyApp> ();
  Address TCPsinkAddress1=InetSocketAddress (interface13.GetAddress (1), TCPsinkPort1 );
  
  TCPapp1->Setup (ns3TcpSocket1, TCPsinkAddress1, 3000, 100000, DataRate ("1.5Mbps"));
  nodes.Get (0)->AddApplication (TCPapp1);
  TCPapp1->SetStartTime (Seconds (1.));
  TCPapp1->SetStopTime (Seconds (20.));


  PacketSinkHelper packetSinkHelperTCP2 ("ns3::TcpSocketFactory", InetSocketAddress (interface13.GetAddress(1), TCPsinkPort2));
  ApplicationContainer sinkApps2 = packetSinkHelperTCP2.Install (nodes.Get (2));
  sinkApps2.Start (Seconds (5.));
  sinkApps2.Stop (Seconds (25.));

  Ptr<Socket> ns3TcpSocket2 = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());

  Ptr<MyApp> TCPapp2 = CreateObject<MyApp> ();
  Address TCPsinkAddress2=InetSocketAddress (interface13.GetAddress (1), TCPsinkPort2 );
  
  TCPapp2->Setup (ns3TcpSocket2, TCPsinkAddress2, 3000, 100000, DataRate ("1.5Mbps"));
  nodes.Get (0)->AddApplication (TCPapp2);
  TCPapp2->SetStartTime (Seconds (5.));
  TCPapp2->SetStopTime (Seconds (25.));

  PacketSinkHelper packetSinkHelperTCP3 ("ns3::TcpSocketFactory", InetSocketAddress (interface23.GetAddress(1), TCPsinkPort3));
  ApplicationContainer sinkApps3 = packetSinkHelperTCP3.Install (nodes.Get (2));
  sinkApps3.Start (Seconds (15.));
  sinkApps3.Stop (Seconds (30.));

  Ptr<Socket> ns3TcpSocket3 = Socket::CreateSocket (nodes.Get (1), TcpSocketFactory::GetTypeId ());

  Ptr<MyApp> TCPapp3 = CreateObject<MyApp> ();
  Address TCPsinkAddress3=InetSocketAddress (interface23.GetAddress (1), TCPsinkPort3 );
  
  TCPapp3->Setup (ns3TcpSocket3, TCPsinkAddress3, 3000, 100000, DataRate ("1.5Mbps"));
  nodes.Get (1)->AddApplication (TCPapp3);
  TCPapp3->SetStartTime (Seconds (15.));
  TCPapp3->SetStopTime (Seconds (30.));  
//   uint16_t UDPsinkPort = 20000;

//   // Create a packet sink to receive these packets on n2...
//   PacketSinkHelper sinkudp ("ns3::UdpSocketFactory",
//                          InetSocketAddress (interfaces[3].GetAddress(1), UDPsinkPort));
//   ApplicationContainer sink_app= sinkudp.Install (nodes.Get (4));
//   sink_app.Start (Seconds (20.0));
//   sink_app.Stop (Seconds (100.0));

//   Ptr<SocketFactory> udp_socketFactory = nodes.Get (1)->GetObject<UdpSocketFactory> ();
//   Ptr<Socket> ns3UdpSocket = udp_socketFactory->CreateSocket();
  
//   Ptr<MyApp> appudp = CreateObject<MyApp> ();
//   Address sinkudpAddress=InetSocketAddress (interfaces[3].GetAddress (1),  UDPsinkPort );
//   appudp->Setup (ns3UdpSocket, sinkudpAddress, 1040, 10000000, DataRate ("250Kbps"));
//   nodes.Get(1)->AddApplication (appudp);
//   appudp->SetStartTime (Seconds (20.));
//   appudp->SetStopTime (Seconds (30.));
  
//   Ptr<SocketFactory> udp_socketFactory1 = nodes.Get (1)->GetObject<UdpSocketFactory> ();
//   Ptr<Socket> ns3UdpSocket1 = udp_socketFactory->CreateSocket();
//   Ptr<MyApp> appudp1 = CreateObject<MyApp> ();
//   appudp1->Setup (ns3UdpSocket1, sinkudpAddress, 1040, 10000000, DataRate ("500Kbps"));
//   nodes.Get(1)->AddApplication (appudp1);
//   appudp1->SetStartTime (Seconds (30.));
//   appudp1->SetStopTime (Seconds (100.));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream1 = asciiTraceHelper.CreateFileStream ("seventh1.cwnd");
  Ptr<OutputStreamWrapper> stream2 = asciiTraceHelper.CreateFileStream ("seventh2.cwnd");
  Ptr<OutputStreamWrapper> stream3 = asciiTraceHelper.CreateFileStream ("seventh3.cwnd");
  ns3TcpSocket1->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream1));
  ns3TcpSocket2->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream2));
  ns3TcpSocket3->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream3));
  
//   p2p.EnablePcapAll ("Question3");

  PcapHelper pcapHelper;
  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile ("seventh.pcap", std::ios::out, PcapHelper::DLT_PPP);
  
  device1.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file));
  device2.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file));
  
  AnimationInterface anim("anime2.xml");
  Simulator::Stop (Seconds (30));
  Simulator::Run ();
  Simulator::Destroy ();
//   std::ofstream Myfile1("Question11.txt");
//   for(auto v:TCPapp1->values)
//   {
//     Myfile1<<v.first << "\t" << v.second<<"\n";
//   }
//   Myfile1.close();
//   std::ofstream Myfile2("Question12.txt");
//   for(auto v:TCPapp2->values)
//   {
//     Myfile2<<v.first << "\t" << v.second<<"\n";
//   }
//   Myfile2.close();  
//   std::ofstream Myfile3("Question13.txt");
//   for(auto v:TCPapp3->values)
//   {
//     Myfile3<<v.first << "\t" << v.second<<"\n";
//   }
//   Myfile3.close();
  std::cout<<"maxwindow"<<" "<<maxwindow<<" "<<noPacket<<std::endl;
//   std::cout<<"maxwindow"<<" "<<TCPapp1->maxwindow<<" "<<TCPapp1->noPacket<<std::endl;
//   std::cout<<"maxwindow"<<" "<<TCPapp2->maxwindow<<" "<<TCPapp1->noPacket<<std::endl;
//   std::cout<<"maxwindow"<<" "<<TCPapp3->maxwindow<<" "<<TCPapp1->noPacket<<std::endl;

  return 0;
}