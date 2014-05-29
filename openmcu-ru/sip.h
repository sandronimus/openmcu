
#ifndef _MCU_SIP_H
#define _MCU_SIP_H

#include "config.h"

#include <map>
#include <ptlib.h>
#include <time.h>
#include <sofia-sip/su.h>
#include <sofia-sip/nta.h>
#include <sofia-sip/nta_stateless.h>
#include <sofia-sip/stun_tag.h>
#include <sofia-sip/sip_status.h>
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/su_log.h>
#include <sofia-sip/auth_digest.h>
#include <sofia-sip/sofia_features.h>
#include <sofia-sip/nta_tport.h>
#include <sofia-sip/tport.h>
#include <sofia-sip/sdp.h>
#include <sofia-sip/msg_addr.h>
#include <sofia-sip/sres_sip.h>

#include "h323.h"
#include "mcu_rtp.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define SIP_USER_AGENT       OpenMCU::Current().GetName()+"/"+OpenMCU::Current().GetVersion()+" ("+SOFIA_SIP_NAME_VERSION+")"
#define SIP_MAX_FORWARDS     "70"
#define SIP_ALLOW_METHODS_REGISTER    "SUBSCRIBE"
#define SIP_ALLOW_METHODS_OPTIONS     "INVITE,BYE,ACK,CANCEL,OPTIONS,SUBSCRIBE,MESSAGE,INFO"

////////////////////////////////////////////////////////////////////////////////////////////////////

class MCUSipEndPoint;
class MCUSipConnection;

////////////////////////////////////////////////////////////////////////////////////////////////////

enum SipSecureTypes
{
  SECURE_TYPE_NONE,
  SECURE_TYPE_ZRTP,
  SECURE_TYPE_SRTP,
  SECURE_TYPE_DTLS_SRTP
};

enum Direction
{
  DIRECTION_INBOUND,
  DIRECTION_OUTBOUND
};

////////////////////////////////////////////////////////////////////////////////////////////////////

PString GetFromIp(PString toAddr, PString toPort);
BOOL GetSipCapabilityParams(PString capname, PString & name, int & pt, int & rate, PString & fmtp);
PString GetSipCallToken(const msg_t *msg, Direction direction);

////////////////////////////////////////////////////////////////////////////////////////////////////

class MCUURL_SIP : public MCUURL
{
  public:
    MCUURL_SIP(const msg_t *msg, Direction dir = DIRECTION_INBOUND);
    const PString & GetRemoteApplication() const { return remote_application; }
    const PString & GetDomainName() const { return domain_name; }
  protected:
    PString domain_name;
    PString remote_application;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class ProxyAccount
{
  public:
    ProxyAccount()
    {
      start_time = PTime(0);
      cseq = 100;
      status = 0;
    }
    void SetStatus(const sip_t *sip);
    PString roomname;
    PString username;
    PString domain;
    PString host;
    PString port;
    PString transport;
    PString password;
    unsigned expires;
    BOOL enable;

    PTime start_time;
    int status;
    PString status_phrase;

    PString sip_www_str;
    PString sip_proxy_str;
    PString call_id;
    unsigned cseq;
};
typedef std::map<PString, ProxyAccount *> ProxyAccountMapType;

////////////////////////////////////////////////////////////////////////////////////////////////////

class SipCapability
{
  public:
    SipCapability(PString _capname)
    {
      capname = _capname;
      Init();
      GetSipCapabilityParams(capname, format, payload, clock, fmtp);
    }
    SipCapability()
    {
      Init();
    }
    ~SipCapability()
    {
      delete cap;
      cap = NULL;
    }
    void Init()
    {
      payload = -1;
      media = 0;
      mode = 3;
      remote_port = 0;
      bandwidth = 0;
      clock = 0;
      cap = NULL;
      inpChan = NULL;
      outChan = NULL;
      secure_type = SECURE_TYPE_NONE;
    }
    void Print();
    int CmpSipCaps(SipCapability &c)
    {
      if(capname != c.capname) return 1;
      if(format != c.format) return 1;
      if(payload != c.payload) return 1;
      if(media != c.media) return 1;
      if(mode != c.mode) return 1;
      if(clock != c.clock) return 1;
      //if(remote_ip != c.remote_ip) return 1; // do not check, may be 0.0.0.0 on reinvite
      if(remote_port != c.remote_port) return 1;
      if(bandwidth != c.bandwidth) return 1;
      if(fmtp != c.fmtp) return 1;
      if(params != c.params) return 1;
      if(secure_type != c.secure_type) return 1;
      if(srtp_remote_type != c.srtp_remote_type) return 1;
      if(srtp_remote_key != c.srtp_remote_key) return 1;
      if(srtp_remote_param != c.srtp_remote_param) return 1;
      inpChan = c.inpChan;
      outChan = c.outChan;
      return 0;
    }

    PString capname; // H.323 capability name
    PString format; // SIP format name
    int payload; // payload type
    int mode; // 0-inactive, 1-recvonly, 2-sendonly, 3-sendrecv
    int media; // media type 0 - audio, 1 - video, 2 - other
    int clock; // rtp clock
    PString remote_ip;
    int remote_port;
    int bandwidth; // bandwidth from MCU
    PString fmtp; // parameters
    PString local_fmtp; // override received parameters
    PString params;
    H323Capability *cap;
    H323_RTPChannel *inpChan;
    H323_RTPChannel *outChan;

    SipSecureTypes secure_type;
    PString srtp_remote_type;
    PString srtp_remote_key;
    PString srtp_remote_param;
    PString srtp_local_type;
    PString srtp_local_key;
    PString srtp_local_param;
    PString zrtp_hash;
    PString dtls_fp;
    PString dtls_fp_type;
};
typedef std::map<int, SipCapability *> SipCapMapType;

////////////////////////////////////////////////////////////////////////////////////////////////////

class H323toSipQueue
{
  public:
    H323toSipQueue() { }
    ~H323toSipQueue() { }
    BOOL Push(PString *cmd)
    {
      PThread::Sleep(10);
      if(!cmd) return FALSE;
      PWaitAndSignal m(mutex);
      if(queue.GetSize() > 100) return FALSE;
      if(queue.GetStringsIndex(*cmd) != P_MAX_INDEX) return FALSE;
      queue.InsertAt(0, cmd);
      return TRUE;
    }
    PString *Pop()
    {
      PThread::Sleep(10);
      PWaitAndSignal m(mutex);
      PString *cmd = (PString *)queue.GetAt(0);
      if(cmd)
      {
        cmd = new PString(*cmd);
        queue.RemoveAt(0);
      }
      return cmd;
    }

  protected:
    PStringArray queue;
    PMutex mutex;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class MCUSipConnection : public MCUH323Connection
{
  public:
    MCUSipConnection(MCUSipEndPoint *_sep, MCUH323EndPoint *_ep, Direction _direction, PString _callToken, const msg_t *msg);
    MCUSipConnection(MCUSipEndPoint *_sep, MCUH323EndPoint *_ep, Direction _direction, PString _callToken);
    ~MCUSipConnection();

    BOOL CreateTempSockets();
    void DeleteTempSockets();

    int ProcessInviteEvent(const msg_t *msg);
    int ProcessReInviteEvent(const msg_t *msg);
    int ProcessACK(const msg_t *msg);
    int ProcessSDP(PString & sdp_str, SipCapMapType & RemoteCaps);
    int ProcessInfo(const msg_t *msg);
    void UpdateLocalContact();

    void StartTransmitChannels();
    void StartReceiveChannels();
    void StartChannel(int pt, int dir);
    void StopChannel(int pt, int dir);
    void StopTransmitChannels();
    void StopReceiveChannels();
    void DeleteMediaChannels(int pt);
    void DeleteChannels();
    void CreateLogicalChannels();
    int CreateMediaChannel(int pt, int dir);
    SipRTP_UDP *CreateRTPSession(SipCapability *sc);

    void FindCapability_H263(SipCapability &c,PStringArray &keys, const char * _H323Name, const char * _SIPName);
    void SelectCapability_H261(SipCapability & sc);
    void SelectCapability_H263(SipCapability & sc);
    void SelectCapability_H263p(SipCapability & sc);
    void SelectCapability_H264(SipCapability & sc);
    void SelectCapability_VP8(SipCapability & sc);
    void SelectCapability_SPEEX(SipCapability & sc);
    void SelectCapability_OPUS(SipCapability & sc);
    virtual BOOL WriteSignalPDU(H323SignalPDU & pdu) { return TRUE; }

    void CleanUpOnCallEnd();
    void LeaveMCU();
    void LeaveMCU(BOOL remove);
    virtual void SendLogicalChannelMiscCommand(H323Channel & channel, unsigned command);

    int SendBYE();
    int SendACK();
    int SendVFU();
    void ReceivedVFU();
    int SendRequest(sip_method_t method, const char *method_name, msg_t *sip_msg);
    void ReceivedDTMF(PString payload);
    BOOL HadAnsweredCall() { return (direction=DIRECTION_INBOUND); }

    BOOL IsAwaitingSignalConnect() { return connectionState == AwaitingSignalConnect; };
    BOOL IsConnected() const { return connectionState == EstablishedConnection; }
    BOOL IsEstablished() const { return connectionState == EstablishedConnection; }

    PString sdp_invite_str;
    PString sdp_ok_str;
    PString ruri_str;
    PString contact_str;

  protected:
    sdp_rtpmap_t *CreateSdpRtpmap(su_home_t *sess_home, SipCapability *sc);
    sdp_media_t *CreateSdpMedia(su_home_t *sess_home, sdp_media_e m_type, sdp_proto_e m_proto);
    sdp_attribute_t *CreateSdpAttr(su_home_t *sess_home, PString m_name, PString m_value);
    sdp_parser_t *SdpParser(PString sdp_str);

    PString CreateSdpStr();
    int CreateSdpOk();
    int CreateSdpInvite();

    void RefreshLocalSipCaps();
    BOOL MergeSipCaps(SipCapMapType & BaseCaps, SipCapMapType & RemoteCaps);

    Direction direction;
    PString local_user;
    PString local_ip;
    PString display_name;
    msg_t *c_sip_msg;
    int cseq_num;

    int scap; // selected audio capability payload type
    int vcap; // selected video capability payload type

    PString pref_audio_cap;
    PString pref_video_cap;
    PString rtp_proto;
    unsigned remote_bw; // bandwidth to MCU

    PString key_audio80;
    PString key_audio32;
    PString key_video80;
    PString key_video32;

    // temp rtp sockets for outgoing invite
    PUDPSocket *aDataSocket, *aControlSocket;
    PUDPSocket *vDataSocket, *vControlSocket;
    unsigned audio_rtp_port, video_rtp_port;

    PString sdp_o_username;
    unsigned int sdp_o_id;
    unsigned int sdp_o_ver;

    SipCapMapType LocalSipCaps;
    SipCapMapType RemoteSipCaps;
    MCUSipEndPoint *sep;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class MCUSipEndPoint : public PThread
{
  public:
    MCUSipEndPoint(MCUH323EndPoint *_ep)
      :PThread(10000,NoAutoDeleteThread,NormalPriority,"SIP Listener:%0x"), ep(_ep)
    {
      restart = 1;
      terminating = 0;
    }
    ~MCUSipEndPoint()
    {
    }
    void Main();
    void Initialise();

    int terminating;
    int restart;

    H323toSipQueue SipQueue;
    void ProcessSipQueue();
    void ProcessProxyAccount();

    PStringArray sipListenerArray;
    BOOL FindListener(PString addr);

    nta_agent_t *GetAgent() { return agent; };
    su_home_t *GetHome() { return &home; };

    nta_outgoing_t * SipMakeCall(PString from, PString to, PString & callToken);
    int CreateIncomingConnection(const msg_t *msg);
    int CreateOutgoingConnection(const msg_t *msg);
    int SipReqReply(const msg_t *msg, unsigned status, const char *status_phrase=NULL, const char *auth_str=NULL, const char *contact_str=NULL, const char *content_str=NULL, const char *payload_str=NULL);

    BOOL MakeProxyAuth(ProxyAccount *proxy, const sip_t *sip);
    PString MakeAuthStr(PString username, PString password, PString uri, const char *method, const char *scheme, const char *realm, const char *nonce);

    ProxyAccountMapType ProxyAccountMap;
    void InsertProxyAccount(ProxyAccount *proxy);
    ProxyAccount *FindProxyAccount(PString account);

    SipCapMapType & GetBaseSipCaps() { return BaseSipCaps; }

  protected:
    void MainLoop();
    MCUH323EndPoint *ep;
    su_home_t home;
    su_root_t *root;
    nta_agent_t *agent;

    MCUSipConnection * FindConnectionWithLock(const PString & callToken)
    { return (MCUSipConnection *)ep->FindConnectionWithLock(callToken); }

    MCUSipConnection * FindConnectionWithoutLock(const PString & callToken)
    { return (MCUSipConnection *)ep->FindConnectionWithoutLock(callToken); }

    static int /*__attribute__((cdecl))*/ ProcessSipEventWrap_cb(nta_agent_magic_t *context, nta_agent_t *agent, msg_t *msg, sip_t *sip)
    { return ((MCUSipEndPoint *)context)->ProcessSipEvent_cb(agent, msg, sip); }
    int ProcessSipEvent_cb(nta_agent_t *agent, msg_t *msg, sip_t *sip);

    static int nta_response_cb1_wrap(nta_outgoing_magic_t *context, nta_outgoing_t *orq, const sip_t *sip)
    { return ((MCUSipEndPoint *)context)->nta_response_cb1(orq, sip); }
    int nta_response_cb1(nta_outgoing_t *orq, const sip_t *sip);

    static int ProcessSipEventWrap_request1(nta_leg_magic_t *context, nta_leg_t *leg, nta_incoming_t *irq, const sip_t *sip)
    { return ((MCUSipEndPoint *)context)->ProcessSipEvent_request1(leg, irq, sip); }
    int ProcessSipEvent_request1(nta_leg_t *leg, nta_incoming_t *irq, const sip_t *sip);

    int SipRegister(ProxyAccount *, BOOL enable = TRUE);
    PString GetRoomAccess(const sip_t *sip);

    void InitProxyAccounts();

    void CreateBaseSipCaps();
    SipCapMapType BaseSipCaps;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // _MCU_SIP_H
