/*
 * Essaie de mise en place d'une topologie de simulation réseau domestique avec un seul serveur distant.
 * Topologie : Devices (Gauche) - AP (Centre) - Server (Droite)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h" 

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDataGeneration");

int main (int argc, char *argv[])
{
  // Simulation Parameters
  uint32_t nWifi = 10; 
  bool verbose = true;
  
  CommandLine cmd (__FILE__);
  cmd.AddValue ("nWifi", "Nombre de stations wifi (N)", nWifi);
  cmd.AddValue ("verbose", "Activer les logs", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
  {
      // logs of the applcation
  }

  // Creating my nodes (Mes noeuds)
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi); // Les appareils

  NodeContainer wifiApNode;
  wifiApNode.Create (1);       // La Box

  NodeContainer serverNode;
  serverNode.Create (1);       // Le Serveur

  // Configuration du Lien Filaire (AP and Server)
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NodeContainer p2pNodes;
  p2pNodes.Add (wifiApNode.Get (0));
  p2pNodes.Add (serverNode.Get (0));
  
  NetDeviceContainer p2pDevices = p2p.Install (p2pNodes);

  // WiFi configuration (802.11ac)
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  
  // algorithme pour adapter le débit
  wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("MaMaisonConnectee");

  // AP
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices = wifi.Install (phy, mac, wifiApNode);

  // Devices
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // Position for simulation in NetAnim
  MobilityHelper mobility;

  
  Ptr<ListPositionAllocator> fixedAlloc = CreateObject<ListPositionAllocator> ();
  fixedAlloc->Add (Vector (35.0, 35.0, 0.0)); // AP in center
  fixedAlloc->Add (Vector (70.0, 35.0, 0.0)); // Server (à droite)
  
  mobility.SetPositionAllocator (fixedAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (serverNode);

  // Devices aligned vertically (à Gauche)
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (15.0),
                                 "MinY", DoubleValue (10.0),
                                 "DeltaX", DoubleValue (0.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  // Internet stack and IP
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);
  stack.Install (serverNode);

  Ipv4AddressHelper address;

  address.SetBase ("192.168.1.0", "255.255.255.0");
  address.Assign (p2pDevices);

  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (apDevices);
  address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // animation file
  AnimationInterface anim ("maison_animation.xml");
  
  // Increase description size
  anim.UpdateNodeDescription (wifiApNode.Get (0), "Access Point");
  anim.UpdateNodeDescription (serverNode.Get (0), "Server");
  
  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
