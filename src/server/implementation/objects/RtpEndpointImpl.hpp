#ifndef __RTP_ENDPOINT_IMPL_HPP__
#define __RTP_ENDPOINT_IMPL_HPP__

#include "BaseRtpEndpointImpl.hpp"
#include "RtpEndpoint.hpp"
#include <EventHandler.hpp>

namespace kurento
{

class MediaPipeline;
class RtpEndpointImpl;

void Serialize (std::shared_ptr<RtpEndpointImpl> &object,
                JsonSerializer &serializer);

class RtpEndpointImpl : public BaseRtpEndpointImpl, public virtual RtpEndpoint
{

public:

  RtpEndpointImpl (const boost::property_tree::ptree &conf,
                   std::shared_ptr<MediaPipeline> mediaPipeline);

  virtual ~RtpEndpointImpl () {};

  /* Next methods are automatically implemented by code generator */
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler);

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response);

  virtual void Serialize (JsonSerializer &serializer);

private:

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

};

} /* kurento */

#endif /*  __RTP_ENDPOINT_IMPL_HPP__ */
