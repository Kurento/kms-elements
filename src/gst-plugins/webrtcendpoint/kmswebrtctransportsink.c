/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "kmswebrtctransportsink.h"
#include <commons/constants.h>

#define GST_DEFAULT_NAME "webrtctransportsink"
#define GST_CAT_DEFAULT kms_webrtc_transport_sink_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_webrtc_transport_sink_parent_class parent_class
G_DEFINE_TYPE (KmsWebrtcTransportSink, kms_webrtc_transport_sink, GST_TYPE_BIN);

#define FUNNEL_FACTORY_NAME "funnel"
#define SRTPENC_FACTORY_NAME "srtpenc"
#define DTLS_ENCODER_FACTORY_NAME "dtlsenc"


static GType 
get_type_from_factory_name (const gchar *factory_name)
{
  GstElementFactory *factory;
  GType type = 0;

  factory = gst_element_factory_find (factory_name)

  if (factory != NULL) {
    type = gst_element_factory_get_element_type (factory_name); 

    g_object_unref(factory);
  }
  return type;
}

static GElement*
kms_webrtc_transport_sink_get_element_in_dtlssrtpenc (KmsWebrtcTransportSink *self, const gchar *factory_name)
{
  GType type;

  type = get_type_from_factory_name (factory_name);
  if (type != 0) {
    return gst_bin_get_by_interface (GST_BIN(self->dtlssrtpenc), type);
  } else {
    GST_WARNING_OBJECT (self, "Factory %s not installed", factory_name);
    return NULL;
  }
}

static void
kms_webrtc_transport_sink_init (KmsWebrtcTransportSink * self)
{
  GstElement *dtls_encoder;

  self->dtlssrtpenc = gst_element_factory_make ("dtlssrtpenc", NULL);
  dtls_encoder = kms_webrtc_transport_sink_get_element_in_dtlssrtpenc (self, DTLS_ENCODER_FACTORY_NAME);
  if (dtls_encoder != NULL) {
    gst_element_set_locked_state (dtls_encoder, TRUE);
    g_object_unref (dtls_encoder);
  } else  {
    GST_WARNING_OBJECT (self, "Cannot get DTLS encoder");
  }
}

void
kms_webrtc_transport_sink_connect_elements (KmsWebrtcTransportSink *self)
{
  GstElement *funnel, *srtpenc;

  gst_bin_add_many (GST_BIN (self), self->dtlssrtpenc, self->sink, NULL);
  gst_element_link (self->dtlssrtpenc, self->sink);

  funnel = kms_webrtc_transport_sink_get_element_in_dtlssrtpenc (self, FUNNEL_FACTORY_NAME);
  if (funnel != NULL) {
    g_object_set (funnel, "forward-sticky-events-mode", 0 /* never */ , NULL);
    g_object_unref (funnel);
  } else {
    GST_WARNING ("Cannot get funnel with name %s", FUNNEL_NAME);
  }

  srtpenc = kms_webrtc_transport_sink_get_element_in_dtlssrtpenc (self, SRTPENC_FACTORY_NAME);
  if (srtpenc != NULL) {
    g_object_set (srtpenc, "allow-repeat-tx", TRUE, "replay-window-size",
        RTP_RTX_SIZE, NULL);
    g_object_unref (srtpenc);
  } else {
    GST_WARNING ("Cannot get srtpenc with name %s", SRTPENC_NAME);
  }

}

void
kms_webrtc_transport_sink_configure_default (KmsWebrtcTransportSink * self,
    KmsIceBaseAgent * agent, const char *stream_id, guint component_id)
{
  KmsWebrtcTransportSinkClass *klass =
      KMS_WEBRTC_TRANSPORT_SINK_CLASS (G_OBJECT_GET_CLASS (self));

  if (klass->configure == kms_webrtc_transport_sink_configure_default) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement 'configure'", G_OBJECT_CLASS_NAME (klass));
  }
}

void
kms_webrtc_transport_sink_set_dtls_is_client_default (KmsWebrtcTransportSink * self,
    gboolean is_client)
{
  g_object_set (G_OBJECT (self->dtlssrtpenc), "is-client", is_client, NULL);
  if (is_client) {
    GST_DEBUG_OBJECT(self, "Set as DTLS client (handshake initiator)");
  } else {
    GST_DEBUG_OBJECT(self, "Set as DTLS server (wait for handshake)");
  }
}

void
kms_webrtc_transport_sink_configure (KmsWebrtcTransportSink * self,
    KmsIceBaseAgent * agent, const char *stream_id, guint component_id)
{
  KmsWebrtcTransportSinkClass *klass =
      KMS_WEBRTC_TRANSPORT_SINK_CLASS (G_OBJECT_GET_CLASS (self));

  klass->configure (self, agent, stream_id, component_id);
}

void
kms_webrtc_transport_sink_set_dtls_is_client (KmsWebrtcTransportSink * self,
    gboolean is_client)
{
  KmsWebrtcTransportSinkClass *klass =
      KMS_WEBRTC_TRANSPORT_SINK_CLASS (G_OBJECT_GET_CLASS (self));

  klass->set_dtls_is_client (self, is_client);
}

static void
kms_webrtc_transport_sink_class_init (KmsWebrtcTransportSinkClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  klass->configure = kms_webrtc_transport_sink_configure_default;
  klass->set_dtls_is_client = kms_webrtc_transport_sink_set_dtls_is_client_default;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  gst_element_class_set_details_simple (gstelement_class,
      "WebrtcTransportSink",
      "Generic",
      "Base bin to manage WebRTC transport sink elements.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");
}

void 
kms_webrtc_transport_sink_start_dtls (KmsWebrtcTransportSink * self)
{
  GstElement *dtls_encoder;

  dtls_encoder = kms_webrtc_transport_sink_get_element_in_dtlssrtpenc (self, DTLS_ENCODER_FACTORY_NAME);
  if (dtls_encoder != NULL) {
    gst_element_set_locked_state (dtls_encoder, FALSE);
    gst_element_sync_state_with_parent (dtls_encoder);
    GST_DEBUG_OBJECT(self, "Starting DTLS");

    g_object_unref (dtls_encoder);
  } else  {
    GST_WARNING ("Cannot get DTLS encoder with name %s", DTLS_ENCODER_NAME);
  }
}


KmsWebrtcTransportSink *
kms_webrtc_transport_sink_new ()
{
  GObject *obj;

  obj = g_object_new (KMS_TYPE_WEBRTC_TRANSPORT_SINK, NULL);

  return KMS_WEBRTC_TRANSPORT_SINK (obj);
}
