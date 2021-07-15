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
#ifndef __COMPOSED_OBJECT_IMPL_HPP__
#define __COMPOSED_OBJECT_IMPL_HPP__

#include <MediaElementImpl.hpp>
#include <PassThroughImpl.hpp>
#include <EventHandler.hpp>

namespace kurento
{

class MediaPipeline;

class ComposedObjectImpl;

void Serialize (std::shared_ptr<ComposedObjectImpl> &object,
                JsonSerializer &serializer);

class ComposedObjectImpl : public MediaElementImpl
{

public:

	ComposedObjectImpl (const boost::property_tree::ptree &conf,
                   std::shared_ptr<MediaPipeline> mediaPipeline);

  virtual ~ComposedObjectImpl ();


  void linkMediaElement (std::shared_ptr<MediaElement> linkSrc, std::shared_ptr<MediaElement> linkSink);

  // Connectivity methods needed to override to make the composition of objects

  void connect (std::shared_ptr<MediaElement> sink) override;
  void connect (std::shared_ptr<MediaElement> sink,
                        std::shared_ptr<MediaType> mediaType) override;
  void connect (std::shared_ptr<MediaElement> sink,
                        std::shared_ptr<MediaType> mediaType,
                        const std::string &sourceMediaDescription) override;


  void disconnect (std::shared_ptr<MediaElement> sink) override;
  void disconnect (std::shared_ptr<MediaElement> sink,
                           std::shared_ptr<MediaType> mediaType) override;
  void disconnect (std::shared_ptr<MediaElement> sink,
                           std::shared_ptr<MediaType> mediaType,
                           const std::string &sourceMediaDescription) override;


protected:
  virtual void postConstructor () override;
  bool connect (const std::string &eventType, std::shared_ptr<EventHandler> handler);
  std::shared_ptr<PassThroughImpl> sinkPt;
  std::shared_ptr<PassThroughImpl> srcPt;

  template<typename T>
  sigc::connection connectEventToExternalHandler (sigc::signal<void, T>& signal, std::weak_ptr<EventHandler>& wh)
  {
      sigc::connection conn = signal.connect ([ &, wh] (T event) {
        std::shared_ptr<EventHandler> lh = wh.lock();
        if (!lh)
          return;

        std::shared_ptr<T> ev_ref (new T(event));
        auto object = this->shared_from_this();

        lh->sendEventAsync ([ev_ref, object, lh] {
            JsonSerializer s (true);

            s.Serialize ("data", ev_ref.get());
            s.Serialize ("object", object.get());
            s.JsonValue["type"] = T::getName().c_str();

            lh->sendEvent (s.JsonValue);
        });
      });
      return conn;
  }

  template<typename T> void
  raiseEvent (T& event, std::shared_ptr<MediaObject> self, sigc::signal<void, T>& signal)
  {
  	  try {
  		  T event2 (event);

  		  event2.setSource(self);
  		  sigcSignalEmit(signal, event2);
  	  } catch (const std::bad_weak_ptr &e) {
  	    // shared_from_this()
  	    GST_ERROR ("BUG creating %s: %s", T::getName ().c_str (),
  	        e.what ());
  	  }
  }



private:

  GstElement* origElem;
  std::shared_ptr<MediaElement> linkedSource;
  std::shared_ptr<MediaElement> linkedSink;

  sigc::signal<void, ElementConnected> signalElementConnected;
  sigc::signal<void, ElementDisconnected> signalElementDisconnected;
  sigc::signal<void, MediaFlowOutStateChange> signalMediaFlowOutStateChange;
  sigc::signal<void, MediaFlowInStateChange> signalMediaFlowInStateChange;
  sigc::signal<void, MediaTranscodingStateChange> signalMediaTranscodingStateChange;
  sigc::signal<void, Error> signalError;

  sigc::connection connElementConnectedSrc;
  sigc::connection connElementConnectedSink;
  sigc::connection connElementDisconnectedSrc;
  sigc::connection connElementDisconnectedSink;
  sigc::connection connMediaTranscodingStateChangeSrc;
  sigc::connection connMediaTranscodingStateChangeSink;
  sigc::connection connMediaFlowOutStateChange;
  sigc::connection connMediaFlowInStateChange;
  sigc::connection connErrorSrc;
  sigc::connection connErrorSink;
  sigc::connection connErrorlinkedSrc;
  sigc::connection connErrorlinkedSink;



  std::recursive_mutex linkMutex;

  class StaticConstructor
  {
  public:
    StaticConstructor();
  };

  static StaticConstructor staticConstructor;

  void connectForwardSignals ();
  void disconnectForwardSignals ();

};

} /* kurento */

#endif /*  __COMPOSED_OBJECT_IMPL_HPP__ */
