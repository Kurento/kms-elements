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
#ifndef __RECORDER_ENDPOINT_IMPL_HPP__
#define __RECORDER_ENDPOINT_IMPL_HPP__

#include "UriEndpointImpl.hpp"
#include "RecorderEndpoint.hpp"
#include <EventHandler.hpp>
#include <condition_variable>

namespace kurento
{

class MediaPipeline;
class MediaProfileSpecType;
class RecorderEndpointImpl;

void Serialize (std::shared_ptr<RecorderEndpointImpl> &object,
                JsonSerializer &serializer);

class RecorderEndpointImpl : public UriEndpointImpl,
  public virtual RecorderEndpoint
{

public:

  RecorderEndpointImpl (const boost::property_tree::ptree &conf,
                        std::shared_ptr<MediaPipeline> mediaPipeline, const std::string &uri,
                        std::shared_ptr<MediaProfileSpecType> mediaProfile, bool stopOnEndOfStream);

  virtual ~RecorderEndpointImpl ();

  void record () override;
  virtual void stopAndWait () override;

  /* Next methods are automatically implemented by code generator */
  using UriEndpointImpl::connect;
  virtual bool connect (const std::string &eventType,
                        std::shared_ptr<EventHandler> handler) override;

  sigc::signal<void, Recording> signalRecording;
  sigc::signal<void, Paused> signalPaused;
  sigc::signal<void, Stopped> signalStopped;

  virtual void invoke (std::shared_ptr<MediaObjectImpl> obj,
                       const std::string &methodName, const Json::Value &params,
                       Json::Value &response) override;

  virtual void Serialize (JsonSerializer &serializer) override;

protected:
  virtual void fillStatsReport (std::map <std::string, std::shared_ptr<Stats>>
                                &report, const GstStructure *stats,
                                double timestamp, int64_t timestampMillis) override;

  virtual void postConstructor () override;

  virtual void release () override;
private:
  static bool support_ksr;
  gulong handlerOnStateChanged = 0;
  std::mutex mtx;
  std::condition_variable cv;
  gint state{};

  void onStateChanged (gint state);
  void waitForStateChange (gint state);

  void collectEndpointStats (std::map <std::string, std::shared_ptr<Stats>>
                             &statsReport, std::string id, const GstStructure *stats,
                             double timestamp, int64_t timestampMillis);

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;
};

} /* kurento */

#endif /*  __RECORDER_ENDPOINT_IMPL_HPP__ */
