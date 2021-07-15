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
#ifndef __REUSABLE_RTP_ENDPOINT_IMPL_HPP__
#define __REUSABLE_RTP_ENDPOINT_IMPL_HPP__

#include "BaseRtpEndpointImpl.hpp"
#include "SipRtpEndpoint.hpp"
#include "PassThroughImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <EventHandler.hpp>

namespace kurento
{

class MediaPipeline;

class SipRtpEndpointImpl;

void Serialize (std::shared_ptr<SipRtpEndpointImpl> &object,
                JsonSerializer &serializer);

// TODO: SipRtpEndpoint can inherit from RtpEndpoint, but we need to add a protected constructor to RtpEndpointImpl that allows us to specify the FACTORY_NAME  for GStreamer element
class SipRtpEndpointImpl : public BaseRtpEndpointImpl, public virtual SipRtpEndpoint
{

public:

	SipRtpEndpointImpl (const boost::property_tree::ptree &conf,
                   std::shared_ptr<MediaPipeline> mediaPipeline,
                   std::shared_ptr<SDES> crypto,
				   bool useIpv6);

  virtual ~SipRtpEndpointImpl ();

  sigc::signal<void, OnKeySoftLimit> signalOnKeySoftLimit;

  std::shared_ptr<SipRtpEndpointImpl> getCleanEndpoint (const boost::property_tree::ptree &conf,
          std::shared_ptr<MediaPipeline> mediaPipeline,
          std::shared_ptr<SDES> crypto, bool useIpv6,
		  const std::string &sdp,
		  bool continue_audio_stream,
		  bool continue_video_stream);


  void setAudioSsrc (guint32 ssrc);
  void setVideoSsrc (guint32 ssrc);

  /* Next methods are automatically implemented by code generator */
  using BaseRtpEndpointImpl::connect;
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler) override;

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response) override;

  virtual void Serialize (JsonSerializer &serializer) override;

  virtual void postConstructor () override;


protected:
private:

  gulong handlerOnKeySoftLimit = 0;
  void onKeySoftLimit (gchar *media);

  std::shared_ptr<SipRtpEndpointImpl> cloneToNewEndpoint (std::shared_ptr<SipRtpEndpointImpl> newEp, const std::string &sdp, bool continue_audio_stream, bool continue_video_stream);

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __SIP_RTP_ENDPOINT_IMPL_HPP__ */
