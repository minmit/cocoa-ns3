/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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
 */

#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/mac48-address.h"
#include "ns3/llc-snap-header.h"
#include "ns3/error-model.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/net-device-queue-interface.h"
#include "point-to-point-net-device.h"
#include "point-to-point-channel.h"
#include "ppp-header.h"

#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PointToPointNetDevice");

NS_OBJECT_ENSURE_REGISTERED (PointToPointNetDevice);

TypeId 
PointToPointNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PointToPointNetDevice")
    .SetParent<NetDevice> ()
    .SetGroupName ("PointToPoint")
    .AddConstructor<PointToPointNetDevice> ()
    .AddAttribute ("Mtu", "The MAC-level Maximum Transmission Unit",
                   UintegerValue (DEFAULT_MTU),
                   MakeUintegerAccessor (&PointToPointNetDevice::SetMtu,
                                         &PointToPointNetDevice::GetMtu),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("Address", 
                   "The MAC address of this device.",
                   Mac48AddressValue (Mac48Address ("ff:ff:ff:ff:ff:ff")),
                   MakeMac48AddressAccessor (&PointToPointNetDevice::m_address),
                   MakeMac48AddressChecker ())
    .AddAttribute ("DataRate", 
                   "The default data rate for point to point links",
                   DataRateValue (DataRate ("32768b/s")),
                   MakeDataRateAccessor (&PointToPointNetDevice::m_bps),
                   MakeDataRateChecker ())
    .AddAttribute ("ReceiveErrorModel", 
                   "The receiver error model used to simulate packet loss",
                   PointerValue (),
                   MakePointerAccessor (&PointToPointNetDevice::m_receiveErrorModel),
                   MakePointerChecker<ErrorModel> ())
    .AddAttribute ("InterframeGap", 
                   "The time to wait between packet (frame) transmissions",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&PointToPointNetDevice::m_tInterframeGap),
                   MakeTimeChecker ())

    //COCOA
    .AddAttribute ("CCLatency",
                   "The latency of the control loop",
                   UintegerValue(0),
                   MakeUintegerAccessor(&PointToPointNetDevice::SetCCLatency,
                                        &PointToPointNetDevice::GetCCLatency),
                  MakeUintegerChecker<uint16_t>())
    //
    // Transmit queueing discipline for the device which includes its own set
    // of trace hooks.
    //
    .AddAttribute ("TxQueue", 
                   "A queue to use as the transmit queue in the device.",
                   PointerValue (),
                   MakePointerAccessor (&PointToPointNetDevice::m_queue),
                   MakePointerChecker<Queue<Packet> > ())

    //
    // Trace sources at the "top" of the net device, where packets transition
    // to/from higher layers.
    //
    .AddTraceSource ("MacTx", 
                     "Trace source indicating a packet has arrived "
                     "for transmission by this device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macTxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacTxDrop", 
                     "Trace source indicating a packet has been dropped "
                     "by the device before transmission",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macTxDropTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacPromiscRx", 
                     "A packet has been received by this device, "
                     "has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  "
                     "This is a promiscuous trace,",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macPromiscRxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("MacRx", 
                     "A packet has been received by this device, "
                     "has been passed up from the physical layer "
                     "and is being forwarded up the local protocol stack.  "
                     "This is a non-promiscuous trace,",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macRxTrace),
                     "ns3::Packet::TracedCallback")
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("MacRxDrop", 
                     "Trace source indicating a packet was dropped "
                     "before being forwarded up the stack",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_macRxDropTrace),
                     "ns3::Packet::TracedCallback")
#endif
    //
    // Trace souces at the "bottom" of the net device, where packets transition
    // to/from the channel.
    //
    .AddTraceSource ("PhyTxBegin", 
                     "Trace source indicating a packet has begun "
                     "transmitting over the channel",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxBeginTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyTxEnd", 
                     "Trace source indicating a packet has been "
                     "completely transmitted over the channel",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxEndTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyTxDrop", 
                     "Trace source indicating a packet has been "
                     "dropped by the device during transmission",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyTxDropTrace),
                     "ns3::Packet::TracedCallback")
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("PhyRxBegin", 
                     "Trace source indicating a packet has begun "
                     "being received by the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxBeginTrace),
                     "ns3::Packet::TracedCallback")
#endif
    .AddTraceSource ("PhyRxEnd", 
                     "Trace source indicating a packet has been "
                     "completely received by the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxEndTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PhyRxDrop", 
                     "Trace source indicating a packet has been "
                     "dropped by the device during reception",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_phyRxDropTrace),
                     "ns3::Packet::TracedCallback")

    //
    // Trace sources designed to simulate a packet sniffer facility (tcpdump).
    // Note that there is really no difference between promiscuous and 
    // non-promiscuous traces in a point-to-point link.
    //
    .AddTraceSource ("Sniffer", 
                    "Trace source simulating a non-promiscuous packet sniffer "
                     "attached to the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_snifferTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("PromiscSniffer", 
                     "Trace source simulating a promiscuous packet sniffer "
                     "attached to the device",
                     MakeTraceSourceAccessor (&PointToPointNetDevice::m_promiscSnifferTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


PointToPointNetDevice::PointToPointNetDevice () 
  :
    m_txMachineState (READY),
    m_channel (0),
    m_linkUp (false),
    m_currentPkt (0)
{
  NS_LOG_FUNCTION (this);
}

PointToPointNetDevice::~PointToPointNetDevice ()
{
  NS_LOG_FUNCTION (this);
}

void
PointToPointNetDevice::AddHeader (Ptr<Packet> p, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << p << protocolNumber);
  PppHeader ppp;
  ppp.SetProtocol (EtherToPpp (protocolNumber));
  p->AddHeader (ppp);
}

bool
PointToPointNetDevice::ProcessHeader (Ptr<Packet> p, uint16_t& param)
{
  NS_LOG_FUNCTION (this << p << param);
  PppHeader ppp;
  p->RemoveHeader (ppp);
  param = PppToEther (ppp.GetProtocol ());
  return true;
}

void
PointToPointNetDevice::DoInitialize (void)
{
  if (m_queueInterface)
    {
      NS_ASSERT_MSG (m_queue != 0, "A Queue object has not been attached to the device");

      // connect the traced callbacks of m_queue to the static methods provided by
      // the NetDeviceQueue class to support flow control and dynamic queue limits.
      // This could not be done in NotifyNewAggregate because at that time we are
      // not guaranteed that a queue has been attached to the netdevice
      m_queueInterface->ConnectQueueTraces (m_queue, 0);
    }

  NetDevice::DoInitialize ();
}

void
PointToPointNetDevice::NotifyNewAggregate (void)
{
  NS_LOG_FUNCTION (this);
  if (m_queueInterface == 0)
    {
      Ptr<NetDeviceQueueInterface> ndqi = this->GetObject<NetDeviceQueueInterface> ();
      //verify that it's a valid netdevice queue interface and that
      //the netdevice queue interface was not set before
      if (ndqi != 0)
        {
          m_queueInterface = ndqi;
        }
    }
  NetDevice::NotifyNewAggregate ();
}

void
PointToPointNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_node = 0;
  m_channel = 0;
  m_receiveErrorModel = 0;
  m_currentPkt = 0;
  m_queue = 0;
  m_queueInterface = 0;
  NetDevice::DoDispose ();
}

void
PointToPointNetDevice::SetDataRate (DataRate bps)
{
  NS_LOG_FUNCTION (this);
  m_bps = bps;
}

void
PointToPointNetDevice::SetInterframeGap (Time t)
{
  NS_LOG_FUNCTION (this << t.GetSeconds ());
  m_tInterframeGap = t;
}

bool
PointToPointNetDevice::TransmitStart (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  //
  // This function is called to start the process of transmitting a packet.
  // We need to tell the channel that we've started wiggling the wire and
  // schedule an event that will be executed when the transmission is complete.
  //
  NS_ASSERT_MSG (m_txMachineState == READY, "Must be READY to transmit");
  m_txMachineState = BUSY;
  m_currentPkt = p;
  m_phyTxBeginTrace (m_currentPkt);

  Time txTime = m_bps.CalculateBytesTxTime (p->GetSize ());
  Time txCompleteTime = txTime + m_tInterframeGap;

  NS_LOG_LOGIC ("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds () << "sec");
  Simulator::Schedule (txCompleteTime, &PointToPointNetDevice::TransmitComplete, this);

  bool result = m_channel->TransmitStart (p, this, txTime);
  if (result == false)
    {
      m_phyTxDropTrace (p);
    }
  return result;
}

void
PointToPointNetDevice::TransmitComplete (void)
{
  NS_LOG_FUNCTION (this);

  //
  // This function is called to when we're all done transmitting a packet.
  // We try and pull another packet off of the transmit queue.  If the queue
  // is empty, we are done, otherwise we need to start transmitting the
  // next packet.
  //
  NS_ASSERT_MSG (m_txMachineState == BUSY, "Must be BUSY if transmitting");
  m_txMachineState = READY;

  NS_ASSERT_MSG (m_currentPkt != 0, "PointToPointNetDevice::TransmitComplete(): m_currentPkt zero");

  m_phyTxEndTrace (m_currentPkt);

  // CoCoA Start
  if (GetNode()->GetId() > 1){

    PppHeader phdr;
    m_currentPkt->RemoveHeader(phdr);
    Ipv4Header ipv4;
    m_currentPkt->RemoveHeader(ipv4);
    TcpHeader tcp;
    m_currentPkt->PeekHeader(tcp);
    m_currentPkt->AddHeader(ipv4);
    m_currentPkt->AddHeader(phdr);

    typedef std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, uint8_t> fid_t;
      
    // Compute FID. Assuming that all packets going out are from
    // IP address on this machine, we can avoid sorting the addresses
    // and always put the local side first and the remote side second
    // something to change later (TODO).
    fid_t fid(std::make_tuple(ipv4.GetSource(), tcp.GetSourcePort(),
                              ipv4.GetDestination(), tcp.GetDestinationPort(),
                              ipv4.GetProtocol()));
    

    std::map<fid_t, FlowState>::iterator fit = flow_info.find(fid);
    // Check for sender-size info Payload if Data or SYN or FIN
    if (fit != flow_info.end()){
      FlowState& st = fit->second;
      CoCoAEventHandler(m_currentPkt, ipv4, tcp, st, PKT_SENT);
    }
    else{
      NS_LOG_DEBUG(GetNode()->GetId() << " ERRRRRRRRRRRRRRRRR1!");
    }
  }
  else{
    PppHeader phdr;
    m_currentPkt->RemoveHeader(phdr);
    Ipv4Header ipv4;
    m_currentPkt->RemoveHeader(ipv4);
    TcpHeader tcp;
    m_currentPkt->PeekHeader(tcp);
    m_currentPkt->AddHeader(ipv4);
    m_currentPkt->AddHeader(phdr);

    NS_LOG_DEBUG(GetNode()->GetId() << " SEND| PKT SENT: " << five_tuple_str(ipv4, tcp, false));
  }
  // CoCoA End
  
  m_currentPkt = 0;

  Ptr<Packet> p = m_queue->Dequeue ();
  if (p == 0)
    {
      NS_LOG_LOGIC ("No pending packets in device queue after tx complete");
      return;
    }

  //
  // Got another packet off of the queue, so start the transmit process again.
  //
  m_snifferTrace (p);
  m_promiscSnifferTrace (p);
  TransmitStart (p);
}

bool
PointToPointNetDevice::Attach (Ptr<PointToPointChannel> ch)
{
  NS_LOG_FUNCTION (this << &ch);

  m_channel = ch;

  m_channel->Attach (this);

  //
  // This device is up whenever it is attached to a channel.  A better plan
  // would be to have the link come up when both devices are attached, but this
  // is not done for now.
  //
  NotifyLinkUp ();
  return true;
}

void
PointToPointNetDevice::SetQueue (Ptr<Queue<Packet> > q)
{
  NS_LOG_FUNCTION (this << q);
  m_queue = q;
}

void
PointToPointNetDevice::SetReceiveErrorModel (Ptr<ErrorModel> em)
{
  NS_LOG_FUNCTION (this << em);
  m_receiveErrorModel = em;
}

void
PointToPointNetDevice::Receive (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (this << packet);
  uint16_t protocol = 0;

  if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt (packet) ) 
    {
      // 
      // If we have an error model and it indicates that it is time to lose a
      // corrupted packet, don't forward this packet up, let it go.
      //
      m_phyRxDropTrace (packet);
    }
  else 
    {
      // 
      // Hit the trace hooks.  All of these hooks are in the same place in this 
      // device because it is so simple, but this is not usually the case in
      // more complicated devices.
      //
      m_snifferTrace (packet);
      m_promiscSnifferTrace (packet);
      m_phyRxEndTrace (packet);

      //
      // Trace sinks will expect complete packets, not packets without some of the
      // headers.
      //
      Ptr<Packet> originalPacket = packet->Copy ();

      //
      // Strip off the point-to-point protocol header and forward this packet
      // up the protocol stack.  Since this is a simple point-to-point link,
      // there is no difference in what the promisc callback sees and what the
      // normal receive callback sees.
      //
      ProcessHeader (packet, protocol);

      // CoCoA Start
      Ipv4Header ipv4;
      packet->RemoveHeader(ipv4);
      TcpHeader tcp;
      packet->PeekHeader(tcp);
      packet->AddHeader(ipv4);
      
      if (GetNode()->GetId() > 1){
        typedef std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, uint8_t> fid_t;
        
        // Compute FID. Assuming that all packets going out are from
        // IP address on this machine, we can avoid sorting the addresses
        // and always put the local side first and the remote side second
        // something to change later (TODO).
        fid_t fid(std::make_tuple(ipv4.GetDestination(), tcp.GetDestinationPort(),
                                  ipv4.GetSource(), tcp.GetSourcePort(),
                                  ipv4.GetProtocol()));
        

        // Check if new flow
        std::map<fid_t, FlowState>::iterator fit = flow_info.find(fid);
        if (fit == flow_info.end()){
          NS_LOG_DEBUG(GetNode()->GetId() << " RECEIVE| NEW FLOW: " << five_tuple_str(ipv4, tcp, true));
          FlowState s;
          RenoInit(s);
          flow_info[fid] = s;
          fit = flow_info.find(fid);
        }

        FlowState& st = fit->second;
        switch(st.state){
          case SETUP:{
            switch(st.setup_state){
              case NONE:{
                uint8_t flags = tcp.GetFlags();
                if ((flags & TcpHeader::SYN) > 0 &&
                    (flags & !(TcpHeader::SYN)) == 0){
                  NS_LOG_DEBUG(GetNode()->GetId() << " RECEIVE| SYN: " << five_tuple_str(ipv4, tcp, true));
                  st.setup_state = SYN;
                  st.initiator = false;
                }
                break;
              }
              case SYN:{
                uint8_t flags = tcp.GetFlags();
                if (st.initiator &&
                    (flags & TcpHeader::SYN) > 0 &&
                    (flags & TcpHeader::ACK) > 0 &&
                    (flags & !(TcpHeader::SYN) & !(TcpHeader::ACK)) == 0){
                  NS_LOG_DEBUG(GetNode()->GetId() << " RECEIVE| SYN ACK: " << five_tuple_str(ipv4, tcp, true));
                  st.setup_state = SYN_ACK;
                  CoCoAEventHandler(packet, ipv4, tcp, st, ACK_RCVD);

                }
                break;
              }
              case SYN_ACK:{
                uint8_t flags = tcp.GetFlags();
                if (!st.initiator &&
                    (flags & TcpHeader::ACK) > 0 &&
                    (flags & !(TcpHeader::ACK)) == 0){
                  NS_LOG_DEBUG(GetNode()->GetId() << " RECEIVE| HANDSHAKE ACK: " << five_tuple_str(ipv4, tcp, true));
                  st.setup_state = ACK;
                  st.state = DATA;
                  CoCoAEventHandler(packet, ipv4, tcp, st, ACK_RCVD);

                }
                break;
              }
              default:
                break;
            }
            break;
          }
          case DATA:{
            uint8_t flags = tcp.GetFlags();
            if ((flags & TcpHeader::ACK) > 0){
              CoCoAEventHandler(packet, ipv4, tcp, st, ACK_RCVD);
            }
            else{
              NS_LOG_DEBUG(GetNode()->GetId() << " RECEIVE| DATA: " << five_tuple_str(ipv4, tcp, true));
            }
            break;
          }
          case TEAR_DOWN:
            NS_LOG_DEBUG(GetNode()->GetId() << " RECEIVE| TEAR DOWN: " << five_tuple_str(ipv4, tcp, true));
            break;
        }
      }
      else{
        NS_LOG_DEBUG(GetNode()->GetId() << " RECEIVED | " << five_tuple_str(ipv4, tcp, false));
      }
      // CoCoA End
      if (!m_promiscCallback.IsNull ())
        {
          m_macPromiscRxTrace (originalPacket);
          m_promiscCallback (this, packet, protocol, GetRemote (), GetAddress (), NetDevice::PACKET_HOST);
        }

      m_macRxTrace (originalPacket);
      m_rxCallback (this, packet, protocol, GetRemote ());
    }
}

Ptr<Queue<Packet> >
PointToPointNetDevice::GetQueue (void) const
{ 
  NS_LOG_FUNCTION (this);
  return m_queue;
}

void
PointToPointNetDevice::NotifyLinkUp (void)
{
  NS_LOG_FUNCTION (this);
  m_linkUp = true;
  m_linkChangeCallbacks ();
}

void
PointToPointNetDevice::SetIfIndex (const uint32_t index)
{
  NS_LOG_FUNCTION (this);
  m_ifIndex = index;
}

uint32_t
PointToPointNetDevice::GetIfIndex (void) const
{
  return m_ifIndex;
}

Ptr<Channel>
PointToPointNetDevice::GetChannel (void) const
{
  return m_channel;
}


//
// This is a point-to-point device, so we really don't need any kind of address
// information.  However, the base class NetDevice wants us to define the
// methods to get and set the address.  Rather than be rude and assert, we let
// clients get and set the address, but simply ignore them.

void
PointToPointNetDevice::SetAddress (Address address)
{
  NS_LOG_FUNCTION (this << address);
  m_address = Mac48Address::ConvertFrom (address);
}

Address
PointToPointNetDevice::GetAddress (void) const
{
  return m_address;
}

bool
PointToPointNetDevice::IsLinkUp (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkUp;
}

void
PointToPointNetDevice::AddLinkChangeCallback (Callback<void> callback)
{
  NS_LOG_FUNCTION (this);
  m_linkChangeCallbacks.ConnectWithoutContext (callback);
}

//
// This is a point-to-point device, so every transmission is a broadcast to
// all of the devices on the network.
//
bool
PointToPointNetDevice::IsBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

//
// We don't really need any addressing information since this is a 
// point-to-point device.  The base class NetDevice wants us to return a
// broadcast address, so we make up something reasonable.
//
Address
PointToPointNetDevice::GetBroadcast (void) const
{
  NS_LOG_FUNCTION (this);
  return Mac48Address ("ff:ff:ff:ff:ff:ff");
}

bool
PointToPointNetDevice::IsMulticast (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

Address
PointToPointNetDevice::GetMulticast (Ipv4Address multicastGroup) const
{
  NS_LOG_FUNCTION (this);
  return Mac48Address ("01:00:5e:00:00:00");
}

Address
PointToPointNetDevice::GetMulticast (Ipv6Address addr) const
{
  NS_LOG_FUNCTION (this << addr);
  return Mac48Address ("33:33:00:00:00:00");
}

bool
PointToPointNetDevice::IsPointToPoint (void) const
{
  NS_LOG_FUNCTION (this);
  return true;
}

bool
PointToPointNetDevice::IsBridge (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

void PointToPointNetDevice::RenoControl(CCState s, std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, uint8_t> fid){
  typedef std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, uint8_t> fid_t;
  
  // Check if new flow
  std::map<fid_t, FlowState>::iterator fit = flow_info.find(fid);
  if (fit == flow_info.end()){
    NS_LOG_DEBUG(GetNode()->GetId() << " SEND| ERROR in control loop");
  }

  FlowState& st = fit->second;

  switch(s){
    case START:{
      st.cc_tmp_win = 1;
      st.cm_window_size = 1;
      st.cc_recovery_seq = st.max_sent__val;
      st.cc_ss_threshold /= 2;
      break;
    }
    case SLOW_START:{
      st.cc_tmp_win += 1;
      st.cm_window_size += 1;
      break;
    }
    case AI:{
      st.cm_window_size = st.cc_tmp_win;
      st.cm_window_size += 1 / st.cm_window_size;
      st.cc_tmp_win += 1 / st.cc_tmp_win;
      break;
    }
    case MD:{
      st.cc_recovery_seq = st.max_sent__val;
      st.cc_tmp_win = st.cm_window_size / 2;
      st.cc_ss_threshold = st.cc_tmp_win; 
      st.cm_window_size = st.cc_tmp_win + st.dup_acks__val;
      break;
    }
    case FR:{
      st.cm_window_size = st.cc_tmp_win + st.dup_acks__val;
      break;
    }
    case IDLE:{
    }
  }
  NS_LOG_DEBUG(GetNode()->GetId() << " RENO CONTROL| WINDOW SIZE: " << st.cm_window_size <<
                            " State: " << CCState_Names[st.cc_state]);
  if (!queues_empty){
    Simulator::ScheduleNow(&PointToPointNetDevice::CoCoASched, this);
  }
}

void PointToPointNetDevice::RenoInit(FlowState& st){
  st = {
    .state = SETUP,
    .setup_state = NONE,
    .cc_state = START,
    .cc_ss_threshold = 65536 * 2,
    .cm_window_size = 1,
    .max_ack__val = 0,
    .new_ack__val = 0,
    .new_ack__ack_num = 0,
    .dup_acks__val = 0,
    .dup_acks__first_ack = true,
    .max_sent__val = 0,
    .rtx_timeout__val = false,
    .rtx_timeout__timer__isset = false,
    .rtx_timeout__timeout_cnt = 0,
    .rtx_timeout__timer_delay = MilliSeconds(500),
  };

}

void PointToPointNetDevice::CoCoASched(){
  typedef std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, uint8_t> fid_t;
  std::map<fid_t, FlowState>::iterator it;
  int32_t qlen = -1;
  bool m_queue_full = false;
  uint32_t queues_occ;

  while ((int32_t)m_queue->GetNPackets() > qlen && !m_queue_full){
    qlen = m_queue->GetNPackets();
    queues_occ = 0;
    for (it = flow_info.begin(); it != flow_info.end(); it++){
      FlowState& st = it->second;
      queues_occ += st.queue.size();
      if (!st.queue.empty()){
        Ptr<Packet> p = st.queue.top().first;

        PppHeader phdr;
        p->RemoveHeader(phdr);
        Ipv4Header ipv4;
        p->RemoveHeader(ipv4);
        TcpHeader tcp;
        p->PeekHeader(tcp);
        p->AddHeader(ipv4);
        p->AddHeader(phdr);

        uint16_t data_size = ipv4.GetPayloadSize() - tcp.GetLength() * 4;
        uint32_t seq = tcp.GetSequenceNumber().GetValue();

        NS_LOG_DEBUG(GetNode()->GetId() << " CM " << seq << " " << data_size << " " << st.cm_start << " " << st.cm_window_size * MSS );
        if (seq < st.cm_start){
          
          while(!st.queue.empty() && seq < st.cm_start){
            st.queue.pop();
            queues_occ--;
            CoCoAEventHandler(p, ipv4, tcp, st, PKT_DEQ);
          
            p = st.queue.top().first;
            p->RemoveHeader(phdr);
            p->RemoveHeader(ipv4);
            p->PeekHeader(tcp);
            p->AddHeader(ipv4);
            p->AddHeader(phdr);

            seq = tcp.GetSequenceNumber().GetValue();
          }
        }
        else if ((seq >= st.cm_start) && 
            (seq + data_size <= st.cm_start + st.cm_window_size * MSS)){

          bool res = m_queue->Enqueue(p);
          if (res){
              st.queue.pop();
              queues_occ--;
              CoCoAEventHandler(p, ipv4, tcp, st, PKT_DEQ);
          }
          else{
            m_queue_full = true;
            break;
          }
        }
      }
    }
  }

  queues_empty = (queues_occ == 0);
  
  //
  // If the channel is ready for transition we send the packet right now
  // 
  if (m_txMachineState == READY && m_queue->GetNPackets() > 0){
    Ptr<Packet> packet = m_queue->Dequeue ();
    m_snifferTrace (packet);
    m_promiscSnifferTrace (packet);
    TransmitStart (packet);
  }
}

void PointToPointNetDevice::CoCoAEventHandler(Ptr<Packet> packet,
                                 const Ipv4Header& ipv4,
                                 const TcpHeader& tcp,
                                 FlowState& st,
                                 CCEvent ev){
  switch(ev){
    case PKT_ENQ:{
      // CM Code
      st.queue.push(std::make_pair(packet, tcp.GetSequenceNumber().GetValue()));
      NS_LOG_DEBUG(GetNode()->GetId() << " SEND| PKT ENQ: " << five_tuple_str(ipv4, tcp, false)
                                     << " - Queue Length: " << st.queue.size());
      // Event Code
      if (queues_empty){
        queues_empty = false;
        Simulator::ScheduleNow(&PointToPointNetDevice::CoCoASched, this); 
      }
      break;
    }
    case PKT_DEQ:{
      NS_LOG_DEBUG(GetNode()->GetId() << " SEND| PKT DEQ: " << five_tuple_str(ipv4, tcp, false)
                                     << " - Queue Length: " << st.queue.size());
      break;
    }
    case PKT_SENT:{
      NS_LOG_DEBUG(GetNode()->GetId() << " SEND| PKT SENT: " << five_tuple_str(ipv4, tcp, false));
      uint16_t data_size = ipv4.GetPayloadSize() - tcp.GetLength() * 4;
      uint32_t sent = tcp.GetSequenceNumber().GetValue() + data_size;
      if (sent > st.max_sent__val){
        st.max_sent__val = sent;
      }

      if (!st.rtx_timeout__timer__isset &&
          st.max_sent__val > st.max_ack__val){
        st.rtx_timeout__val = false;
        st.rtx_timeout__timer__isset = true;
        st.rtx_timeout__timeout_cnt++;
        Simulator::Schedule(st.rtx_timeout__timer_delay, 
            &PointToPointNetDevice::rtx_timeout__timeout, this, 
                                               ipv4, tcp, st.rtx_timeout__timeout_cnt);
        NS_LOG_DEBUG(GetNode()->GetId() << " RENO| TIME OUT " << st.rtx_timeout__timeout_cnt << " Scheduled for " 
                                       << five_tuple_str(ipv4, tcp, false, false));
      }

      break;
    }
    case ACK_RCVD:{
      NS_LOG_DEBUG(GetNode()->GetId() << " RECEIVE| ACK RCVD: " << five_tuple_str(ipv4, tcp, true));
      uint32_t ack = tcp.GetAckNumber().GetValue();
      if (ack > st.cm_start){
        st.cm_start = ack;
        NS_LOG_DEBUG(GetNode()->GetId() << " WINDOW | advancing to " << ack);
      }
      
      if (ack > st.max_ack__val){
        st.max_ack__val = ack;
      }
      
      if ((ack == st.max_ack__val) &&
          (st.max_ack__val > st.new_ack__ack_num)){
        st.new_ack__ack_num = ack;
        st.new_ack__val = true;
      }
      else{
        st.new_ack__val = false;
      }

      if (st.dup_acks__first_ack){
        st.dup_acks__first_ack = false;
        st.dup_acks__last_ack = ack;
      }
      else if (st.dup_acks__last_ack == ack){
        st.dup_acks__val += 1;
      }
      else{
        st.dup_acks__last_ack = ack;
        st.dup_acks__val = 0;
      }

      if (st.new_ack__val || st.dup_acks__val == 3){
        st.rtx_timeout__val = false;
        st.rtx_timeout__timer__isset = true;
        st.rtx_timeout__timeout_cnt++;
        Simulator::Schedule(st.rtx_timeout__timer_delay, &PointToPointNetDevice::rtx_timeout__timeout, 
                                                this, ipv4, tcp, st.rtx_timeout__timeout_cnt);
        NS_LOG_DEBUG(GetNode()->GetId() << " RENO| TIME OUT " << st.rtx_timeout__timeout_cnt << " Scheduled for " 
                                       << five_tuple_str(ipv4, tcp, true, false));
      }
      bool transitioned = true;
      // ASM
      switch(st.cc_state){
        case START:{
          if (st.new_ack__val == 1){
            st.cc_state = SLOW_START;
          }
          else transitioned = false;
          break;
        }
        case SLOW_START:{
          if(st.new_ack__val == 1){
            if(st.cm_window_size < st.cc_ss_threshold){
              st.cc_state = SLOW_START;
            }
            else{
              st.cc_state = AI;
            }
          }
          else if (st.dup_acks__val == 3){
            if (st.max_ack__val > st.cc_recovery_seq){
              st.cc_state = MD;
            }
            else{
              st.cc_state = IDLE;
            }
          }
          else transitioned = false;
          break;
        }
        case AI:{
          if (st.new_ack__val == 1){
            st.cc_state = AI;
          }
          else if (st.dup_acks__val == 3){
            if (st.max_ack__val > st.cc_recovery_seq){
              st.cc_state = MD;
            }
            else{
              st.cc_state = IDLE;
            }
          }
          else transitioned = false;
          break;
        }
        case MD:{
          if (st.new_ack__val == 1){
            st.cc_state = AI;
          }
          else if (st.dup_acks__val > 0){
            st.cc_state = FR;
          }
          else transitioned = false;
          break;
        }
        case FR:{
          if (st.new_ack__val == 1){
            st.cc_state = AI;
          }
          else if (st.dup_acks__val > 0){
            st.cc_state = FR;
          }
          else transitioned = false;
          break;
        }
        case IDLE:{
          if(st.new_ack__val == 1){
            if(st.cm_window_size < st.cc_ss_threshold){
              st.cc_state = SLOW_START;
            }
            else{
              st.cc_state = AI;
            }
          }
          else transitioned = false;
          break;
        }
      }
      if (transitioned){
        typedef std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, uint8_t> fid_t;
        
        // Compute FID. Assuming that all packets going out are from
        // IP address on this machine, we can avoid sorting the addresses
        // and always put the local side first and the remote side second
        // something to change later (TODO).
        fid_t fid(std::make_tuple(ipv4.GetDestination(), tcp.GetDestinationPort(),
                                  ipv4.GetSource(), tcp.GetSourcePort(),
                                  ipv4.GetProtocol()));
        if (CC_LATENCY>0){
          Simulator::Schedule(MicroSeconds(CC_LATENCY), &PointToPointNetDevice::RenoControl, this, st.cc_state, fid);
        }
        else{
          RenoControl(st.cc_state, fid);
        }
      }
      break;
    }
  }
  /*NS_LOG_DEBUG("max ack " << st.max_ack__val <<
               " new_ack " << st.new_ack__val <<
               " dup_ack " << st.dup_acks__val <<
               " max_sent " << st.max_sent__val); */
}

std::string PointToPointNetDevice::five_tuple_str(const Ipv4Header& ipv4, 
                                                  const TcpHeader& tcp,
                                                  bool flip,
                                                  bool payload_size){
  std::ostringstream res;
  res << ipv4.GetIdentification() << " " << tcp.GetSequenceNumber().GetValue() << " " << tcp.GetAckNumber().GetValue() << " " ;
  if (flip){
    res << "(" << ipv4.GetDestination() << " " 
        << tcp.GetDestinationPort() << " " << ipv4.GetSource() 
        << " " << tcp.GetSourcePort() << " " << (int)ipv4.GetProtocol() << ")";
  }
  else{
    res << "(" << ipv4.GetSource() << " " 
        << tcp.GetSourcePort() << " " << ipv4.GetDestination() 
        << " " << tcp.GetDestinationPort() << " " << (int)ipv4.GetProtocol() << ")";
  } 
  if (payload_size){
    uint16_t data_size = ipv4.GetPayloadSize() - tcp.GetLength() * 4;
    res << " " << data_size << " Bytes"; 
  }
  return res.str();
}

void PointToPointNetDevice::rtx_timeout__timeout(Ipv4Header ipv4, TcpHeader tcp, uint32_t cnt){
  typedef std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, uint8_t> fid_t;
  //TODO: FIXME
  fid_t fid1(std::make_tuple(ipv4.GetSource(), tcp.GetSourcePort(),
                              ipv4.GetDestination(), tcp.GetDestinationPort(),
                              ipv4.GetProtocol()));
 
  fid_t fid2(std::make_tuple(ipv4.GetDestination(), tcp.GetDestinationPort(),
                              ipv4.GetSource(), tcp.GetSourcePort(),
                              ipv4.GetProtocol()));
  std::map<fid_t, FlowState>::iterator fit = flow_info.find(fid1);
  if (fit != flow_info.end()){
    FlowState& st = fit->second;
    if (st.rtx_timeout__timeout_cnt == cnt){
      NS_LOG_DEBUG(GetNode()->GetId() << " TIMEOUT| ID: " << cnt);
      bool prev_val = st.rtx_timeout__val;
      st.rtx_timeout__timer__isset = false;
      st.rtx_timeout__val = true;
      if (!prev_val){
        switch (st.cc_state){
          case SLOW_START:
          case AI:
          case MD:
          case FR:
          case IDLE:
            st.cc_state = START;
            if (CC_LATENCY > 0){
              Simulator::Schedule(MicroSeconds(CC_LATENCY), &PointToPointNetDevice::RenoControl, this, st.cc_state, fid1);
            }
            else{
              RenoControl(st.cc_state, fid1);
            }
            break;
          default:
            break;
        }
      }
    }
    return;
  }
  fit = flow_info.find(fid2);
  if (fit != flow_info.end()){
    FlowState& st = fit->second;
    if (st.rtx_timeout__timeout_cnt == cnt){
      NS_LOG_DEBUG(GetNode()->GetId() << " TIMEOUT| ID: " << cnt);
      bool prev_val = st.rtx_timeout__val;
      st.rtx_timeout__timer__isset = false;
      st.rtx_timeout__val = true;
      if (!prev_val){
        switch (st.cc_state){
          case SLOW_START:
          case AI:
          case MD:
          case FR:
          case IDLE:
            st.cc_state = START;
            if (CC_LATENCY > 0){
              Simulator::Schedule(MicroSeconds(CC_LATENCY), &PointToPointNetDevice::RenoControl, this, st.cc_state, fid2);
            }
            else{
              RenoControl(st.cc_state, fid2);
            }
            break;
          default:
            break;
        }
      }
    }
  }

}

bool
PointToPointNetDevice::Send (
  Ptr<Packet> packet, 
  const Address &dest, 
  uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << packet << dest << protocolNumber);
  NS_LOG_LOGIC ("p=" << packet << ", dest=" << &dest);
  NS_LOG_LOGIC ("UID is " << packet->GetUid ());

  // If IsLinkUp() is false it means there is no channel to send any packet 
  // over so we just hit the drop trace on the packet and return an error.
  //
  if (IsLinkUp () == false)
    {
      m_macTxDropTrace (packet);
      return false;
    }

  
  
  // COCOA Start
  //
 
  Ipv4Header ipv4;
  packet->RemoveHeader(ipv4);
  TcpHeader tcp;
  packet->PeekHeader(tcp);
  packet->AddHeader(ipv4);

  //
  // Stick a point to point protocol header on the packet in preparation for
  // shoving it out the door.
  //
  AddHeader (packet, protocolNumber);

  m_macTxTrace (packet);
  
  if (GetNode()->GetId() > 1){
    typedef std::tuple<Ipv4Address, uint16_t, Ipv4Address, uint16_t, uint8_t> fid_t;
    
    // Compute FID. Assuming that all packets going out are from
    // IP address on this machine, we can avoid sorting the addresses
    // and always put the local side first and the remote side second
    // something to change later (TODO).
    fid_t fid(std::make_tuple(ipv4.GetSource(), tcp.GetSourcePort(),
                              ipv4.GetDestination(), tcp.GetDestinationPort(),
                              ipv4.GetProtocol()));
    

    // Check if new flow
    std::map<fid_t, FlowState>::iterator fit = flow_info.find(fid);
    if (fit == flow_info.end()){
      NS_LOG_DEBUG(GetNode()->GetId() << " SEND| NEW FLOW: " << five_tuple_str(ipv4, tcp, false));
      FlowState s;
      RenoInit(s);
      flow_info[fid] = s;
      fit = flow_info.find(fid);
    }

    FlowState& st = fit->second;
    TCPState cur_st = st.state;
    switch(st.state){
      case SETUP:{
        switch(st.setup_state){
          case NONE:{
            uint8_t flags = tcp.GetFlags();
            if ((flags & TcpHeader::SYN) > 0 &&
                (flags & !(TcpHeader::SYN)) == 0){
              NS_LOG_DEBUG(GetNode()->GetId() << " SEND| SYN: " << five_tuple_str(ipv4, tcp, false));
              st.initiator = true;
              st.setup_state = SYN;
              st.init_seq = tcp.GetSequenceNumber();
              st.cm_start = st.init_seq.GetValue();
            }
            break;
          }
          case SYN:{
            uint8_t flags = tcp.GetFlags();
            if (!st.initiator &&
                (flags & TcpHeader::SYN) > 0 &&
                (flags & TcpHeader::ACK) > 0 &&
                (flags & !(TcpHeader::SYN) & !(TcpHeader::ACK)) == 0){
              NS_LOG_DEBUG(GetNode()->GetId() << " SEND| SYN ACK: " << five_tuple_str(ipv4, tcp, false));
              st.setup_state = SYN_ACK;
              st.init_seq = tcp.GetSequenceNumber();
              st.cm_start = st.init_seq.GetValue();
            }
            break;
          }
          case SYN_ACK:{
            uint8_t flags = tcp.GetFlags();
            if (st.initiator &&
                (flags & TcpHeader::ACK) > 0 &&
                (flags & !(TcpHeader::ACK)) == 0){
              NS_LOG_DEBUG(GetNode()->GetId() << " SEND| HANDSHAKE ACK: " << five_tuple_str(ipv4, tcp, false)
                                                   << " Init SEQ " << st.init_seq);
              st.setup_state = ACK;
              st.state = DATA;
            }
            break;
          }
          default:
            break;
        }
        break;
      }
      case DATA: {
        uint8_t flags = tcp.GetFlags();
        uint16_t data_size = ipv4.GetPayloadSize() - tcp.GetLength() * 4;
        if (data_size > 0){
          CoCoAEventHandler(packet, ipv4, tcp, st, PKT_ENQ);
          break;
        }
        else if ((flags & TcpHeader::FIN) != 0){
          // We should enqueue and dequeue the packet to hit the tracing hooks.
          //
          if (m_queue->Enqueue (packet)){
            //
            // If the channel is ready for transition we send the packet right now
            // 
            if (m_txMachineState == READY){
              packet = m_queue->Dequeue ();
              m_snifferTrace (packet);
              m_promiscSnifferTrace (packet);
              bool ret = TransmitStart (packet);
              return ret;
            }
          }
          //
          // Enqueue may fail (overflow)
          //
          m_macTxDropTrace (packet);
          return false;
        }
        else{
          st.state = TEAR_DOWN;
          cur_st = TEAR_DOWN;
        }
      }
      case TEAR_DOWN:{
        NS_LOG_DEBUG(GetNode()->GetId() << " SEND| TEAR DOWN " << five_tuple_str(ipv4, tcp, false));
      }
    }
  
    
    if (cur_st != DATA){
      // We should enqueue and dequeue the packet to hit the tracing hooks.
      //
      if (m_queue->Enqueue (packet)){
        //
        // If the channel is ready for transition we send the packet right now
        // 
        if (m_txMachineState == READY){
          packet = m_queue->Dequeue ();
          m_snifferTrace (packet);
          m_promiscSnifferTrace (packet);
          bool ret = TransmitStart (packet);
          return ret;
        }
        return true;
      }
      //
      // Enqueue may fail (overflow)
      //
      m_macTxDropTrace (packet);
      return false;
    }

    return true;
  }
  else{
        
    //
    // We should enqueue and dequeue the packet to hit the tracing hooks.
    //
    if (m_queue->Enqueue (packet)){
      //
      // If the channel is ready for transition we send the packet right now
      // 
      if (m_txMachineState == READY){
        packet = m_queue->Dequeue ();
        m_snifferTrace (packet);
        m_promiscSnifferTrace (packet);
        bool ret = TransmitStart (packet);
        return ret;
      }
      return true;
    }
    //
    // Enqueue may fail (overflow)
    //
    m_macTxDropTrace (packet);
    return false;
  }
  // COCOA End
  //
  
  
}

bool
PointToPointNetDevice::SendFrom (Ptr<Packet> packet, 
                                 const Address &source, 
                                 const Address &dest, 
                                 uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << packet << source << dest << protocolNumber);
  return false;
}

Ptr<Node>
PointToPointNetDevice::GetNode (void) const
{
  return m_node;
}

void
PointToPointNetDevice::SetNode (Ptr<Node> node)
{
  NS_LOG_FUNCTION (this);
  m_node = node;
}

bool
PointToPointNetDevice::NeedsArp (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

void
PointToPointNetDevice::SetReceiveCallback (NetDevice::ReceiveCallback cb)
{
  m_rxCallback = cb;
}

void
PointToPointNetDevice::SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb)
{
  m_promiscCallback = cb;
}

bool
PointToPointNetDevice::SupportsSendFrom (void) const
{
  NS_LOG_FUNCTION (this);
  return false;
}

void
PointToPointNetDevice::DoMpiReceive (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);
  Receive (p);
}

Address 
PointToPointNetDevice::GetRemote (void) const
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_channel->GetNDevices () == 2);
  for (uint32_t i = 0; i < m_channel->GetNDevices (); ++i)
    {
      Ptr<NetDevice> tmp = m_channel->GetDevice (i);
      if (tmp != this)
        {
          return tmp->GetAddress ();
        }
    }
  NS_ASSERT (false);
  // quiet compiler.
  return Address ();
}

bool
PointToPointNetDevice::SetMtu (uint16_t mtu)
{
  NS_LOG_FUNCTION (this << mtu);
  m_mtu = mtu;
  return true;
}

uint16_t
PointToPointNetDevice::GetMtu (void) const
{
  NS_LOG_FUNCTION (this);
  return m_mtu;
}

bool
PointToPointNetDevice::SetCCLatency(uint16_t x)
{
  CC_LATENCY = x;
  return true;
}

uint16_t
PointToPointNetDevice::GetCCLatency(void) const{
  return CC_LATENCY;
}

uint16_t
PointToPointNetDevice::PppToEther (uint16_t proto)
{
  NS_LOG_FUNCTION_NOARGS();
  switch(proto)
    {
    case 0x0021: return 0x0800;   //IPv4
    case 0x0057: return 0x86DD;   //IPv6
    default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  return 0;
}

uint16_t
PointToPointNetDevice::EtherToPpp (uint16_t proto)
{
  NS_LOG_FUNCTION_NOARGS();
  switch(proto)
    {
    case 0x0800: return 0x0021;   //IPv4
    case 0x86DD: return 0x0057;   //IPv6
    default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  return 0;
}


} // namespace ns3
