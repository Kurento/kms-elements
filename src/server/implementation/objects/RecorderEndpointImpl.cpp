#include <gst/gst.h>
#include "MediaType.hpp"
#include "MediaPipeline.hpp"
#include "MediaProfileSpecType.hpp"
#include <RecorderEndpointImplFactory.hpp>
#include "RecorderEndpointImpl.hpp"
#include <jsonrpc/JsonSerializer.hpp>
#include <KurentoException.hpp>
#include <gst/gst.h>
#include <commons/kmsrecordingprofile.h>

#include "StatsType.hpp"
#include "EndpointStats.hpp"
#include <commons/kmsutils.h>
#include <commons/kmsstats.h>

#define GST_CAT_DEFAULT kurento_recorder_endpoint_impl
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "KurentoRecorderEndpointImpl"

#define FACTORY_NAME "recorderendpoint"

namespace kurento
{

bool RecorderEndpointImpl::support_ksr;

static bool
check_support_for_ksr ()
{
  GstPlugin *plugin = NULL;
  bool supported;

  plugin = gst_plugin_load_by_name ("kmsrecorder");

  supported = plugin != NULL;

  g_clear_object (&plugin);

  return supported;
}

RecorderEndpointImpl::RecorderEndpointImpl (const boost::property_tree::ptree
    &conf,
    std::shared_ptr<MediaPipeline> mediaPipeline, const std::string &uri,
    std::shared_ptr<MediaProfileSpecType> mediaProfile,
    bool stopOnEndOfStream) : UriEndpointImpl (conf,
          std::dynamic_pointer_cast<MediaObjectImpl> (mediaPipeline), FACTORY_NAME, uri)
{
  g_object_ref (getGstreamerElement() );

  g_object_set (G_OBJECT (getGstreamerElement() ), "accept-eos",
                stopOnEndOfStream, NULL);

  switch (mediaProfile->getValue() ) {
  case MediaProfileSpecType::WEBM:
    g_object_set ( G_OBJECT (element), "profile", KMS_RECORDING_PROFILE_WEBM, NULL);
    GST_INFO ("Set WEBM profile");
    break;

  case MediaProfileSpecType::MP4:
    g_object_set ( G_OBJECT (element), "profile", KMS_RECORDING_PROFILE_MP4, NULL);
    GST_INFO ("Set MP4 profile");
    break;

  case MediaProfileSpecType::WEBM_VIDEO_ONLY:
    g_object_set ( G_OBJECT (element), "profile",
                   KMS_RECORDING_PROFILE_WEBM_VIDEO_ONLY, NULL);
    GST_INFO ("Set WEBM VIDEO ONLY profile");
    break;

  case MediaProfileSpecType::WEBM_AUDIO_ONLY:
    g_object_set ( G_OBJECT (element), "profile",
                   KMS_RECORDING_PROFILE_WEBM_AUDIO_ONLY, NULL);
    GST_INFO ("Set WEBM AUDIO ONLY profile");
    break;

  case MediaProfileSpecType::MP4_VIDEO_ONLY:
    g_object_set ( G_OBJECT (element), "profile",
                   KMS_RECORDING_PROFILE_MP4_VIDEO_ONLY, NULL);
    GST_INFO ("Set MP4 VIDEO ONLY profile");
    break;

  case MediaProfileSpecType::MP4_AUDIO_ONLY:
    g_object_set ( G_OBJECT (element), "profile",
                   KMS_RECORDING_PROFILE_MP4_AUDIO_ONLY, NULL);
    GST_INFO ("Set MP4 AUDIO ONLY profile");
    break;

  case MediaProfileSpecType::KURENTO_SPLIT_RECORDER:
    if (!RecorderEndpointImpl::support_ksr) {
      throw KurentoException (MEDIA_OBJECT_ILLEGAL_PARAM_ERROR,
                              "Kurento Split Recorder not supported");
    }

    g_object_set ( G_OBJECT (element), "profile", KMS_RECORDING_PROFILE_KSR, NULL);
    GST_INFO ("Set KSR profile");
    break;
  }
}

static void
dispose_element (GstElement *element)
{
  GST_TRACE_OBJECT (element, "Disposing");

  gst_element_set_state (element, GST_STATE_NULL);
  g_object_unref (element);
}

static void
state_changed (GstElement *element, gint state, gpointer data)
{
  GST_TRACE_OBJECT (element, "State changed: %d", state);
  dispose_element (element);
}

RecorderEndpointImpl::~RecorderEndpointImpl()
{
  gint state = -1;

  g_object_get (getGstreamerElement(), "state", &state, NULL);

  if (state == 0 /* stop */) {
    dispose_element (getGstreamerElement() );
    return;
  }

  g_signal_connect (getGstreamerElement(), "state-changed",
                    G_CALLBACK (state_changed), NULL);

  stop();
}

void RecorderEndpointImpl::record ()
{
  start();
}

static void
setDeprecatedProperties (std::shared_ptr<EndpointStats> eStats)
{
  std::vector<std::shared_ptr<MediaLatencyStat>> inStats =
        eStats->getE2ELatency();

  for (unsigned i = 0; i < inStats.size(); i++) {
    if (inStats[i]->getName() == "sink_audio_default") {
      eStats->setAudioE2ELatency (inStats[i]->getAvg() );
    } else if (inStats[i]->getName() == "sink_video_default") {
      eStats->setVideoE2ELatency (inStats[i]->getAvg() );
    }
  }
}

void
RecorderEndpointImpl::collectEndpointStats (std::map
    <std::string, std::shared_ptr<Stats>>
    &statsReport, std::string id, const GstStructure *stats,
    double timestamp)
{
  std::shared_ptr<Stats> endpointStats;
  GstStructure *e2e_stats;

  std::vector<std::shared_ptr<MediaLatencyStat>> inputStats;
  std::vector<std::shared_ptr<MediaLatencyStat>> e2eStats;

  if (gst_structure_get (stats, "e2e-latencies", GST_TYPE_STRUCTURE,
                         &e2e_stats, NULL) ) {
    collectLatencyStats (e2eStats, e2e_stats);
    gst_structure_free (e2e_stats);
  }

  endpointStats = std::make_shared <EndpointStats> (id,
                  std::make_shared <StatsType> (StatsType::endpoint), timestamp,
                  0.0, 0.0, inputStats, 0.0, 0.0, e2eStats);

  setDeprecatedProperties (std::dynamic_pointer_cast <EndpointStats>
                           (endpointStats) );

  statsReport[id] = endpointStats;
}

void
RecorderEndpointImpl::fillStatsReport (std::map
                                       <std::string, std::shared_ptr<Stats>>
                                       &report, const GstStructure *stats, double timestamp)
{
  const GstStructure *e_stats;

  e_stats = kms_utils_get_structure_by_name (stats, KMS_MEDIA_ELEMENT_FIELD);

  if (e_stats != NULL) {
    collectEndpointStats (report, getId (), e_stats, timestamp);
  }

  UriEndpointImpl::fillStatsReport (report, stats, timestamp);
}

MediaObjectImpl *
RecorderEndpointImplFactory::createObject (const boost::property_tree::ptree
    &conf, std::shared_ptr<MediaPipeline>
    mediaPipeline, const std::string &uri,
    std::shared_ptr<MediaProfileSpecType> mediaProfile,
    bool stopOnEndOfStream) const
{
  return new RecorderEndpointImpl (conf, mediaPipeline, uri, mediaProfile,
                                   stopOnEndOfStream);
}

RecorderEndpointImpl::StaticConstructor RecorderEndpointImpl::staticConstructor;

RecorderEndpointImpl::StaticConstructor::StaticConstructor()
{
  RecorderEndpointImpl::support_ksr = check_support_for_ksr();

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
                           GST_DEFAULT_NAME);
}

} /* kurento */
