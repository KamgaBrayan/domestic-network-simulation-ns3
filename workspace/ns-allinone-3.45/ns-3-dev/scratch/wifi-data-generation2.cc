// Simulation Réseau Domestique 
// (N Stations - K Types d'Apps - K Serveurs)

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDataGeneration");

int main (int argc, char *argv[])
{
  // My parameters
  uint32_t nWifi = 20;      // Defaut number of devices
  uint32_t nServers = 4;    // K = 4 types of applications
  double simulationTime = 20.0; // simulation duration
  bool verbose = true;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nWifi", "Nombre de stations wifi", nWifi);
  cmd.AddValue ("simulationTime", "Duree de la simulation", simulationTime);
  cmd.Parse (argc, argv);

  // Nodes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);

  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  NodeContainer serverNodes;
  serverNodes.Create (nServers); // K Serveurs

  // Réseau Filaire (Ethernet Switch : AP - Servers)
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NodeContainer wiredNodes;
  wiredNodes.Add (wifiApNode.Get (0));
  wiredNodes.Add (serverNodes);

  NetDeviceContainer wiredDevices = csma.Install (wiredNodes);

  // WiFi (802.11ac)
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");

  WifiMacHelper mac;
  Ssid ssid = Ssid ("MaMaisonConnectee");

  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevices = wifi.Install (phy, mac, wifiApNode);

  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // Position
  MobilityHelper mobility;

  Ptr<ListPositionAllocator> fixedAlloc = CreateObject<ListPositionAllocator> ();
  fixedAlloc->Add (Vector (50.0, 50.0, 0.0)); // AP (au centre)

  // aligning servers
  for (uint32_t i = 0; i < nServers; i++) {
      fixedAlloc->Add (Vector (80.0, 30.0 + (i * 10.0), 0.0)); 
  }
  
  mobility.SetPositionAllocator (fixedAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (serverNodes);

  // aligning devices
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (20.0),
                                 "MinY", DoubleValue (5.0),
                                 "DeltaX", DoubleValue (0.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  // Internet stack
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);
  stack.Install (serverNodes);

  Ipv4AddressHelper address;

  // Réseau Filaire (192.168.1.x)
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wiredInterfaces = address.Assign (wiredDevices);

  // WiFi Network (10.1.1.x)
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Application configuration
  Ptr<UniformRandomVariable> typeRng = CreateObject<UniformRandomVariable> ();
  typeRng->SetAttribute ("Min", DoubleValue (0.0));
  typeRng->SetAttribute ("Max", DoubleValue (3.99)); // Pour avoir 0, 1, 2, 3

  Ptr<UniformRandomVariable> startRng = CreateObject<UniformRandomVariable> ();
  startRng->SetAttribute ("Min", DoubleValue (0.0));
  startRng->SetAttribute ("Max", DoubleValue (5.0)); // Démarrage entre 0 et 5s

  // Listening ports for servers
  uint16_t portBase = 9000;

  // Installing sink on servers
  for (uint32_t k = 0; k < nServers; k++) {
      
      Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), portBase + k));
      
      // Sink UDP
      PacketSinkHelper packetSinkHelperUDP ("ns3::UdpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkAppsUDP = packetSinkHelperUDP.Install (serverNodes.Get(k));
      sinkAppsUDP.Start (Seconds (0.0));
      sinkAppsUDP.Stop (Seconds (simulationTime));

      // Sink TCP
      PacketSinkHelper packetSinkHelperTCP ("ns3::TcpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkAppsTCP = packetSinkHelperTCP.Install (serverNodes.Get(k));
      sinkAppsTCP.Start (Seconds (0.0));
      sinkAppsTCP.Stop (Seconds (simulationTime));
  }

  // Installing client applications on N devices
  for (uint32_t i = 0; i < nWifi; i++) {
      
      int appType = (int)typeRng->GetValue (); // 0, 1, 2 ou 3
      double startTime = startRng->GetValue ();
      
      // Server address depends on app type (Server k)
      // WiredInterfaces: Index 0=AP, Index 1=Server0, Index 2=Server1...
      Ipv4Address serverAddress = wiredInterfaces.GetAddress (appType + 1); 
      uint16_t port = portBase + appType;

      if (verbose) {
          std::cout << "Node " << i << " is Type " << appType 
                    << " -> Server " << appType << " (" << serverAddress << ")" << std::endl;
      }

      if (appType == 0) { 
          // Type 0: Capteur IoT (UDP, Très faible débit, sporadique)
          OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (serverAddress, port)));
          onoff.SetConstantRate (DataRate ("1kbps")); // Débit ridicule
          onoff.SetAttribute ("PacketSize", UintegerValue (50)); // Petit paquet
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]")); // Dort 10s
          
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          app.Start (Seconds (startTime));
          app.Stop (Seconds (simulationTime));

      } else if (appType == 1) {
          // Type 1: Caméra IP (UDP, Gros débit, Constant)
          OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (serverAddress, port)));
          onoff.SetConstantRate (DataRate ("2Mbps")); 
          onoff.SetAttribute ("PacketSize", UintegerValue (1024));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]")); // Jamais Off
          
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          app.Start (Seconds (startTime));
          app.Stop (Seconds (simulationTime));

      } else if (appType == 2) {
          // Type 2: Web / HTTP (TCP, Bursty) 
          
          OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (serverAddress, port)));
          onoff.SetAttribute ("DataRate", StringValue ("5Mbps"));
          onoff.SetAttribute ("PacketSize", UintegerValue (1400));
          // Comportement Burst: Actif court (téléchargement page)
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ExponentialRandomVariable[Mean=2.0]")); 
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean=5.0]"));
          
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          app.Start (Seconds (startTime));
          app.Stop (Seconds (simulationTime));

      } else {
          // Type 3: VoIP (UDP, Paquets moyens, très régulier)
          OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (serverAddress, port)));
          onoff.SetConstantRate (DataRate ("64kbps")); // Codec G.711 standard
          onoff.SetAttribute ("PacketSize", UintegerValue (160));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]")); 
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
          
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          app.Start (Seconds (startTime));
          app.Stop (Seconds (simulationTime));
      }
  }

  // animation
  AnimationInterface anim ("maison_animation2.xml");
  
  anim.UpdateNodeDescription (wifiApNode.Get (0), "Gateway AP");
  for(uint32_t k=0; k<nServers; ++k){
       anim.UpdateNodeDescription (serverNodes.Get (k), "Server Type " + std::to_string(k));
  }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
