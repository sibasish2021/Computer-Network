/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 Universita' di Firenze, Italy
 *
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
 *
 * Author: Tommaso Pecorella <tommaso.pecorella@unifi.it>
 */

// Network topology
//
//    SRC
//     |<=== source network
//     A-----B
//      \   / \   all networks have cost 1, except
//       \ /  |   for the direct link from C to D, which
//        C  /    has cost 10
//        | /
//        |/
//        D
//        |<=== target network
//       DST
//
//
// A, B, C and D are RIPng routers.
// A and D are configured with static addresses.
// SRC and DST will exchange packets.
//
// After about 3 seconds, the topology is built, and Echo Reply will be received.
// After 40 seconds, the link between B and D will break, causing a route failure.
// After 44 seconds from the failure, the routers will recovery from the failure.
// Split Horizoning should affect the recovery time, but it is not. See the manual
// for an explanation of this effect.
//
// If "showPings" is enabled, the user will see:
// 1) if the ping has been acknowledged
// 2) if a Destination Unreachable has been received by the sender
// 3) nothing, when the Echo Request has been received by the destination but
//    the Echo Reply is unable to reach the sender.
// Examining the .pcap files with Wireshark can confirm this effect.


#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/netanim-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RipSimpleRouting");

void TearDownLink (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t interfaceA, uint32_t interfaceB)
{
  nodeA->GetObject<Ipv4> ()->SetDown (interfaceA);
  nodeB->GetObject<Ipv4> ()->SetDown (interfaceB);
}
void SetupL (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t interfaceA, uint32_t interfaceB)
{
  nodeA->GetObject<Ipv4> ()->SetUp (interfaceA);
  nodeB->GetObject<Ipv4> ()->SetUp (interfaceB);
}
int main (int argc, char **argv)
{
  bool verbose = false;
  bool printRoutingTables = true;
  bool showPings = false;
  std::string SplitHorizon ("SplitHorizon");

  CommandLine cmd;
  cmd.AddValue ("verbose", "turn on log components", verbose);
  cmd.AddValue ("printRoutingTables", "Print routing tables at 30, 60 and 90 seconds", printRoutingTables);
  cmd.AddValue ("showPings", "Show Ping6 reception", showPings);
  cmd.AddValue ("splitHorizonStrategy", "Split Horizon strategy to use (NoSplitHorizon, SplitHorizon, PoisonReverse)", SplitHorizon);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnableAll (LogLevel (LOG_PREFIX_TIME | LOG_PREFIX_NODE));
      LogComponentEnable ("RipSimpleRouting", LOG_LEVEL_INFO);
      LogComponentEnable ("Rip", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv4Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("Icmpv4L4Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("ArpCache", LOG_LEVEL_ALL);
      LogComponentEnable ("V4Ping", LOG_LEVEL_ALL);
    }

  if (SplitHorizon == "NoSplitHorizon")
    {
      Config::SetDefault ("ns3::Rip::SplitHorizon", EnumValue (RipNg::NO_SPLIT_HORIZON));
    }
  else if (SplitHorizon == "SplitHorizon")
    {
      Config::SetDefault ("ns3::Rip::SplitHorizon", EnumValue (RipNg::SPLIT_HORIZON));
    }
  else
    {
      Config::SetDefault ("ns3::Rip::SplitHorizon", EnumValue (RipNg::POISON_REVERSE));
    }

  NS_LOG_INFO ("Create nodes.");
  Ptr<Node> src = CreateObject<Node> ();
  Names::Add ("SrcNode", src);
  Ptr<Node> dst = CreateObject<Node> ();
  Names::Add ("DstNode", dst);
  Ptr<Node> r1 = CreateObject<Node> ();
  Names::Add ("RouterA", r1);
  Ptr<Node> r2 = CreateObject<Node> ();
  Names::Add ("RouterB", r2);
  Ptr<Node> r3 = CreateObject<Node> ();
  Names::Add ("RouterC", r3);
  // Ptr<Node> d = CreateObject<Node> ();
  // Names::Add ("RouterD", d);
  NodeContainer net1 (src, r1);
  NodeContainer net2 (r1, r2);
  NodeContainer net3 (r1, r3);
  NodeContainer net4 (r2, r3);
  // NodeContainer net5 (c, d);
  // NodeContainer net6 (b, d);
  NodeContainer net5 (r3, dst);
  NodeContainer routers (r1, r2, r3);
  NodeContainer nodes (src, dst);


  NS_LOG_INFO ("Create channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer ndc1 = csma.Install (net1);
  NetDeviceContainer ndc2 = csma.Install (net2);
  NetDeviceContainer ndc3 = csma.Install (net3);
  NetDeviceContainer ndc4 = csma.Install (net4);
  NetDeviceContainer ndc5 = csma.Install (net5);
  // NetDeviceContainer ndc6 = csma.Install (net6);
  // NetDeviceContainer ndc7 = csma.Install (net7);

  NS_LOG_INFO ("Create IPv4 and routing");
  RipHelper ripRouting;

  // Rule of thumb:
  // Interfaces are added sequentially, starting from 0
  // However, interface 0 is always the loopback...
  // ripRouting.ExcludeInterface (r1, 1);
  // ripRouting.ExcludeInterface (c, 3);

  // ripRouting.SetInterfaceMetric (c, 3, 10);
  // ripRouting.SetInterfaceMetric (r3, 1, 10);

  Ipv4ListRoutingHelper listRH;
  listRH.Add (ripRouting, 0);
//  Ipv4StaticRoutingHelper staticRh;
//  listRH.Add (staticRh, 5);

  InternetStackHelper internet;
  internet.SetIpv6StackInstall (false);
  internet.SetRoutingHelper (listRH);
  internet.Install (routers);

  InternetStackHelper internetNodes;
  internetNodes.SetIpv6StackInstall (false);
  internetNodes.Install (nodes);

  // Assign addresses.
  // The source and destination networks have global addresses
  // The "core" network just needs link-local addresses for routing.
  // We assign global addresses to the routers as well to receive
  // ICMPv6 errors.
  NS_LOG_INFO ("Assign IPv4 Addresses.");
  Ipv4AddressHelper ipv4;

  ipv4.SetBase (Ipv4Address ("10.0.0.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic1 = ipv4.Assign (ndc1);

  ipv4.SetBase (Ipv4Address ("10.0.1.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic2 = ipv4.Assign (ndc2);

  ipv4.SetBase (Ipv4Address ("10.0.2.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic3 = ipv4.Assign (ndc3);

  ipv4.SetBase (Ipv4Address ("10.0.3.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic4 = ipv4.Assign (ndc4);

  ipv4.SetBase (Ipv4Address ("10.0.4.0"), Ipv4Mask ("255.255.255.0"));
  Ipv4InterfaceContainer iic5 = ipv4.Assign (ndc5);

  // ipv4.SetBase (Ipv4Address ("10.0.5.0"), Ipv4Mask ("255.255.255.0"));
  // Ipv4InterfaceContainer iic6 = ipv4.Assign (ndc6);

  // ipv4.SetBase (Ipv4Address ("10.0.6.0"), Ipv4Mask ("255.255.255.0"));
  // Ipv4InterfaceContainer iic7 = ipv4.Assign (ndc7);

  Ptr<Ipv4StaticRouting> staticRouting;
  staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (src->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute ("10.0.0.2", 1 );
  staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (dst->GetObject<Ipv4> ()->GetRoutingProtocol ());
  staticRouting->SetDefaultRoute ("10.0.4.1", 1 );

  if (printRoutingTables)
    {
      RipHelper routingHelper;
      AsciiTraceHelper asciiTraceHelper;
      // Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
      Ptr<OutputStreamWrapper> routingStream1 = asciiTraceHelper.CreateFileStream ("ROUTER1.cwnd");
      Ptr<OutputStreamWrapper> routingStream2 = asciiTraceHelper.CreateFileStream ("ROUTER2.cwnd");
      Ptr<OutputStreamWrapper> routingStream3 = asciiTraceHelper.CreateFileStream ("ROUTER3.cwnd");
      routingHelper.PrintRoutingTableAt (Seconds (121.0), r1, routingStream1);
      routingHelper.PrintRoutingTableAt (Seconds (121.0), r2, routingStream2);
      routingHelper.PrintRoutingTableAt (Seconds (121.0), r3, routingStream3);

      routingHelper.PrintRoutingTableAt (Seconds (180.0), r1, routingStream1);
      routingHelper.PrintRoutingTableAt (Seconds (180.0), r2, routingStream2);
      routingHelper.PrintRoutingTableAt (Seconds (180.0), r3, routingStream3);
      // routingHelper.PrintRoutingTableAt (Seconds (30.0), r4, routingStream);

      // routingHelper.PrintRoutingTableAt (Seconds (10.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (10.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (10.0), r3, routingStream3);

      // routingHelper.PrintRoutingTableAt (Seconds (12.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (12.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (12.0), r3, routingStream3);

      // routingHelper.PrintRoutingTableAt (Seconds (50.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (50.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (50.0), r3, routingStream3);

      // routingHelper.PrintRoutingTableAt (Seconds (60.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (60.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (60.0), r3, routingStream3);

      // routingHelper.PrintRoutingTableAt (Seconds (100.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (100.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (100.0), r3, routingStream3);

      // routingHelper.PrintRoutingTableAt (Seconds (120.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (120.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (120.0), r3, routingStream3);

      // routingHelper.PrintRoutingTableAt (Seconds (500.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (500.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (500.0), r3, routingStream3);

      // routingHelper.PrintRoutingTableAt (Seconds (550.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (550.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (550.0), r3, routingStream3);

      // routingHelper.PrintRoutingTableAt (Seconds (60.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (60.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (60.0), r3, routingStream3);
      // // routingHelper.PrintRoutingTableAt (Seconds (60.0), d, routingStream);

      // routingHelper.PrintRoutingTableAt (Seconds (90.0), r1, routingStream1);
      // routingHelper.PrintRoutingTableAt (Seconds (90.0), r2, routingStream2);
      // routingHelper.PrintRoutingTableAt (Seconds (90.0), r3, routingStream3);
      // routingHelper.PrintRoutingTableAt (Seconds (90.0), d, routingStream);
    }

  NS_LOG_INFO ("Create Applications.");
  uint32_t packetSize = 1024;
  Time interPacketInterval = Seconds (1.0);
  V4PingHelper ping ("10.0.4.2");

  ping.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping.SetAttribute ("Size", UintegerValue (packetSize));
  if (showPings)
    {
      ping.SetAttribute ("Verbose", BooleanValue (true));
    }
  ApplicationContainer apps = ping.Install (src);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (1000.0));

  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("rip-simple-routing.tr"));
  csma.EnablePcapAll ("rip-simple-routing", true);

  Simulator::Schedule (Seconds (50), &TearDownLink, r1, r2, 2, 1);
  Simulator::Schedule (Seconds(50),&TearDownLink, r1,r3, 3,1 );
  Simulator::Schedule (Seconds (120), &SetupL, r1, r2, 2, 1);
  Simulator::Schedule (Seconds(120),&SetupL, r1,r3, 3,1 );
  // AnimationInterface anim("anime5.xml");
  /* Now, do the actual simulation. */
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (1000.0));
  Simulator::Run ();
  Simulator::Destroy ();
  
//   Simulator::Stop (Seconds (30));
//   Simulator::Run ();
//   Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}

