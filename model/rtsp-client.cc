/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#include <ns3/rtsp-client.h>

#include <sstream>
#include <ns3/log.h>
#include <ns3/simulator.h>
#include <ns3/callback.h>
#include <ns3/config.h>
#include <ns3/pointer.h>
#include <ns3/uinteger.h>
#include <ns3/three-gpp-http-variables.h>
#include <ns3/packet.h>
#include <ns3/socket.h>
#include <ns3/tcp-socket.h>
#include <ns3/tcp-socket-factory.h>
#include <ns3/inet-socket-address.h>
#include <ns3/inet6-socket-address.h>
#include <ns3/unused.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("RtspClient");

NS_OBJECT_ENSURE_REGISTERED(RtspClient);

TypeId
RtspClient::GetTypeId (void) 
{
    static TypeId tid = TypeId("ns3::RtspClient")
        .SetParent<Application> ()
        .SetGroupName("Applications")
        .AddConstructor<RtspClient>()
    ;
    return tid;
}

RtspClient::RtspClient ()
{
    NS_LOG_FUNCTION (this);
}

RtspClient::~RtspClient ()
{
    NS_LOG_FUNCTION (this);
}

void
RtspClient::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  if (!Simulator::IsFinished ())
    {
      StopApplication ();
    }

  Application::DoDispose (); // Chain up.
}

void
RtspClient::StartApplication ()
{
    NS_LOG_FUNCTION (this);

    /*RTSP 소켓 초기화*/
    if (m_rtspSocket == 0)
    {
        // Find the current default MTU value of TCP sockets.
        Ptr<const ns3::AttributeValue> previousSocketMtu;
        const TypeId tcpSocketTid = TcpSocket::GetTypeId ();
        for (uint32_t i = 0; i < tcpSocketTid.GetAttributeN (); i++)
        {
            struct TypeId::AttributeInformation attrInfo = tcpSocketTid.GetAttribute (i);
            if (attrInfo.name == "SegmentSize")
            {
                previousSocketMtu = attrInfo.initialValue;
            }
        }

        // Creating a TCP socket to connect to the Client.
        m_rtspSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());

        if (Ipv4Address::IsMatchingType (m_localAddress))
        {
            const Ipv4Address ipv4 = Ipv4Address::ConvertFrom (m_localAddress);
            const InetSocketAddress inetSocket = InetSocketAddress (ipv4, m_rtspPort);
            if (m_rtpSocket->Bind (inetSocket) == -1)
              {
                NS_FATAL_ERROR ("Failed to bind socket");
              }
        }
        m_rtspSocket->Listen ();
    } // end of `if (m_rtspSocket == 0)`

    NS_ASSERT_MSG (m_rtspSocket != 0, "Failed creating RTSP socket.");

    m_rtspSocket->SetRecvCallback (MakeCallback (&RtspClient::HandleRtspReceive, this));

    /* RTP 소켓 초기화 */
    if (m_rtpSocket == 0)
    {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        m_rtpSocket = Socket::CreateSocket (GetNode (), tid);
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_rtpPort);
        if (m_rtpSocket->Bind (local) == -1)
        {
            NS_FATAL_ERROR ("Failed to bind socket");
        }
    }
    NS_ASSERT_MSG (m_rtpSocket != 0, "Failed creating RTP socket.");

    m_rtpSocket->SetRecvCallback (MakeCallback (&RtspClient::HandleRtpReceive, this));

    /* RTCP 소켓 초기화 */
    if (m_rtcpSocket == 0)
    {
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        m_rtcpSocket = Socket::CreateSocket (GetNode (), tid);
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_rtcpPort);
        if (m_rtcpSocket->Bind (local) == -1)
        {
            NS_FATAL_ERROR ("Failed to bind socket");
        }
    }
    NS_ASSERT_MSG (m_rtpSocket != 0, "Failed creating RTP socket.");

    m_rtpSocket->SetRecvCallback (MakeCallback (&RtspClient::HandleRtcpReceive, this));
} // end of `void StartApplication ()`

void
RtspClient::StopApplication ()
{
  NS_LOG_FUNCTION (this);

  // Stop listening.
  if (m_rtspSocket != 0)
    {
      m_rtspSocket->Close ();
      m_rtspSocket->SetAcceptCallback (MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
                                          MakeNullCallback<void, Ptr<Socket>, const Address &> ());
      m_rtspSocket->SetCloseCallbacks (MakeNullCallback<void, Ptr<Socket> > (),
                                          MakeNullCallback<void, Ptr<Socket> > ());
      m_rtspSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_rtspSocket->SetSendCallback (MakeNullCallback<void, Ptr<Socket>, uint32_t > ());
    }
  if (m_rtpSocket != 0) 
  {
    m_rtpSocket->Close();
    m_rtpSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  }
}

//RTSP handler
void
RtspClient::HandleRtspReceive (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  while((packet = socket->Recv()))
  {
    if (packet->GetSize () == 0)
    {
      break;
    }
    uint8_t* msg = new uint8_t[packet->GetSize()+1];
    packet->CopyData(msg, packet->GetSize());
    std::istringstream strin(std::string((char*)msg), std::ios::in);
  }
}

//RTP handler
void
RtspClient::HandleRtpReceive(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
  {
    socket->GetSockName (localAddress);

    if(m_clientAddress != from)
    {
      m_clientAddress = from;
    }

    uint8_t* msg = new uint8_t[packet->GetSize()+1];
    packet->CopyData(msg, packet->GetSize());
    std::cout<< '\n' << msg << '\n';
  }
}

//RTCP handler
void
RtspClient::HandleRtcpReceive(Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);

  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
    {
      socket->GetSockName (localAddress);

      if(m_clientAddress != from)
      {
        m_clientAddress = from;
      }
    }   
}

}