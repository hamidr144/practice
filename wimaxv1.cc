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

NS_LOG_COMPONENT_DEFINE ("WimaxSimpleExample");

using namespace ns3;

int main (int argc, char *argv[])
{


  int duration = 7; //schedType = 0;
  WimaxHelper::SchedulerType scheduler = WimaxHelper::SCHED_TYPE_SIMPLE;


  //нужно для вывода логов
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  //создание узлов
  NodeContainer ssNodes;//абоненские узлы
  NodeContainer bsNodes;//базовый станции

  ssNodes.Create (1);//ssNodes.Create (1);
  bsNodes.Create (1);

  WimaxHelper wimax;

  NetDeviceContainer ssDevs, bsDevs;

  ssDevs = wimax.Install (ssNodes,
                          WimaxHelper::DEVICE_TYPE_SUBSCRIBER_STATION,
                          WimaxHelper::SIMPLE_PHY_TYPE_OFDM,
                          scheduler);
  bsDevs = wimax.Install (bsNodes, WimaxHelper::DEVICE_TYPE_BASE_STATION, 
                          WimaxHelper::SIMPLE_PHY_TYPE_OFDM, scheduler);



  Ptr<SubscriberStationNetDevice> ss;


      ss = ssDevs.Get (0)->GetObject<SubscriberStationNetDevice> ();
      ss->SetModulationType (WimaxPhy::MODULATION_TYPE_QAM16_12);
    

  Ptr<BaseStationNetDevice> bs;

  bs = bsDevs.Get (0)->GetObject<BaseStationNetDevice> ();

  InternetStackHelper stack;
  stack.Install (bsNodes);
  stack.Install (ssNodes);

  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer SSinterfaces = address.Assign (ssDevs);
  Ipv4InterfaceContainer BSinterface = address.Assign (bsDevs);

  UdpServerHelper udpServer;
  ApplicationContainer serverApps;
  UdpClientHelper udpClient;
  ApplicationContainer clientApps;

  udpServer = UdpServerHelper (100);

  serverApps = udpServer.Install (bsNodes.Get (0));
  serverApps.Start (Seconds (2));
  serverApps.Stop (Seconds (duration));

  udpClient = UdpClientHelper (BSinterface.GetAddress (0), 100);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (3));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));

  clientApps = udpClient.Install (ssNodes.Get (0));
  clientApps.Start (Seconds (4));
  clientApps.Stop (Seconds (duration));

  Simulator::Stop (Seconds (duration + 0.1));
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  wimax.EnablePcap ("wimax-simple-ss0", ssNodes.Get (0)->GetId (), ss->GetIfIndex ());
  wimax.EnablePcap ("wimax-simple-bs0", bsNodes.Get (0)->GetId (), bs->GetIfIndex ());

  IpcsClassifierRecord DlClassifierUgs (BSinterface.GetAddress(0),
                                        Ipv4Mask ("255.255.255.255"),
                                        SSinterfaces.GetAddress (0),
                                        Ipv4Mask ("255.255.255.255"),
                                        0,
                                        65000,
                                        100,
                                        100,
                                        17,
                                        1);
  ServiceFlow DlServiceFlowUgs = wimax.CreateServiceFlow (ServiceFlow::SF_DIRECTION_DOWN,
                                                          ServiceFlow::SF_TYPE_RTPS,
                                                          DlClassifierUgs);



  ss->AddServiceFlow (DlServiceFlowUgs);

  NS_LOG_INFO ("Starting simulation.....");
  Simulator::Run ();

  ss = 0;
  bs = 0;

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  return 0;
}
