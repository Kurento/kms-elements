/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
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

#include "kmswebrtcbundleconnection.h"
#include "kmswebrtctransport.h"
#include <commons/kmsutils.h>

#define GST_CAT_DEFAULT kmswebrtcbundleconnection
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "kmswebrtcbundleconnection"

#define KMS_WEBRTC_BUNDLE_CONNECTION_GET_PRIVATE(obj) ( \
  G_TYPE_INSTANCE_GET_PRIVATE (                         \
    (obj),                                              \
    KMS_TYPE_WEBRTC_BUNDLE_CONNECTION,                  \
    KmsWebRtcBundleConnectionPrivate                    \
  )                                                     \
)

enum
{
  PROP_0,
  PROP_ADDED,
  PROP_CONNECTED
};

struct _KmsWebRtcBundleConnectionPrivate
{
  KmsWebRtcTransport *tr;

  gboolean added;
  gboolean connected;
};

static void
kms_webrtc_bundle_rtp_connection_interface_init (KmsIRtpConnectionInterface *
    iface);

static void
    kms_webrtc_bundle_rtcp_mux_connection_interface_init
    (KmsIRtcpMuxConnectionInterface * iface);

static void
kms_webrtc_bundle_connection_interface_init (KmsIBundleConnectionInterface *
    iface);

G_DEFINE_TYPE_WITH_CODE (KmsWebRtcBundleConnection,
    kms_webrtc_bundle_connection, KMS_TYPE_WEBRTC_BASE_CONNECTION,
    G_IMPLEMENT_INTERFACE (KMS_TYPE_I_RTP_CONNECTION,
        kms_webrtc_bundle_rtp_connection_interface_init)
    G_IMPLEMENT_INTERFACE (KMS_TYPE_I_RTCP_MUX_CONNECTION,
        kms_webrtc_bundle_rtcp_mux_connection_interface_init)
    G_IMPLEMENT_INTERFACE (KMS_TYPE_I_BUNDLE_CONNECTION,
        kms_webrtc_bundle_connection_interface_init));

static void
    kms_webrtc_bundle_connection_set_certificate_pem_file
    (KmsWebRtcBaseConnection * base_conn, const gchar * pem)
{
  GST_WARNING_OBJECT (base_conn, "Deprectated (using erdtls)");
}

static gchar *
kms_webrtc_bundle_connection_get_certificate_pem (KmsWebRtcBaseConnection *
    base_conn)
{
  KmsWebRtcBundleConnection *self = KMS_WEBRTC_BUNDLE_CONNECTION (base_conn);
  gchar *pem;

  g_object_get (G_OBJECT (self->priv->tr->dtlssrtpdec), "pem", &pem, NULL);

  return pem;
}

static void
kms_webrtc_bundle_connection_add (KmsIRtpConnection * base_rtp_conn,
    GstBin * bin, gboolean active)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);
  KmsWebRtcBundleConnectionPrivate *priv = self->priv;
  KmsWebRtcTransport *tr = priv->tr;

  /* srcs */
  g_object_set (G_OBJECT (tr->dtlssrtpenc), "is-client", active, NULL);

  gst_bin_add_many (bin,
      g_object_ref (tr->nicesrc), g_object_ref (tr->dtlssrtpdec), NULL);
  gst_element_link (tr->nicesrc, tr->dtlssrtpdec);

  /* sinks */
  gst_bin_add_many (bin, g_object_ref (tr->dtlssrtpenc),
      g_object_ref (tr->nicesink), NULL);

  gst_element_link (tr->dtlssrtpenc, tr->nicesink);
}

static void
kms_webrtc_bundle_connection_src_sync_state_with_parent (KmsIRtpConnection *
    base_rtp_conn)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);
  KmsWebRtcTransport *tr = self->priv->tr;

  gst_element_sync_state_with_parent_target_state (tr->dtlssrtpdec);
  gst_element_sync_state_with_parent_target_state (tr->nicesrc);
}

static void
kms_webrtc_bundle_connection_sink_sync_state_with_parent (KmsIRtpConnection *
    base_rtp_conn)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);
  KmsWebRtcTransport *tr = self->priv->tr;

  gst_element_sync_state_with_parent_target_state (tr->nicesink);
  gst_element_sync_state_with_parent_target_state (tr->dtlssrtpenc);
}

static GstPad *
kms_webrtc_bundle_connection_request_rtp_sink (KmsIRtpConnection *
    base_rtp_conn)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);
  GstPad *pad;
  gchar *str;

  str = g_strdup_printf ("rtp_sink_%d",
      g_atomic_int_add (&self->priv->tr->rtp_id, 1));

  pad = gst_element_get_request_pad (self->priv->tr->dtlssrtpenc, str);
  g_free (str);

  return pad;
}

static GstPad *
kms_webrtc_bundle_connection_request_rtp_src (KmsIRtpConnection * base_rtp_conn)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);

  return gst_element_get_static_pad (self->priv->tr->dtlssrtpdec, "rtp_src");
}

static GstPad *
kms_webrtc_bundle_connection_request_rtcp_sink (KmsIRtpConnection *
    base_rtp_conn)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);
  GstPad *pad;
  gchar *str;

  str = g_strdup_printf ("rtcp_sink_%d",
      g_atomic_int_add (&self->priv->tr->rtcp_id, 1));

  pad = gst_element_get_request_pad (self->priv->tr->dtlssrtpenc, str);
  g_free (str);

  return pad;
}

static GstPad *
kms_webrtc_bundle_connection_request_rtcp_src (KmsIRtpConnection *
    base_rtp_conn)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);

  return gst_element_get_static_pad (self->priv->tr->dtlssrtpdec, "rtcp_src");
}

static GstPad *
kms_webrtc_bundle_connection_request_data_src (KmsIRtpConnection *
    base_rtp_conn)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);

  return gst_element_get_request_pad (self->priv->tr->dtlssrtpdec, "data_src");
}

static GstPad *
kms_webrtc_bundle_connection_request_data_sink (KmsIRtpConnection *
    base_rtp_conn)
{
  KmsWebRtcBundleConnection *self =
      KMS_WEBRTC_BUNDLE_CONNECTION (base_rtp_conn);

  return gst_element_get_request_pad (self->priv->tr->dtlssrtpenc, "data_sink");
}

static void
kms_webrtc_bundle_connection_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsWebRtcBundleConnection *self = KMS_WEBRTC_BUNDLE_CONNECTION (object);

  switch (prop_id) {
    case PROP_ADDED:
      self->priv->added = g_value_get_boolean (value);
      break;
    case PROP_CONNECTED:
      self->priv->connected = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
kms_webrtc_bundle_connection_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  KmsWebRtcBundleConnection *self = KMS_WEBRTC_BUNDLE_CONNECTION (object);

  switch (prop_id) {
    case PROP_ADDED:
      g_value_set_boolean (value, self->priv->added);
      break;
    case PROP_CONNECTED:
      g_value_set_boolean (value, self->priv->connected);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
connected_cb (GstElement * dtlssrtpenc, gpointer self)
{
  kms_i_rtp_connection_connected_signal (self);
}

KmsWebRtcBundleConnection *
kms_webrtc_bundle_connection_new (NiceAgent * agent, GMainContext * context,
    const gchar * name)
{
  GObject *obj;
  KmsWebRtcBaseConnection *base_conn;
  KmsWebRtcBundleConnection *conn;
  KmsWebRtcBundleConnectionPrivate *priv;

  obj = g_object_new (KMS_TYPE_WEBRTC_BUNDLE_CONNECTION, NULL);
  base_conn = KMS_WEBRTC_BASE_CONNECTION (obj);
  conn = KMS_WEBRTC_BUNDLE_CONNECTION (obj);
  priv = conn->priv;

  if (!kms_webrtc_base_connection_configure (base_conn, agent, name)) {
    g_object_unref (obj);
    return NULL;
  }

  priv->tr =
      kms_webrtc_transport_create (agent, base_conn->stream_id,
      NICE_COMPONENT_TYPE_RTP);

  if (priv->tr == NULL) {
    GST_ERROR_OBJECT (conn, "Cannot create connection");
    g_object_unref (obj);
    return NULL;
  }

  g_signal_connect (priv->tr->dtlssrtpenc, "on-key-set",
      G_CALLBACK (connected_cb), conn);

  nice_agent_attach_recv (agent, base_conn->stream_id,
      NICE_COMPONENT_TYPE_RTP, context, kms_webrtc_transport_nice_agent_recv_cb,
      NULL);

  return conn;
}

static void
kms_webrtc_bundle_connection_finalize (GObject * object)
{
  KmsWebRtcBundleConnection *self = KMS_WEBRTC_BUNDLE_CONNECTION (object);
  KmsWebRtcBundleConnectionPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "finalize");

  kms_webrtc_transport_destroy (priv->tr);

  /* chain up */
  G_OBJECT_CLASS (kms_webrtc_bundle_connection_parent_class)->finalize (object);
}

static void
kms_webrtc_bundle_connection_init (KmsWebRtcBundleConnection * self)
{
  self->priv = KMS_WEBRTC_BUNDLE_CONNECTION_GET_PRIVATE (self);
  self->priv->connected = FALSE;
}

static void
kms_webrtc_bundle_connection_class_init (KmsWebRtcBundleConnectionClass * klass)
{
  GObjectClass *gobject_class;
  KmsWebRtcBaseConnectionClass *base_conn_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = kms_webrtc_bundle_connection_finalize;
  gobject_class->set_property = kms_webrtc_bundle_connection_set_property;
  gobject_class->get_property = kms_webrtc_bundle_connection_get_property;

  base_conn_class = KMS_WEBRTC_BASE_CONNECTION_CLASS (klass);
  base_conn_class->set_certificate_pem_file =
      kms_webrtc_bundle_connection_set_certificate_pem_file;
  base_conn_class->get_certificate_pem =
      kms_webrtc_bundle_connection_get_certificate_pem;

  g_type_class_add_private (klass, sizeof (KmsWebRtcBundleConnectionPrivate));

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  g_object_class_override_property (gobject_class, PROP_ADDED, "added");
  g_object_class_override_property (gobject_class, PROP_CONNECTED, "connected");
}

static void
kms_webrtc_bundle_rtp_connection_interface_init (KmsIRtpConnectionInterface *
    iface)
{
  iface->add = kms_webrtc_bundle_connection_add;
  iface->src_sync_state_with_parent =
      kms_webrtc_bundle_connection_src_sync_state_with_parent;
  iface->sink_sync_state_with_parent =
      kms_webrtc_bundle_connection_sink_sync_state_with_parent;
  iface->request_rtp_sink = kms_webrtc_bundle_connection_request_rtp_sink;
  iface->request_rtp_src = kms_webrtc_bundle_connection_request_rtp_src;
  iface->request_rtcp_sink = kms_webrtc_bundle_connection_request_rtcp_sink;
  iface->request_rtcp_src = kms_webrtc_bundle_connection_request_rtcp_src;
  iface->request_data_src = kms_webrtc_bundle_connection_request_data_src;
  iface->request_data_sink = kms_webrtc_bundle_connection_request_data_sink;
}

static void
    kms_webrtc_bundle_rtcp_mux_connection_interface_init
    (KmsIRtcpMuxConnectionInterface * iface)
{
  /* Nothing to do */
}

static void
kms_webrtc_bundle_connection_interface_init (KmsIBundleConnectionInterface *
    iface)
{
  /* Nothing to do */
}
