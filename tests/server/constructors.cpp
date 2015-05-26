/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */

#include <glibmm/module.h>
#include <FactoryRegistrar.hpp>
#include <ModuleManager.hpp>
#include <MediaObjectImpl.hpp>
#include <KurentoException.hpp>
#include <jsonrpc/JsonSerializer.hpp>
#include <MediaSet.hpp>
#include <gst/gst.h>
#include <config.h>

boost::property_tree::ptree config;

void
testHttpPostEndPoint (kurento::ModuleManager &moduleManager,
                      std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);

  w.SerializeNVP (mediaPipeline);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("HttpPostEndpoint")->createObject (config, "",
        w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

void
testPlayerEndPoint (kurento::ModuleManager &moduleManager,
                    std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);
  std::string uri ("http://kurento.org");

  w.SerializeNVP (mediaPipeline);
  w.SerializeNVP (uri);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("PlayerEndpoint")->createObject (config, "",
        w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

void
testRecorderEndPoint (kurento::ModuleManager &moduleManager,
                      std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);
  std::string uri ("http://kurento.org");

  w.SerializeNVP (mediaPipeline);
  w.SerializeNVP (uri);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("RecorderEndpoint")->createObject (config, "",
        w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

void
testRtpEndpoint (kurento::ModuleManager &moduleManager,
                 std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);

  w.SerializeNVP (mediaPipeline);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("RtpEndpoint")->createObject (config, "",
        w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

void
testWebRTCEndpoint (kurento::ModuleManager &moduleManager,
                    std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);

  w.SerializeNVP (mediaPipeline);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("WebRtcEndpoint")->createObject (config, "",
        w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

void
testMixer (kurento::ModuleManager &moduleManager,
           std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);

  w.SerializeNVP (mediaPipeline);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("Mixer")->createObject (config, "", w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

void
testDispatcher (kurento::ModuleManager &moduleManager,
                std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);

  w.SerializeNVP (mediaPipeline);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("Dispatcher")->createObject (config, "", w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

void
testDispatcherOneToMany (kurento::ModuleManager &moduleManager,
                         std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);

  w.SerializeNVP (mediaPipeline);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("DispatcherOneToMany")->createObject (config, "",
        w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

void
testComposite (kurento::ModuleManager &moduleManager,
               std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline)
{
  kurento::JsonSerializer w (true);

  w.SerializeNVP (mediaPipeline);

  std::shared_ptr <kurento::MediaObjectImpl >  object =
    moduleManager.getFactory ("Composite")->createObject (config, "", w.JsonValue);
  kurento::MediaSet::getMediaSet()->release (object);
}

int
main (int argc, char **argv)
{
  std::shared_ptr <kurento::MediaObjectImpl> mediaPipeline;
  std::shared_ptr <kurento::Factory> factory;

  gst_init (&argc, &argv);

  kurento::ModuleManager moduleManager;

  moduleManager.loadModulesFromDirectories ("../../src/server");

  mediaPipeline = moduleManager.getFactory ("MediaPipeline")->createObject (
                    config, "",
                    Json::Value() );

  config.add ("configPath", "../../../tests" );
  config.add ("modules.kurento.SdpEndpoint.numAudioMedias", 0);
  config.add ("modules.kurento.SdpEndpoint.numVideoMedias", 0);
  config.add ("modules.kurento.SdpEndpoint.audioCodecs", "[]");
  config.add ("modules.kurento.SdpEndpoint.videoCodecs", "[]");

  testHttpPostEndPoint (moduleManager, mediaPipeline);
  testPlayerEndPoint (moduleManager, mediaPipeline);
  testRecorderEndPoint (moduleManager, mediaPipeline);
  testRtpEndpoint (moduleManager, mediaPipeline);
  testWebRTCEndpoint (moduleManager, mediaPipeline);
  testMixer (moduleManager, mediaPipeline);
  testDispatcher (moduleManager, mediaPipeline);
  testDispatcherOneToMany (moduleManager, mediaPipeline);
  testComposite (moduleManager, mediaPipeline);

  kurento::MediaSet::getMediaSet()->release (mediaPipeline);

  return 0;
}
