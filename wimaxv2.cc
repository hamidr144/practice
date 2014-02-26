#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wimax-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipcs-classifier-record.h"
#include "ns3/service-flow.h"
#include <iostream>
#include "ns3/global-route-manager.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/vector.h"



NS_LOG_COMPONENT_DEFINE ("Wimax");

using namespace ns3;

int main (int argc, char *argv[])
{


  double duration = 5.5;
  WimaxHelper::SchedulerType scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;


  //нужно для вывода логов
  //LogComponentPrintList();
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  //LogComponentEnable ("MobilityHelper", LOG_INFO);
  //LogComponentEnableAll(LOG_LEVEL_INFO);


  //создание узлов
  NodeContainer ssNodes;//абоненские узлы
  NodeContainer bsNodes;//базовый станции

  ssNodes.Create (1);
  bsNodes.Create (1);

  WimaxHelper wimax;

  NetDeviceContainer ssDevs, bsDevs;

  ssDevs = wimax.Install (ssNodes,
                          WimaxHelper::DEVICE_TYPE_SUBSCRIBER_STATION,
                          WimaxHelper::SIMPLE_PHY_TYPE_OFDM,
                          scheduler);
  bsDevs = wimax.Install (bsNodes, WimaxHelper::DEVICE_TYPE_BASE_STATION, 
                          WimaxHelper::SIMPLE_PHY_TYPE_OFDM, scheduler);



  Ptr<ConstantPositionMobilityModel> BSPosition;
  Ptr<RandomWaypointMobilityModel> SSPosition;
  Ptr<RandomRectanglePositionAllocator> SSPosAllocator;

  //задаем позицию базы
  BSPosition = CreateObject<ConstantPositionMobilityModel> ();

  BSPosition->SetPosition (Vector (0, 0, 0));
  bsNodes.Get (0)->AggregateObject (BSPosition);
  bsDevs.Add (bsDevs);

  //задаем область передвижения мобильной станции
  SSPosition = CreateObject<RandomWaypointMobilityModel> ();
  SSPosAllocator = CreateObject<RandomRectanglePositionAllocator> ();
  Ptr<UniformRandomVariable> xVar = CreateObject<UniformRandomVariable> ();
  xVar->SetAttribute ("Min", DoubleValue (0));
  xVar->SetAttribute ("Max", DoubleValue (1000));
  SSPosAllocator->SetX (xVar);
  Ptr<UniformRandomVariable> yVar = CreateObject<UniformRandomVariable> ();
  yVar->SetAttribute ("Min", DoubleValue (0));
  yVar->SetAttribute ("Max", DoubleValue (0));
  SSPosAllocator->SetY (yVar);
  SSPosition->SetAttribute ("PositionAllocator", PointerValue (SSPosAllocator));
  SSPosition->SetAttribute ("Speed", StringValue ("ns3::ConstantRandomVariable[Constant=125]"));
  SSPosition->SetAttribute ("Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.01]"));


  ssNodes.Get (0)->AggregateObject (SSPosition);



  Ptr<SubscriberStationNetDevice> ss;


  ss = ssDevs.Get (0)->GetObject<SubscriberStationNetDevice> ();
  ss->SetModulationType (WimaxPhy::MODULATION_TYPE_QAM16_12);
    

  Ptr<BaseStationNetDevice> bs;

  bs = bsDevs.Get (0)->GetObject<BaseStationNetDevice> ();

  InternetStackHelper stack;
  stack.Install (bsNodes);
  stack.Install (ssNodes);

  MobilityHelper mobility;
  mobility.Install (bsNodes); 
  mobility.Install (bsNodes);


  //настройка адрессов
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer SSinterfaces = address.Assign (ssDevs);
  Ipv4InterfaceContainer BSinterface = address.Assign (bsDevs);



  // настройка приложений на сервере
  UdpServerHelper udpServer;
  ApplicationContainer serverApps;
  UdpClientHelper udpClient;
  ApplicationContainer clientApps;

  udpServer = UdpServerHelper (100);

  serverApps = udpServer.Install (bsNodes.Get (0));
  serverApps.Start (Seconds (1));
  serverApps.Stop (Seconds (duration));

  udpClient = UdpClientHelper (BSinterface.GetAddress (0), 100);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (3));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));

  clientApps = udpClient.Install (ssNodes.Get (0));
  clientApps.Start (Seconds (3));
  clientApps.Stop (Seconds (duration));


  UdpEchoServerHelper echoServer (99);

  ApplicationContainer serverApps1 = echoServer.Install (bsNodes.Get (0));
  serverApps1.Start (Seconds (1.0));
  serverApps1.Stop (Seconds (duration));

  UdpEchoClientHelper echoClient (BSinterface.GetAddress (0), 99);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps1 = echoClient.Install (ssNodes.Get (0));
  clientApps1.Start (Seconds (2.0));
  clientApps1.Stop (Seconds (duration));




  Simulator::Stop (Seconds (duration));

  wimax.EnablePcap ("wimax-ss0", ssNodes.Get (0)->GetId (), ss->GetIfIndex ());
  wimax.EnablePcap ("wimax-bs0", bsNodes.Get (0)->GetId (), bs->GetIfIndex ());

  IpcsClassifierRecord DlClassifierUgs (BSinterface.GetAddress(0),
                                        Ipv4Mask ("255.255.255.255"),
                                        SSinterfaces.GetAddress (0),
                                        Ipv4Mask ("255.255.255.255"),
                                        101,
                                        65000,
                                        0,
                                        100,
                                        17,
                                        1);
  ServiceFlow DlServiceFlowUgs = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_DOWN,
                                                          ServiceFlow::SF_TYPE_RTPS,
                                                          DlClassifierUgs);

  ss->AddServiceFlow (DlServiceFlowUgs);

  //проверка начальной позиции мобильной станции
  Ptr<MobilityModel> mob = ssNodes.Get(0)->GetObject<MobilityModel>();
  Vector pos = mob->GetPosition ();
  std::cout << "POS: x=" << pos.x << ", y=" << pos.y << std::endl;

  
  Simulator::Run ();
  //вывод конечной точки мобтльной точки
  mob = ssNodes.Get(0)->GetObject<MobilityModel>();
  pos = mob->GetPosition ();
  std::cout << "POS: x=" << pos.x << ", y=" << pos.y << std::endl;

  Simulator::Destroy ();


  return 0;
}