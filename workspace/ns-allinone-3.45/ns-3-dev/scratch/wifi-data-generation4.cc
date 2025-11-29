//Simulation Réseau Domestique, Collecte de données (PCAP)

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiDataGeneration");

int main (int argc, char *argv[])
{
  // Simulaton parameters
  uint32_t nWifi = 25;
  uint32_t nServers = 4;
  double simulationTime = 60.0;
  bool verbose = true;
  bool enablePcap = true; // activate PCAP

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nWifi", "Nombre de stations wifi", nWifi);
  cmd.AddValue ("simulationTime", "Duree de la simulation", simulationTime);
  cmd.AddValue ("enablePcap", "Activer la capture de paquets", enablePcap);
  cmd.Parse (argc, argv);

  // My nodes
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);

  NodeContainer wifiApNode;
  wifiApNode.Create (1);

  NodeContainer serverNodes;
  serverNodes.Create (nServers);

  // Réseau Filaire
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1Gbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NodeContainer wiredNodes;
  wiredNodes.Add (wifiApNode.Get (0));
  wiredNodes.Add (serverNodes);

  NetDeviceContainer wiredDevices = csma.Install (wiredNodes);

  // Wifi devices
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());
  
  // Variabilité : Distance et signal
  channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  channel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");

  // activate PCAP
  if (enablePcap) {
      phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO); 
  }

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
  fixedAlloc->Add (Vector (50.0, 50.0, 0.0)); 
  for (uint32_t i = 0; i < nServers; i++) {
      fixedAlloc->Add (Vector (80.0, 30.0 + (i * 10.0), 0.0)); 
  }
  mobility.SetPositionAllocator (fixedAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  mobility.Install (serverNodes);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (20.0), "MinY", DoubleValue (10.0),
                                 "DeltaX", DoubleValue (0.0), "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (1), "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  // Internet
  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);
  stack.Install (serverNodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wiredInterfaces = address.Assign (wiredDevices);

  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (apDevices);
  Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Applications
  Ptr<UniformRandomVariable> typeRng = CreateObject<UniformRandomVariable> ();
  typeRng->SetAttribute ("Min", DoubleValue (0.0));
  typeRng->SetAttribute ("Max", DoubleValue (3.99));

  Ptr<UniformRandomVariable> startRng = CreateObject<UniformRandomVariable> ();
  startRng->SetAttribute ("Min", DoubleValue (0.0));
  startRng->SetAttribute ("Max", DoubleValue (5.0));
  
  Ptr<UniformRandomVariable> varRng = CreateObject<UniformRandomVariable>();

  uint16_t portBase = 9000;

  // Serveurs
  for (uint32_t k = 0; k < nServers; k++) {
      Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), portBase + k));
      PacketSinkHelper packetSinkHelperUDP ("ns3::UdpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkAppsUDP = packetSinkHelperUDP.Install (serverNodes.Get(k));
      sinkAppsUDP.Start (Seconds (0.0));
      sinkAppsUDP.Stop (Seconds (simulationTime));

      PacketSinkHelper packetSinkHelperTCP ("ns3::TcpSocketFactory", sinkLocalAddress);
      ApplicationContainer sinkAppsTCP = packetSinkHelperTCP.Install (serverNodes.Get(k));
      sinkAppsTCP.Start (Seconds (0.0));
      sinkAppsTCP.Stop (Seconds (simulationTime));
  }


  // Clients
  for (uint32_t i = 0; i < nWifi; i++) {
      int appType = (int)typeRng->GetValue ();
      double startTime = startRng->GetValue ();
      Ipv4Address serverAddress = wiredInterfaces.GetAddress (appType + 1); 
      uint16_t port = portBase + appType;

      if (verbose) {
          std::cout << "Node " << i << " is Type " << appType << " -> Server " << appType << std::endl;
      }

      if (appType == 0) { // IoT
      
          uint32_t pktSize = 40 + varRng->GetInteger(0, 40);
          
          OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (serverAddress, port)));
          onoff.SetConstantRate (DataRate ("1kbps"));
          onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"));
          
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          app.Start (Seconds (startTime));
          app.Stop (Seconds (simulationTime));
          
      } else if (appType == 1) { // Camera
      
          uint32_t pktSize = 1000 + varRng->GetInteger(0, 400);
          uint32_t dataRate = 1500000 + varRng->GetInteger(0, 1000000);
          
          OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (serverAddress, port)));
          onoff.SetConstantRate (DataRate (dataRate));
          onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          app.Start (Seconds (startTime));
          app.Stop (Seconds (simulationTime));
      } else if (appType == 2) { // Web
          uint32_t pktSize = 500 + varRng->GetInteger(0, 960);
          
          OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (serverAddress, port)));
          onoff.SetAttribute ("DataRate", StringValue ("5Mbps"));
          onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::LogNormalRandomVariable[Mu=0.0|Sigma=1.0]")); 
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ExponentialRandomVariable[Mean=5.0]"));
          
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          app.Start (Seconds (startTime));
          app.Stop (Seconds (simulationTime));
      } else { // VoIP
          uint32_t pktSize = 160 + varRng->GetValue(0, 40);
          
          OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (serverAddress, port)));
          onoff.SetConstantRate (DataRate ("64kbps"));
          onoff.SetAttribute ("PacketSize", UintegerValue (pktSize));
          onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]")); 
          onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
          
          ApplicationContainer app = onoff.Install (wifiStaNodes.Get (i));
          app.Start (Seconds (startTime));
          app.Stop (Seconds (simulationTime));
      }
  }

  // Capture PCAP
  if (enablePcap) {
      
      phy.EnablePcap ("wifi-traces", apDevices.Get (0), true);
  }

  // Global statistics with FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // animation
  AnimationInterface anim ("maison_animation4.xml");
  
  // Augmenter la limite de paquets pour NetAnim
  anim.SetMaxPktsPerTraceFile(1000000); 

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  
  // Stats FlowMonitor
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  monitor->SerializeToXmlFile("lab-results_final.xml", true, true);

  Simulator::Destroy ();

  return 0;
}
