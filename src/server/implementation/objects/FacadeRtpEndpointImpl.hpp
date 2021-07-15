/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef __SIP_RTP_ENDPOINT_IMPL_HPP__
#define __SIP_RTP_ENDPOINT_IMPL_HPP__

#include "ComposedObjectImpl.hpp"
#include "SipRtpEndpoint.hpp"
#include <BaseRtpEndpointImpl.hpp>
#include <EventHandler.hpp>
#include <SipRtpEndpointImpl.hpp>
#include <sigc++/connection.h>
#include <gst/sdp/gstsdpmessage.h>




namespace kurento
{

class MediaPipeline;

class FacadeRtpEndpointImpl;

class SDES;
class CryptoSuite;

void Serialize (std::shared_ptr<FacadeRtpEndpointImpl> &object,
                JsonSerializer &serializer);

class FacadeRtpEndpointImpl : public ComposedObjectImpl, public virtual SipRtpEndpoint
{

public:

  FacadeRtpEndpointImpl (const boost::property_tree::ptree &conf,
                   std::shared_ptr<MediaPipeline> mediaPipeline,
                   std::shared_ptr<SDES> crypto,
				   bool cryptoAgnostic,
				   bool useIpv6);

  virtual ~FacadeRtpEndpointImpl ();

  operator BaseRtpEndpointImpl();



  /*----------------- MEthods from BaseRtpEndpoint ---------------*/
  int getMinVideoRecvBandwidth () override;
  void setMinVideoRecvBandwidth (int minVideoRecvBandwidth) override;

  int getMinVideoSendBandwidth () override;
  void setMinVideoSendBandwidth (int minVideoSendBandwidth) override;

  int getMaxVideoSendBandwidth () override;
  void setMaxVideoSendBandwidth (int maxVideoSendBandwidth) override;

  std::shared_ptr<MediaState> getMediaState () override;
  std::shared_ptr<ConnectionState> getConnectionState () override;

  std::shared_ptr<RembParams> getRembParams () override;
  void setRembParams (std::shared_ptr<RembParams> rembParams)override;

  sigc::signal<void, MediaStateChanged> getSignalMediaStateChanged ();
  sigc::signal<void, ConnectionStateChanged> getSignalConnectionStateChanged ();

  virtual int getMtu ();
  virtual void setMtu (int mtu);


  /*---------------- Overloaded methods from SDP Endpoint ---------------*/
  int getMaxVideoRecvBandwidth () override;
  void setMaxVideoRecvBandwidth (int maxVideoRecvBandwidth) override;
  int getMaxAudioRecvBandwidth () override;
  void setMaxAudioRecvBandwidth (int maxAudioRecvBandwidth) override;
  std::string generateOffer () override;
  std::string processOffer (const std::string &offer) override;
  std::string processAnswer (const std::string &answer) override;
  std::string getLocalSessionDescriptor () override;
  std::string getRemoteSessionDescriptor () override;


  /*----------------------- Overloaded methods from Media Element --------------*/
  std::map <std::string, std::shared_ptr<Stats>> getStats () override;
  std::map <std::string, std::shared_ptr<Stats>> getStats (
        std::shared_ptr<MediaType> mediaType) override;


  std::vector<std::shared_ptr<ElementConnectionData>> getSourceConnections () override;
  std::vector<std::shared_ptr<ElementConnectionData>>
      getSourceConnections (
        std::shared_ptr<MediaType> mediaType) override;
  std::vector<std::shared_ptr<ElementConnectionData>>
      getSourceConnections (
        std::shared_ptr<MediaType> mediaType, const std::string &description) override;
  std::vector<std::shared_ptr<ElementConnectionData>>
      getSinkConnections () override;
  std::vector<std::shared_ptr<ElementConnectionData>> getSinkConnections (
        std::shared_ptr<MediaType> mediaType) override;
  std::vector<std::shared_ptr<ElementConnectionData>> getSinkConnections (
        std::shared_ptr<MediaType> mediaType, const std::string &description) override;
  void setAudioFormat (std::shared_ptr<AudioCaps> caps) override;
  void setVideoFormat (std::shared_ptr<VideoCaps> caps) override;

  virtual void release () override;

  virtual std::string getGstreamerDot () override;
  virtual std::string getGstreamerDot (std::shared_ptr<GstreamerDotDetails>
                                       details) override;

  virtual void setOutputBitrate (int bitrate) override;

  bool isMediaFlowingIn (std::shared_ptr<MediaType> mediaType) override;
  bool isMediaFlowingIn (std::shared_ptr<MediaType> mediaType,
                         const std::string &sinkMediaDescription) override;
  bool isMediaFlowingOut (std::shared_ptr<MediaType> mediaType) override;
  bool isMediaFlowingOut (std::shared_ptr<MediaType> mediaType,
                          const std::string &sourceMediaDescription) override;
  bool isMediaTranscoding (std::shared_ptr<MediaType> mediaType) override;
  bool isMediaTranscoding (std::shared_ptr<MediaType> mediaType,
                           const std::string &binName) override;

  virtual int getMinOuputBitrate () override;
  virtual void setMinOuputBitrate (int minOuputBitrate) override;

  virtual int getMinOutputBitrate () override;
  virtual void setMinOutputBitrate (int minOutputBitrate) override;

  virtual int getMaxOuputBitrate () override;
  virtual void setMaxOuputBitrate (int maxOuputBitrate) override;

  virtual int getMaxOutputBitrate () override;
  virtual void setMaxOutputBitrate (int maxOutputBitrate) override;


  /* Next methods are automatically implemented by code generator */
  using ComposedObjectImpl::connect;
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler) override;



  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response) override;

  virtual void Serialize (JsonSerializer &serializer) override;

protected:
  virtual void postConstructor () override;

private:

  bool cryptoAgnostic;

  sigc::signal<void, MediaStateChanged> signalMediaStateChanged;
  sigc::signal<void, ConnectionStateChanged> signalConnectionStateChanged;
  sigc::signal<void, MediaSessionStarted> signalMediaSessionStarted;
  sigc::signal<void, MediaSessionTerminated> signalMediaSessionTerminated;
  sigc::signal<void, OnKeySoftLimit> signalOnKeySoftLimit;

  sigc::connection connMediaStateChanged;
  sigc::connection connConnectionStateChanged;
  sigc::connection connMediaSessionStarted;
  sigc::connection connMediaSessionTerminated;
  sigc::connection connOnKeySoftLimit;



  guint32 agnosticCryptoAudioSsrc;
  guint32 agnosticCryptoVideoSsrc;
  guint32 agnosticNonCryptoAudioSsrc;
  guint32 agnosticNonCryptoVideoSsrc;

  bool
  sameConnection (GstSDPMessage *sdp1, GstSDPMessage *sdp2);

  bool
  findCompatibleMedia (GstSDPMedia* media, GstSDPMessage *oldAnswer);

  void
  answerHasCompatibleMedia (const std::string& answer, bool& audio_compatible, bool& video_compatible);

  bool
  isCryptoAgnostic ();

  bool
  generateCryptoAgnosticOffer (std::string& offer);

  bool
  checkCryptoOffer (std::string& offer, std::shared_ptr<SDES>& crypto);

  bool
  checkCryptoAnswer (std::string& answer, std::shared_ptr<SDES>& crypto);

  void
  replaceSsrc (GstSDPMedia *media, guint idx, gchar *newSsrcStr, guint32 &oldSsrc);

  void
  replaceAllSsrcAttrs (GstSDPMedia *media, std::list<guint> sscrIdxs, guint32 &oldSsrc, guint32 &newSsrc);

  void
  removeCryptoAttrs (GstSDPMedia *media, std::list<guint> cryptoIdx);

  void
  addAgnosticMedia (GstSDPMedia *media, GstSDPMessage *sdpOffer);

  void
  disconnectForwardSignals ();

  void
  connectForwardSignals ();

  std::shared_ptr<SipRtpEndpointImpl>
  renewInternalEndpoint (std::shared_ptr<SipRtpEndpointImpl> newEndpoint);

  void
  setProperties (std::shared_ptr<SipRtpEndpointImpl> from);

  std::shared_ptr<AudioCaps> audioCapsSet;
  std::shared_ptr<VideoCaps> videoCapsSet;
  std::shared_ptr<RembParams> rembParamsSet;

  std::shared_ptr<SipRtpEndpointImpl> rtp_ep;

  std::shared_ptr<SDES> cryptoCache;

  bool useIpv6Cache;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __SIP_RTP_ENDPOINT_IMPL_HPP__ */
