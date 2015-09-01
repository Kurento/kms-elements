/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "kmswebrtcendpoint.h"
#include "kmswebrtcconnection.h"
#include "kmswebrtcrtcpmuxconnection.h"
#include "kmswebrtcbundleconnection.h"
#include "kmswebrtcsctpconnection.h"
#include <commons/kmsloop.h>
#include <commons/kmsutils.h>
#include <commons/sdp_utils.h>
#include <commons/kmsrefstruct.h>
#include <commons/sdpagent/kmssdprtpsavpfmediahandler.h>
#include <commons/sdpagent/kmssdpsctpmediahandler.h>
#include "kms-webrtc-marshal.h"

#include <gio/gio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <ftw.h>
#include <string.h>
#include <errno.h>

#include <gst/rtp/gstrtcpbuffer.h>

#define KMS_WEBRTC_DATA_CHANNEL_PPID_STRING 51

static void
kms_webrtc_endpoint_remote_sdp_add_ice_candidate (KmsWebrtcEndpoint * self,
    NiceCandidate * nice_cand, guint8 index);

static gboolean
kms_webrtc_endpoint_set_remote_ice_candidate (KmsWebrtcEndpoint * self,
    KmsIceCandidate * candidate, NiceCandidate * nice_cand);

#define PLUGIN_NAME "webrtcendpoint"

#define GST_CAT_DEFAULT kms_webrtc_endpoint_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_webrtc_endpoint_parent_class parent_class
G_DEFINE_TYPE (KmsWebrtcEndpoint, kms_webrtc_endpoint,
    KMS_TYPE_BASE_RTP_ENDPOINT);

#define KMS_WEBRTC_ENDPOINT_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (                 \
    (obj),                                      \
    KMS_TYPE_WEBRTC_ENDPOINT,                   \
    KmsWebrtcEndpointPrivate                    \
  )                                             \
)

/* TODO: default prop values */
enum
{
  PROP_0,
  PROP_CERTIFICATE_PEM_FILE,
  PROP_STUN_SERVER_IP,
  PROP_STUN_SERVER_PORT,
  PROP_TURN_URL,                /* user:password@address:port?transport=[udp|tcp|tls] */
  N_PROPERTIES
};

enum
{
  SIGNAL_ON_ICE_CANDIDATE,
  SIGNAL_ON_ICE_GATHERING_DONE,
  SIGNAL_ON_ICE_COMPONENT_STATE_CHANGED,
  SIGNAL_GATHER_CANDIDATES,
  SIGNAL_ADD_ICE_CANDIDATE,
  LAST_SIGNAL
};

static guint kms_webrtc_endpoint_signals[LAST_SIGNAL] = { 0 };

struct _KmsWebrtcEndpointPrivate
{
  KmsLoop *loop;
  GMainContext *context;
  gchar *certificate_pem_file;

  NiceAgent *agent;
  GSList *remote_candidates;

  gchar *turn_url;
  gchar *turn_user;
  gchar *turn_password;
  gchar *turn_address;
  guint turn_port;
  NiceRelayType turn_transport;
};

typedef struct _ConnectSCTPData
{
  KmsRefStruct ref;
  KmsWebrtcEndpoint *self;
  KmsIRtpConnection *conn;
  GstSDPMedia *media;
  gboolean connected;
} ConnectSCTPData;

static void
connect_sctp_data_destroy (ConnectSCTPData * data, GClosure * closure)
{
  gst_sdp_media_free (data->media);

  g_slice_free (ConnectSCTPData, data);
}

static ConnectSCTPData *
connect_sctp_data_new (KmsWebrtcEndpoint * self, GstSDPMedia * media,
    KmsIRtpConnection * conn)
{
  ConnectSCTPData *data;

  data = g_slice_new0 (ConnectSCTPData);

  kms_ref_struct_init (KMS_REF_STRUCT_CAST (data),
      (GDestroyNotify) connect_sctp_data_destroy);

  data->self = self;
  data->conn = conn;
  data->media = media;

  return data;
}

/* Media handler management begin */
static void
kms_webrtc_endpoint_create_media_handler (KmsBaseSdpEndpoint * base_sdp,
    const gchar * media, KmsSdpMediaHandler ** handler)
{
  if (g_strcmp0 (media, "audio") == 0 || g_strcmp0 (media, "video") == 0) {
    *handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  } else if (g_strcmp0 (media, "application") == 0) {
    *handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_sctp_media_handler_new ());
  }

  /* Chain up */
  KMS_BASE_SDP_ENDPOINT_CLASS
      (kms_webrtc_endpoint_parent_class)->create_media_handler (base_sdp, media,
      handler);
}

/* Media handler management end */

/* Connection management begin */
static KmsIRtpConnection *
kms_webrtc_endpoint_create_connection (KmsBaseRtpEndpoint * base_rtp_endpoint,
    SdpMediaConfig * mconf, const gchar * name)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (base_rtp_endpoint);
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  KmsWebRtcBaseConnection *conn;

  if (g_strcmp0 (gst_sdp_media_get_proto (media), "DTLS/SCTP") == 0) {
    GST_DEBUG_OBJECT (self, "Create SCTP connection");
    conn =
        KMS_WEBRTC_BASE_CONNECTION (kms_webrtc_sctp_connection_new (self->priv->
            agent, self->priv->context, name));
  } else {
    GST_DEBUG_OBJECT (self, "Create RTP connection");
    conn =
        KMS_WEBRTC_BASE_CONNECTION (kms_webrtc_connection_new (self->priv->
            agent, self->priv->context, name));
  }

  kms_webrtc_base_connection_set_certificate_pem_file (conn,
      self->priv->certificate_pem_file);

  return KMS_I_RTP_CONNECTION (conn);
}

static KmsIRtcpMuxConnection *
kms_webrtc_endpoint_create_rtcp_mux_connection (KmsBaseRtpEndpoint *
    base_rtp_endpoint, const gchar * name)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (base_rtp_endpoint);
  KmsWebRtcRtcpMuxConnection *conn;

  conn =
      kms_webrtc_rtcp_mux_connection_new (self->priv->agent,
      self->priv->context, name);
  kms_webrtc_base_connection_set_certificate_pem_file
      (KMS_WEBRTC_BASE_CONNECTION (conn), self->priv->certificate_pem_file);

  return KMS_I_RTCP_MUX_CONNECTION (conn);
}

static KmsIBundleConnection *
kms_webrtc_endpoint_create_bundle_connection (KmsBaseRtpEndpoint *
    base_rtp_endpoint, const gchar * name)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (base_rtp_endpoint);
  KmsWebRtcBundleConnection *conn;

  conn =
      kms_webrtc_bundle_connection_new (self->priv->agent, self->priv->context,
      name);
  kms_webrtc_base_connection_set_certificate_pem_file
      (KMS_WEBRTC_BASE_CONNECTION (conn), self->priv->certificate_pem_file);

  return KMS_I_BUNDLE_CONNECTION (conn);
}

static KmsWebRtcBaseConnection *
kms_webrtc_endpoint_media_get_connection (KmsWebrtcEndpoint * self,
    SdpMediaConfig * mconf)
{
  KmsBaseRtpEndpoint *base_rtp = KMS_BASE_RTP_ENDPOINT (self);
  KmsIRtpConnection *conn;

  conn = kms_base_rtp_endpoint_get_connection (base_rtp, mconf);
  if (conn == NULL) {
    return NULL;
  }

  return KMS_WEBRTC_BASE_CONNECTION (conn);
}

static guint
kms_webrtc_endpoint_media_get_stream_id (KmsWebrtcEndpoint * self,
    SdpMediaConfig * mconf)
{
  KmsWebRtcBaseConnection *conn;

  conn = kms_webrtc_endpoint_media_get_connection (self, mconf);
  if (conn == NULL) {
    return -1;
  }

  return conn->stream_id;
}

/* Connection management end */

static gchar *
generate_fingerprint_from_pem (const gchar * pem)
{
  guint i;
  gchar *line;
  guchar *der, *tmp;
  gchar **lines;
  gint state = 0;
  guint save = 0;
  gsize der_length = 0;
  GChecksum *checksum;
  guint8 *digest;
  gsize digest_length;
  GString *fingerprint;
  gchar *ret;

  der = tmp = g_new0 (guchar, (strlen (pem) / 4) * 3 + 3);
  lines = g_strsplit (pem, "\n", 0);
  for (i = 0, line = lines[i]; line; line = lines[++i]) {
    if (line[0] && !g_str_has_prefix (line, "-----"))
      tmp += g_base64_decode_step (line, strlen (line), tmp, &state, &save);
  }
  der_length = tmp - der;
  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  digest_length = g_checksum_type_get_length (G_CHECKSUM_SHA256);
  digest = g_new (guint8, digest_length);
  g_checksum_update (checksum, der, der_length);
  g_checksum_get_digest (checksum, digest, &digest_length);
  fingerprint = g_string_new (NULL);
  for (i = 0; i < digest_length; i++) {
    if (i)
      g_string_append (fingerprint, ":");
    g_string_append_printf (fingerprint, "%02X", digest[i]);
  }
  ret = g_string_free (fingerprint, FALSE);

  g_free (digest);
  g_checksum_free (checksum);
  g_free (der);
  g_strfreev (lines);

  return ret;
}

static gchar *
kms_webrtc_endpoint_generate_fingerprint_sdp_attr (KmsWebrtcEndpoint * self,
    SdpMediaConfig * mconf)
{
  gchar *fp, *ret;
  KmsBaseRtpEndpoint *base_rtp_endpoint = KMS_BASE_RTP_ENDPOINT (self);

  KmsWebRtcBaseConnection *conn =
      KMS_WEBRTC_BASE_CONNECTION (kms_base_rtp_endpoint_get_connection
      (base_rtp_endpoint, mconf));
  gchar *pem = kms_webrtc_base_connection_get_certificate_pem (conn);

  fp = generate_fingerprint_from_pem (pem);
  g_free (pem);

  if (fp == NULL) {
    return NULL;
  }

  /* TODO: store fingerprint to reuse it for each media */
  ret = g_strconcat ("sha-256 ", fp, NULL);
  g_free (fp);

  return ret;
}

/* Set Transport begin */

static gboolean
kms_webrtc_endpoint_sdp_media_add_default_info (KmsWebrtcEndpoint * self,
    SdpMediaConfig * mconf, gboolean use_ipv6)
{
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  NiceAgent *agent = self->priv->agent;
  guint stream_id;
  NiceCandidate *rtp_default_candidate, *rtcp_default_candidate;
  gchar rtp_addr[NICE_ADDRESS_STRING_LEN + 1];
  gchar rtcp_addr[NICE_ADDRESS_STRING_LEN + 1];
  const gchar *rtp_addr_type, *rtcp_addr_type;
  gboolean rtp_is_ipv6, rtcp_is_ipv6;
  guint rtp_port, rtcp_port;
  guint conn_len, c;
  gchar *str;
  guint attr_len, i;

  stream_id = kms_webrtc_endpoint_media_get_stream_id (self, mconf);
  if (stream_id == -1) {
    return FALSE;
  }

  rtp_default_candidate =
      nice_agent_get_default_local_candidate (agent, stream_id,
      NICE_COMPONENT_TYPE_RTP);

  if (kms_sdp_media_config_is_rtcp_mux (mconf) ||
      kms_sdp_media_config_get_group (mconf) != NULL) {
    rtcp_default_candidate =
        nice_agent_get_default_local_candidate (agent, stream_id,
        NICE_COMPONENT_TYPE_RTP);
  } else {
    rtcp_default_candidate =
        nice_agent_get_default_local_candidate (agent, stream_id,
        NICE_COMPONENT_TYPE_RTCP);
  }

  if (rtcp_default_candidate == NULL || rtcp_default_candidate == NULL) {
    GST_WARNING_OBJECT (self,
        "Error getting ICE candidates. Network can be unavailable.");
    return FALSE;
  }

  nice_address_to_string (&rtp_default_candidate->addr, rtp_addr);
  rtp_port = nice_address_get_port (&rtp_default_candidate->addr);
  rtp_is_ipv6 = nice_address_ip_version (&rtp_default_candidate->addr) == IPV6;
  nice_candidate_free (rtp_default_candidate);

  nice_address_to_string (&rtcp_default_candidate->addr, rtcp_addr);
  rtcp_port = nice_address_get_port (&rtcp_default_candidate->addr);
  rtcp_is_ipv6 =
      nice_address_ip_version (&rtcp_default_candidate->addr) == IPV6;
  nice_candidate_free (rtcp_default_candidate);

  rtp_addr_type = rtp_is_ipv6 ? "IP6" : "IP4";
  rtcp_addr_type = rtcp_is_ipv6 ? "IP6" : "IP4";

  if (use_ipv6 != rtp_is_ipv6) {
    GST_WARNING_OBJECT (self, "No valid rtp address type: %s", rtp_addr_type);
    return FALSE;
  }

  media->port = rtp_port;
  conn_len = gst_sdp_media_connections_len (media);
  for (c = 0; c < conn_len; c++) {
    gst_sdp_media_remove_connection (media, c);
  }
  gst_sdp_media_add_connection (media, "IN", rtp_addr_type, rtp_addr, 0, 0);

  attr_len = gst_sdp_media_attributes_len (media);
  for (i = 0; i < attr_len; i++) {
    const GstSDPAttribute *attr = gst_sdp_media_get_attribute (media, i);

    if (g_strcmp0 (attr->key, "rtcp") == 0) {
      str =
          g_strdup_printf ("%d IN %s %s", rtcp_port, rtcp_addr_type, rtcp_addr);
      gst_sdp_attribute_clear ((GstSDPAttribute *) attr);
      gst_sdp_attribute_set ((GstSDPAttribute *) attr, "rtcp", str);
      g_free (str);
    }
  }

  return TRUE;
}

static void
kms_webrtc_endpoint_set_relay_info (KmsWebrtcEndpoint * self,
    KmsWebRtcBaseConnection * conn)
{
  KmsWebrtcEndpointPrivate *priv = self->priv;

  if (priv->turn_address == NULL) {
    return;
  }

  kms_webrtc_base_connection_set_relay_info (conn, priv->turn_address,
      priv->turn_port, priv->turn_user, priv->turn_password,
      priv->turn_transport);
}

static gboolean
kms_webrtc_endpoint_local_sdp_add_default_info (KmsWebrtcEndpoint * self)
{
  KmsBaseSdpEndpoint *base_sdp_ep = KMS_BASE_SDP_ENDPOINT (self);
  SdpMessageContext *local_ctx =
      kms_base_sdp_endpoint_get_local_sdp_ctx (base_sdp_ep);
  const GstSDPMessage *sdp =
      kms_sdp_message_context_get_sdp_message (local_ctx);
  const GSList *item = kms_sdp_message_context_get_medias (local_ctx);
  gboolean use_ipv6;
  GstSDPConnection *conn;

  conn = (GstSDPConnection *) gst_sdp_message_get_connection (sdp);
  gst_sdp_connection_clear (conn);
  g_object_get (base_sdp_ep, "use-ipv6", &use_ipv6, NULL);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *mconf = item->data;

    if (kms_sdp_media_config_is_inactive (mconf)) {
      gint mid = kms_sdp_media_config_get_id (mconf);

      GST_DEBUG_OBJECT (self, "Media (id=%d) inactive", mid);
      continue;
    }

    if (!kms_webrtc_endpoint_sdp_media_add_default_info (self, mconf, use_ipv6)) {
      return FALSE;
    }
  }

  return TRUE;
}

/* Set Transport end */

/* Configure media SDP begin */
static gboolean
kms_webrtc_endpoint_set_ice_credentials (KmsWebrtcEndpoint * self,
    SdpMediaConfig * mconf)
{
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  KmsWebRtcBaseConnection *conn;
  gchar *ufrag, *pwd;

  conn = kms_webrtc_endpoint_media_get_connection (self, mconf);
  if (conn == NULL) {
    return FALSE;
  }

  nice_agent_get_local_credentials (self->priv->agent, conn->stream_id,
      &ufrag, &pwd);
  gst_sdp_media_add_attribute (media, SDP_ICE_UFRAG_ATTR, ufrag);
  g_free (ufrag);
  gst_sdp_media_add_attribute (media, SDP_ICE_PWD_ATTR, pwd);
  g_free (pwd);

  return TRUE;
}

static gboolean
kms_webrtc_endpoint_set_crypto_info (KmsWebrtcEndpoint * self,
    SdpMediaConfig * mconf)
{
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  gchar *fingerprint;

  /* Crypto info */
  fingerprint = kms_webrtc_endpoint_generate_fingerprint_sdp_attr (self, mconf);
  if (fingerprint == NULL) {
    return FALSE;
  }

  gst_sdp_media_add_attribute (media, "fingerprint", fingerprint);
  g_free (fingerprint);

  return TRUE;
}

static gboolean
kms_webrtc_endpoint_configure_media (KmsBaseSdpEndpoint *
    base_sdp_endpoint, SdpMediaConfig * mconf)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (base_sdp_endpoint);
  gboolean ret;

  /* Chain up */
  ret = KMS_BASE_SDP_ENDPOINT_CLASS
      (kms_webrtc_endpoint_parent_class)->configure_media (base_sdp_endpoint,
      mconf);
  if (ret == FALSE) {
    return FALSE;
  }

  if (!kms_webrtc_endpoint_set_ice_credentials (self, mconf)) {
    return FALSE;
  }

  return kms_webrtc_endpoint_set_crypto_info (self, mconf);
}

/* Configure media SDP end */

static gboolean
get_port_from_string (const gchar * str, gint * _ret)
{
  gchar *endptr;
  gint64 val;

  errno = 0;                    /* To distinguish success/failure after call */
  val = g_ascii_strtoll (str, &endptr, 0);
  if ((errno == ERANGE && (val == G_MAXINT64 || val == G_MININT64))
      || (errno == EINVAL && val == 0)) {
    return FALSE;
  }

  if (str == endptr) {
    /* Nothing parsed from the string */
    return FALSE;
  }

  if (val > G_MAXINT32 || val < G_MININT32) {
    /* Value ut of int 32 range */
    return FALSE;
  }

  if (_ret != NULL) {
    *_ret = val;
  }

  return TRUE;
}

static guint
get_sctp_association_id ()
{
  static guint assoc_id = 0;

  return g_atomic_int_add (&assoc_id, 1);
}

static gboolean
configure_sctp_elements (KmsWebrtcEndpoint * self, GstSDPMedia * media,
    GstElement * sctpdec, GstElement * sctpenc)
{
  const gchar *sctpmap_attr = NULL;
  guint i, id, len;
  gint port = -1;

  id = get_sctp_association_id ();
  g_object_set (sctpdec, "sctp-association-id", id, NULL);
  g_object_set (sctpenc, "sctp-association-id", id, "use-sock-stream", TRUE,
      NULL);

  len = gst_sdp_media_formats_len (media);
  if (len < 0) {
    GST_WARNING_OBJECT (self, "No SCTP format");
    return FALSE;
  }

  if (len > 1) {
    GST_INFO_OBJECT (self,
        "Only one SCTP link is supported over the same DTLS connection");
  }

  for (i = 0; i < len; i++) {
    const gchar *port_str;
    gchar **attrs;

    port_str = gst_sdp_media_get_format (media, 0);
    sctpmap_attr = sdp_utils_get_attr_map_value (media, "sctpmap", port_str);

    attrs = g_strsplit (sctpmap_attr, " ", 0);
    if (g_strcmp0 (attrs[1], "webrtc-datachannel") != 0) {
      g_strfreev (attrs);
      continue;
    }

    if (get_port_from_string (port_str, &port)) {
      g_strfreev (attrs);
      break;
    }

    g_strfreev (attrs);
  }

  if (port < 0) {
    GST_ERROR_OBJECT (self, "SCTP can not be configured");
    return FALSE;
  }

  g_object_set (sctpdec, "local-sctp-port", port, NULL);
  g_object_set (sctpenc, "remote-sctp-port", port, NULL);

  return TRUE;
}

static void
kms_webrtc_endpoint_add_sink_data (KmsWebrtcEndpoint * self,
    GstElement * sctpenc, guint sctp_stream_id)
{
  GstPadTemplate *pad_template;
  GstCaps *caps;
  GstPad *sinkpad;
  gchar *pad_name;

  GST_DEBUG_OBJECT (self, "Create sink data pad for stream %d", sctp_stream_id);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (sctpenc),
      "sink_%u");

  caps =
      gst_caps_new_simple ("application/data", "ordered", G_TYPE_BOOLEAN, TRUE,
      "ppid", G_TYPE_UINT, KMS_WEBRTC_DATA_CHANNEL_PPID_STRING,
      "partially-reliability", G_TYPE_STRING, "none", NULL);

  pad_name = g_strdup_printf ("sink_%u", sctp_stream_id);
  sinkpad = gst_element_request_pad (sctpenc, pad_template, pad_name, caps);
  gst_caps_unref (caps);
  g_free (pad_name);

  kms_element_connect_sink_target (KMS_ELEMENT (self), sinkpad,
      KMS_ELEMENT_PAD_TYPE_DATA);

  g_object_unref (sinkpad);
}

static void
kms_webrtc_endpoint_src_data_pad_added (GstElement * sctpdec, GstPad * pad,
    GstElement * sctpenc)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (GST_ELEMENT_PARENT (sctpdec));
  GstElement *data_tee;
  GstPad *sinkpad = NULL;
  guint sctp_stream_id;
  gchar *name;

  data_tee = kms_element_get_data_tee (KMS_ELEMENT (self));
  sinkpad = gst_element_get_static_pad (data_tee, "sink");

  if (GST_PAD_IS_LINKED (sinkpad)) {
    GST_WARNING_OBJECT (self, "Only 1 stream is supported per data channel");
    goto end;
  }

  if (gst_pad_link (pad, sinkpad) == GST_PAD_LINK_OK) {
    GST_DEBUG_OBJECT (self, "New data pad: %" GST_PTR_FORMAT " linked to %"
        GST_PTR_FORMAT, pad, data_tee);
  } else {
    GST_ERROR_OBJECT (self, "Can not link data pad %" GST_PTR_FORMAT, pad);
  }

  name = gst_pad_get_name (pad);
  sscanf (name, "src_%u", &sctp_stream_id);
  g_free (name);

  kms_webrtc_endpoint_add_sink_data (self, sctpenc, sctp_stream_id);

end:
  g_clear_object (&sinkpad);
}

static void
kms_webrtc_endpoint_sctp_association_established (GstElement * sctpenc,
    gboolean connected, KmsWebrtcEndpoint * self)
{
  if (!connected) {
    GST_WARNING_OBJECT (self, "Disconnected SCTP association %" GST_PTR_FORMAT,
        sctpenc);
  } else {
    GST_DEBUG_OBJECT (self, "SCTP association established");
  }
}

static void
kms_webrtc_endpoint_connect_sctp_elements (KmsWebrtcEndpoint * self,
    GstSDPMedia * media, KmsIRtpConnection * conn)
{
  GstElement *sctpdec = NULL, *sctpenc = NULL;
  GstPad *srcpad = NULL, *sinkpad = NULL, *tmppad;

  sctpdec = gst_element_factory_make ("sctpdec", NULL);
  if (sctpdec == NULL) {
    GST_WARNING_OBJECT (self, "Can not create sctpdec element");
    return;
  }

  sctpenc = gst_element_factory_make ("sctpenc", NULL);
  if (sctpenc == NULL) {
    GST_WARNING_OBJECT (self, "Can not create sctpenc element");
    goto error;
  }

  sinkpad = kms_i_rtp_connection_request_data_sink (conn);

  if (sinkpad == NULL) {
    GST_ERROR_OBJECT (self, "Can not get data sink pad");
    goto error;
  }

  srcpad = kms_i_rtp_connection_request_data_src (conn);
  if (srcpad == NULL) {
    GST_ERROR_OBJECT (self, "Can not get data src pad");
    goto error;
  }

  if (!configure_sctp_elements (self, media, sctpdec, sctpenc)) {
    goto error;
  }

  g_signal_connect (sctpdec, "pad-added",
      G_CALLBACK (kms_webrtc_endpoint_src_data_pad_added), sctpenc);
  g_signal_connect (sctpenc, "sctp-association-established",
      G_CALLBACK (kms_webrtc_endpoint_sctp_association_established), self);

  gst_bin_add_many (GST_BIN (self), sctpdec, sctpenc, NULL);

  tmppad = gst_element_get_static_pad (sctpdec, "sink");
  gst_pad_link (srcpad, tmppad);
  g_object_unref (tmppad);

  tmppad = gst_element_get_static_pad (sctpenc, "src");
  gst_pad_link (tmppad, sinkpad);
  g_object_unref (tmppad);

  g_object_unref (sinkpad);
  g_object_unref (srcpad);

  gst_element_sync_state_with_parent_target_state (sctpdec);
  gst_element_sync_state_with_parent_target_state (sctpenc);

  return;

error:
  GST_ERROR_OBJECT (self, "Rtc data channels are not supported");

  g_clear_object (&sctpdec);
  g_clear_object (&sctpenc);
  g_clear_object (&sinkpad);
  g_clear_object (&srcpad);
}

static void
kms_webrtc_endpoint_connect_sctp_elements_cb (KmsIRtpConnection * conn,
    ConnectSCTPData * data)
{
  if (g_atomic_int_compare_and_exchange (&data->connected, FALSE, TRUE)) {
    kms_webrtc_endpoint_connect_sctp_elements (data->self, data->media,
        data->conn);
  } else {
    GST_WARNING_OBJECT (data->self, "SCTP elements already configured");
  }

  /* DTLS is already connected, so we do not need to be attached to this */
  /* signal any more. We can free the tmp data without waiting for the   */
  /* object to be realeased. (Early release) */
  g_signal_handlers_disconnect_by_data (data->conn, data);
}

static void
kms_webrtc_endpoint_support_sctp_stream (KmsWebrtcEndpoint * self,
    SdpMediaConfig * mconf)
{
  gboolean connected = FALSE;
  KmsIRtpConnection *conn;
  ConnectSCTPData *data;
  GstSDPMedia *media;
  gulong handler_id = 0;

  conn = kms_base_rtp_endpoint_get_connection (KMS_BASE_RTP_ENDPOINT (self),
      mconf);

  if (conn == NULL) {
    return;
  }

  gst_sdp_media_copy (kms_sdp_media_config_get_sdp_media (mconf), &media);
  data = connect_sctp_data_new (self, media, conn);

  handler_id = g_signal_connect_data (conn, "connected",
      G_CALLBACK (kms_webrtc_endpoint_connect_sctp_elements_cb),
      kms_ref_struct_ref (KMS_REF_STRUCT_CAST (data)),
      (GClosureNotify) kms_ref_struct_unref, 0);

  g_object_get (conn, "connected", &connected, NULL);
  if (connected && g_atomic_int_compare_and_exchange (&data->connected, FALSE,
          TRUE)) {
    if (handler_id) {
      g_signal_handler_disconnect (conn, handler_id);
    }
    kms_webrtc_endpoint_connect_sctp_elements (self,
        kms_sdp_media_config_get_sdp_media (mconf), conn);
  } else {
    GST_DEBUG_OBJECT (self, "SCTP: waiting for DTLS layer to be established");
  }

  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (data));
}

static void
kms_webrtc_endpoint_connect_input_elements (KmsBaseSdpEndpoint *
    base_sdp_endpoint, SdpMessageContext * negotiated_ctx)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (base_sdp_endpoint);
  const GSList *item = kms_sdp_message_context_get_medias (negotiated_ctx);

  /* Chain up */
  KMS_BASE_SDP_ENDPOINT_CLASS
      (kms_webrtc_endpoint_parent_class)->connect_input_elements
      (base_sdp_endpoint, negotiated_ctx);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *mconf = item->data;
    GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
    const gchar *media_str = gst_sdp_media_get_media (media);

    if (gst_sdp_media_get_port (media) == 0) {
      /* Media not supported */
      GST_DEBUG_OBJECT (base_sdp_endpoint, "Media not supported: %s",
          media_str);
      continue;
    }

    if (g_strcmp0 (media_str, "application") == 0 &&
        g_strcmp0 (gst_sdp_media_get_proto (media), "DTLS/SCTP") == 0) {
      kms_webrtc_endpoint_support_sctp_stream (self, mconf);
    }
  }
}

/* Start Transport begin */
static void
gst_media_add_remote_candidates (SdpMediaConfig * mconf,
    KmsWebRtcBaseConnection * conn,
    const gchar * msg_ufrag, const gchar * msg_pwd)
{
  const GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  NiceAgent *agent = conn->agent;
  guint stream_id = conn->stream_id;
  const gchar *ufrag, *pwd;
  guint len, i;

  ufrag = gst_sdp_media_get_attribute_val (media, SDP_ICE_UFRAG_ATTR);
  pwd = gst_sdp_media_get_attribute_val (media, SDP_ICE_PWD_ATTR);
  if (!nice_agent_set_remote_credentials (agent, stream_id, ufrag, pwd)) {
    GST_WARNING ("Cannot set remote media credentials (%s, %s).", ufrag, pwd);
    if (!nice_agent_set_remote_credentials (agent, stream_id, msg_ufrag,
            msg_pwd)) {
      GST_WARNING ("Cannot set remote message credentials (%s, %s).", ufrag,
          pwd);
      return;
    } else {
      GST_DEBUG ("Set remote message credentials OK (%s, %s).", ufrag, pwd);
    }
  } else {
    GST_DEBUG ("Set remote media credentials OK (%s, %s).", ufrag, pwd);
  }

  len = gst_sdp_media_attributes_len (media);
  for (i = 0; i < len; i++) {
    const GstSDPAttribute *attr;
    NiceCandidate *cand;

    attr = gst_sdp_media_get_attribute (media, i);
    if (g_strcmp0 (SDP_CANDIDATE_ATTR, attr->key) != 0) {
      continue;
    }

    kms_ice_candidate_create_nice_from_str (attr->value, &cand);
    if (cand != NULL) {
      GSList *candidates = g_slist_append (NULL, cand);

      if (nice_agent_set_remote_candidates (agent, stream_id,
              cand->component_id, candidates) < 0) {
        GST_WARNING ("Cannot add candidate: '%s'in stream_id: %d.", attr->value,
            stream_id);
      } else {
        GST_TRACE ("Candidate added: '%s' in stream_id: %d.", attr->value,
            stream_id);
      }
      g_slist_free (candidates);
      nice_candidate_free (cand);
    }
  }
}

static void
kms_webrtc_endpoint_remote_sdp_add_stored_ice_candidates (gpointer data,
    gpointer user_data)
{
  KmsIceCandidate *candidate = data;
  KmsWebrtcEndpoint *self = user_data;
  NiceCandidate *nice_cand;
  guint8 index;

  kms_ice_candidate_create_nice (candidate, &nice_cand);
  if (nice_cand == NULL) {
    return;
  }

  index = kms_ice_candidate_get_sdp_m_line_index (candidate);
  kms_webrtc_endpoint_remote_sdp_add_ice_candidate (self, nice_cand, index);
  nice_candidate_free (nice_cand);
}

static void
kms_webrtc_endpoint_add_stored_ice_candidates (KmsWebrtcEndpoint * self)
{
  guint i;
  guint len = g_slist_length (self->priv->remote_candidates);

  for (i = 0; i < len; i++) {
    KmsIceCandidate *candidate =
        g_slist_nth_data (self->priv->remote_candidates, i);
    NiceCandidate *nice_cand;

    kms_ice_candidate_create_nice (candidate, &nice_cand);
    if (nice_cand == NULL) {
      return;
    }

    if (!kms_webrtc_endpoint_set_remote_ice_candidate (self, candidate,
            nice_cand)) {
      nice_candidate_free (nice_cand);
      return;
    }
    nice_candidate_free (nice_cand);
  }
}

static gboolean
kms_webrtc_endpoint_add_connection (KmsBaseRtpEndpoint * self,
    SdpMediaConfig * mconf, gboolean offerer)
{
  gboolean connected, active;
  KmsIRtpConnection *conn;

  conn = kms_base_rtp_endpoint_get_connection (self, mconf);
  if (conn == NULL) {
    GST_ERROR_OBJECT (self, "No connection created");
    return FALSE;
  }

  g_object_get (conn, "added", &connected, NULL);
  if (connected) {
    GST_DEBUG_OBJECT (self, "Conn already added");
    return TRUE;
  }

  active =
      sdp_utils_media_is_active (kms_sdp_media_config_get_sdp_media (mconf),
      offerer);

  kms_i_rtp_connection_add (conn, GST_BIN (self), active);
  kms_i_rtp_connection_sink_sync_state_with_parent (conn);
  kms_i_rtp_connection_src_sync_state_with_parent (conn);

  return TRUE;
}

static gboolean
kms_webrtc_endpoint_configure_connection (KmsBaseRtpEndpoint * self,
    SdpMediaConfig * neg_mconf, SdpMediaConfig * remote_mconf, gboolean offerer)
{
  GstSDPMedia *neg_media = kms_sdp_media_config_get_sdp_media (neg_mconf);
  const gchar *neg_proto_str = gst_sdp_media_get_proto (neg_media);
  GstSDPMedia *remote_media = kms_sdp_media_config_get_sdp_media (remote_mconf);
  const gchar *remote_proto_str = gst_sdp_media_get_proto (remote_media);

  if (g_strcmp0 (neg_proto_str, remote_proto_str) != 0) {
    GST_WARNING_OBJECT (self,
        "Negotiated proto ('%s') not matching with remote proto ('%s')",
        neg_proto_str, remote_proto_str);
    return FALSE;
  }

  if (g_strcmp0 (neg_proto_str, "DTLS/SCTP") != 0) {
    return FALSE;
  }

  kms_webrtc_endpoint_add_connection (self, neg_mconf, offerer);

  return TRUE;
}

static void
kms_webrtc_endpoint_configure_connections (KmsBaseSdpEndpoint *
    base_sdp_endpoint, gboolean offerer)
{
  KmsBaseRtpEndpoint *self = KMS_BASE_RTP_ENDPOINT (base_sdp_endpoint);
  SdpMessageContext *neg_ctx =
      kms_base_sdp_endpoint_get_negotiated_sdp_ctx (base_sdp_endpoint);
  SdpMessageContext *remote_ctx =
      kms_base_sdp_endpoint_get_remote_sdp_ctx (base_sdp_endpoint);
  GSList *item = kms_sdp_message_context_get_medias (neg_ctx);
  GSList *remote_media_list = kms_sdp_message_context_get_medias (remote_ctx);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *neg_mconf = item->data;
    gint mid = kms_sdp_media_config_get_id (neg_mconf);
    SdpMediaConfig *remote_mconf;

    if (kms_sdp_media_config_is_inactive (neg_mconf)) {
      GST_DEBUG_OBJECT (self, "Media (id=%d) inactive", mid);
      continue;
    }

    remote_mconf = g_slist_nth_data (remote_media_list, mid);
    if (remote_mconf == NULL) {
      GST_WARNING_OBJECT (self, "Media (id=%d) is not in the remote SDP", mid);
      continue;
    }

    kms_webrtc_endpoint_configure_connection (self, neg_mconf, remote_mconf,
        offerer);
  }
}

static void
kms_webrtc_endpoint_start_transport_send (KmsBaseSdpEndpoint *
    base_sdp_endpoint, gboolean offerer)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (base_sdp_endpoint);
  SdpMessageContext *remote_ctx =
      kms_base_sdp_endpoint_get_remote_sdp_ctx (base_sdp_endpoint);
  const GstSDPMessage *sdp =
      kms_sdp_message_context_get_sdp_message (remote_ctx);
  SdpMessageContext *neg_ctx =
      kms_base_sdp_endpoint_get_negotiated_sdp_ctx (base_sdp_endpoint);
  const GSList *item = kms_sdp_message_context_get_medias (neg_ctx);
  GSList *remote_media_list = kms_sdp_message_context_get_medias (remote_ctx);
  const gchar *ufrag, *pwd;

  /*  [rfc5245#section-5.2]
   *  The agent that generated the offer which
   *  started the ICE processing MUST take the controlling role, and the
   *  other MUST take the controlled role.
   */
  g_object_set (self->priv->agent, "controlling-mode", offerer, NULL);

  /* Chain up */
  KMS_BASE_SDP_ENDPOINT_CLASS
      (kms_webrtc_endpoint_parent_class)->start_transport_send
      (base_sdp_endpoint, offerer);

  /* Configure specific webrtc connection such as SCTP if negotiated */
  kms_webrtc_endpoint_configure_connections (base_sdp_endpoint, offerer);

  ufrag = gst_sdp_message_get_attribute_val (sdp, SDP_ICE_UFRAG_ATTR);
  pwd = gst_sdp_message_get_attribute_val (sdp, SDP_ICE_PWD_ATTR);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *neg_mconf = item->data;
    gint mid = kms_sdp_media_config_get_id (neg_mconf);
    SdpMediaConfig *remote_mconf;
    KmsWebRtcBaseConnection *conn;

    if (kms_sdp_media_config_is_inactive (neg_mconf)) {
      GST_DEBUG_OBJECT (self, "Media (id=%d) inactive", mid);
      continue;
    }

    conn = kms_webrtc_endpoint_media_get_connection (self, neg_mconf);
    if (conn == NULL) {
      continue;
    }

    remote_mconf = g_slist_nth_data (remote_media_list, mid);
    if (remote_mconf == NULL) {
      GST_WARNING_OBJECT (self, "Media (id=%d) is not in the remote SDP", mid);
      continue;
    }
    gst_media_add_remote_candidates (remote_mconf, conn, ufrag, pwd);
  }

  kms_webrtc_endpoint_add_stored_ice_candidates (self);

  g_slist_foreach (self->priv->remote_candidates,
      kms_webrtc_endpoint_remote_sdp_add_stored_ice_candidates, self);
}

/* Start Transport end */

static void
kms_webrtc_endpoint_component_state_change (NiceAgent * agent, guint stream_id,
    guint component_id, NiceComponentState state, KmsWebrtcEndpoint * self)
{
  GST_DEBUG_OBJECT (self, "stream_id: %d, component_id: %d, state: %s",
      stream_id, component_id, nice_component_state_to_string (state));

  g_signal_emit (G_OBJECT (self),
      kms_webrtc_endpoint_signals[SIGNAL_ON_ICE_COMPONENT_STATE_CHANGED], 0,
      stream_id, component_id, state);
}

/* ICE candidates management begin */

static void
kms_webrtc_endpoint_gathering_done (NiceAgent * agent, guint stream_id,
    KmsWebrtcEndpoint * self)
{
  KmsBaseRtpEndpoint *base_rtp_endpoint = KMS_BASE_RTP_ENDPOINT (self);
  GHashTable *conns;
  GHashTableIter iter;
  gpointer key, v;
  gboolean done = TRUE;

  GST_DEBUG_OBJECT (self, "ICE gathering done for '%s' stream.",
      nice_agent_get_stream_name (agent, stream_id));

  KMS_ELEMENT_LOCK (self);
  conns = kms_base_rtp_endpoint_get_connections (base_rtp_endpoint);

  g_hash_table_iter_init (&iter, conns);
  while (g_hash_table_iter_next (&iter, &key, &v)) {
    KmsWebRtcBaseConnection *conn = KMS_WEBRTC_BASE_CONNECTION (v);

    if (stream_id == conn->stream_id) {
      conn->ice_gathering_done = TRUE;
    }

    if (!conn->ice_gathering_done) {
      done = FALSE;
    }
  }

  if (done) {
    kms_webrtc_endpoint_local_sdp_add_default_info (self);
  }
  KMS_ELEMENT_UNLOCK (self);

  if (done) {
    g_signal_emit (G_OBJECT (self),
        kms_webrtc_endpoint_signals[SIGNAL_ON_ICE_GATHERING_DONE], 0);
  }
}

static gboolean
kms_webrtc_endpoint_gather_candidates (KmsWebrtcEndpoint * self)
{
  KmsBaseRtpEndpoint *base_rtp_endpoint = KMS_BASE_RTP_ENDPOINT (self);
  GHashTable *conns = kms_base_rtp_endpoint_get_connections (base_rtp_endpoint);
  GHashTableIter iter;
  gpointer key, v;
  gboolean ret = TRUE;

  KMS_ELEMENT_LOCK (self);
  g_hash_table_iter_init (&iter, conns);
  while (g_hash_table_iter_next (&iter, &key, &v)) {
    KmsWebRtcBaseConnection *conn = KMS_WEBRTC_BASE_CONNECTION (v);

    kms_webrtc_endpoint_set_relay_info (self, conn);
    if (!nice_agent_gather_candidates (conn->agent, conn->stream_id)) {
      GST_ERROR_OBJECT (self, "Failed to start candidate gathering for '%s'.",
          conn->name);
      ret = FALSE;
    }
  }
  KMS_ELEMENT_UNLOCK (self);

  return ret;
}

static void
sdp_media_add_ice_candidate (GstSDPMedia * media, NiceAgent * agent,
    NiceCandidate * cand)
{
  gchar *str;

  str = nice_agent_generate_local_candidate_sdp (agent, cand);
  gst_sdp_media_add_attribute (media, SDP_CANDIDATE_ATTR,
      str + SDP_CANDIDATE_ATTR_LEN);
  g_free (str);
}

static const gchar *
kms_webrtc_endpoint_sdp_media_add_ice_candidate (KmsWebrtcEndpoint * self,
    SdpMediaConfig * mconf, NiceAgent * agent, NiceCandidate * cand)
{
  guint media_stream_id;
  GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);
  const gchar *mid;

  media_stream_id = kms_webrtc_endpoint_media_get_stream_id (self, mconf);
  if (media_stream_id == -1) {
    return NULL;
  }

  if (media_stream_id != cand->stream_id) {
    return NULL;
  }

  sdp_media_add_ice_candidate (media, agent, cand);

  mid = kms_sdp_media_config_get_mid (mconf);
  if (mid == NULL) {
    return "";
  }

  return mid;
}

static void
kms_webrtc_endpoint_sdp_msg_add_ice_candidate (KmsWebrtcEndpoint * self,
    NiceAgent * agent, NiceCandidate * nice_cand)
{
  KmsBaseSdpEndpoint *base_sdp_ep = KMS_BASE_SDP_ENDPOINT (self);
  SdpMessageContext *local_ctx =
      kms_base_sdp_endpoint_get_local_sdp_ctx (base_sdp_ep);
  const GSList *item = kms_sdp_message_context_get_medias (local_ctx);
  GList *list = NULL, *iterator = NULL;

  KMS_ELEMENT_LOCK (self);

  for (; item != NULL; item = g_slist_next (item)) {
    SdpMediaConfig *mconf = item->data;
    gint idx = kms_sdp_media_config_get_id (mconf);
    const gchar *mid;

    if (kms_sdp_media_config_is_inactive (mconf)) {
      GST_DEBUG_OBJECT (self, "Media (id=%d) inactive", idx);
      continue;
    }

    mid =
        kms_webrtc_endpoint_sdp_media_add_ice_candidate (self, mconf,
        agent, nice_cand);
    if (mid != NULL) {
      KmsIceCandidate *candidate =
          kms_ice_candidate_new_from_nice (agent, nice_cand, mid, idx);

      list = g_list_append (list, candidate);
    }
  }

  KMS_ELEMENT_UNLOCK (self);

  for (iterator = list; iterator; iterator = iterator->next) {
    g_signal_emit (G_OBJECT (self),
        kms_webrtc_endpoint_signals[SIGNAL_ON_ICE_CANDIDATE], 0,
        iterator->data);
  }

  g_list_free_full (list, g_object_unref);
}

/* TODO: change using "new-candidate-full" of libnice 0.1.8 */
static void
kms_webrtc_endpoint_new_candidate (NiceAgent * agent,
    guint stream_id,
    guint component_id, gchar * foundation, KmsWebrtcEndpoint * self)
{
  GSList *candidates;
  GSList *walk;

  GST_TRACE_OBJECT (self, "stream_id: %d, component_id: %d, foundation: %s",
      stream_id, component_id, foundation);

  candidates = nice_agent_get_local_candidates (agent, stream_id, component_id);

  for (walk = candidates; walk; walk = walk->next) {
    NiceCandidate *cand = walk->data;

    if (cand->stream_id == stream_id &&
        cand->component_id == component_id &&
        g_strcmp0 (foundation, cand->foundation) == 0) {
      kms_webrtc_endpoint_sdp_msg_add_ice_candidate (self, agent, cand);
    }
  }
  g_slist_free_full (candidates, (GDestroyNotify) nice_candidate_free);
}

static gboolean
kms_webrtc_endpoint_set_remote_ice_candidate (KmsWebrtcEndpoint * self,
    KmsIceCandidate * candidate, NiceCandidate * nice_cand)
{
  KmsBaseSdpEndpoint *base_sdp_ep = KMS_BASE_SDP_ENDPOINT (self);
  SdpMessageContext *local_ctx =
      kms_base_sdp_endpoint_get_local_sdp_ctx (base_sdp_ep);
  guint8 index;
  GSList *medias;
  SdpMediaConfig *mconf;
  gboolean ret;

  if (local_ctx == NULL) {
    GST_INFO_OBJECT (self,
        "Cannot add candidate until local SDP is generated.");
    return TRUE;                /* We do not know if the candidate is valid until it is set */
  }

  medias = kms_sdp_message_context_get_medias (local_ctx);
  index = kms_ice_candidate_get_sdp_m_line_index (candidate);
  mconf = g_slist_nth_data (medias, index);
  if (mconf == NULL) {
    GST_WARNING_OBJECT (self,
        "Media not found in local SDP for index %" G_GUINT16_FORMAT, index);
    return FALSE;
  } else if (kms_sdp_media_config_is_inactive (mconf)) {
    GST_DEBUG_OBJECT (self, "Media inactive for index %" G_GUINT16_FORMAT,
        index);
    return TRUE;
  } else {
    GSList *candidates;
    const gchar *cand_str;

    nice_cand->stream_id =
        kms_webrtc_endpoint_media_get_stream_id (self, mconf);
    if (nice_cand->stream_id == -1) {
      return FALSE;
    }

    cand_str = kms_ice_candidate_get_candidate (candidate);
    candidates = g_slist_append (NULL, nice_cand);

    if (nice_agent_set_remote_candidates (self->priv->agent,
            nice_cand->stream_id, nice_cand->component_id, candidates) < 0) {
      GST_WARNING_OBJECT (self, "Cannot add candidate: '%s'in stream_id: %d.",
          cand_str, nice_cand->stream_id);
      ret = FALSE;
    } else {
      GST_TRACE_OBJECT (self, "Candidate added: '%s' in stream_id: %d.",
          cand_str, nice_cand->stream_id);
      ret = TRUE;
    }

    g_slist_free (candidates);
  }

  return ret;
}

static void
kms_webrtc_endpoint_remote_sdp_add_ice_candidate (KmsWebrtcEndpoint * self,
    NiceCandidate * nice_cand, guint8 index)
{
  KmsBaseSdpEndpoint *base_sdp_ep = KMS_BASE_SDP_ENDPOINT (self);
  SdpMessageContext *remote_ctx =
      kms_base_sdp_endpoint_get_remote_sdp_ctx (base_sdp_ep);
  GSList *medias;
  SdpMediaConfig *mconf;

  if (remote_ctx == NULL) {
    GST_INFO_OBJECT (self, "Cannot update remote SDP until it is set.");
    return;
  }

  medias = kms_sdp_message_context_get_medias (remote_ctx);
  mconf = g_slist_nth_data (medias, index);
  if (mconf == NULL) {
    GST_WARNING_OBJECT (self,
        "Media not found in remote SDP for index %" G_GUINT16_FORMAT, index);
  } else {
    GstSDPMedia *media = kms_sdp_media_config_get_sdp_media (mconf);

    sdp_media_add_ice_candidate (media, self->priv->agent, nice_cand);
  }
}

static gboolean
kms_webrtc_endpoint_add_ice_candidate (KmsWebrtcEndpoint * self,
    KmsIceCandidate * candidate)
{
  NiceCandidate *nice_cand;
  guint8 index;
  gboolean ret;

  ret = kms_ice_candidate_create_nice (candidate, &nice_cand);
  if (nice_cand == NULL) {
    return ret;
  }

  KMS_ELEMENT_LOCK (self);
  self->priv->remote_candidates = g_slist_append (self->priv->remote_candidates,
      g_object_ref (candidate));

  ret =
      kms_webrtc_endpoint_set_remote_ice_candidate (self, candidate, nice_cand);

  index = kms_ice_candidate_get_sdp_m_line_index (candidate);
  kms_webrtc_endpoint_remote_sdp_add_ice_candidate (self, nice_cand, index);
  KMS_ELEMENT_UNLOCK (self);

  nice_candidate_free (nice_cand);

  return ret;
}

/* ICE candidates management end */

static void
kms_webrtc_endpoint_parse_turn_url (KmsWebrtcEndpoint * self)
{
  GRegex *regex;
  GMatchInfo *match_info = NULL;

  g_free (self->priv->turn_user);
  self->priv->turn_user = NULL;
  g_free (self->priv->turn_password);
  self->priv->turn_password = NULL;
  g_free (self->priv->turn_address);
  self->priv->turn_address = NULL;

  if ((self->priv->turn_url == NULL)
      || (g_strcmp0 ("", self->priv->turn_url) == 0)) {
    GST_INFO_OBJECT (self, "TURN server info cleared");
    return;
  }

  regex =
      g_regex_new
      ("^(?<user>.+):(?<password>.+)@(?<address>[0-9.]+):(?<port>[0-9]+)(\\?transport=(?<transport>(udp|tcp|tls)))?$",
      0, 0, NULL);
  g_regex_match (regex, self->priv->turn_url, 0, &match_info);
  g_regex_unref (regex);

  if (g_match_info_matches (match_info)) {
    gchar *port_str;
    gchar *turn_transport;

    self->priv->turn_user = g_match_info_fetch_named (match_info, "user");
    self->priv->turn_password =
        g_match_info_fetch_named (match_info, "password");
    self->priv->turn_address = g_match_info_fetch_named (match_info, "address");

    port_str = g_match_info_fetch_named (match_info, "port");
    self->priv->turn_port = g_ascii_strtoll (port_str, NULL, 10);
    g_free (port_str);

    self->priv->turn_transport = NICE_RELAY_TYPE_TURN_UDP;      /* default */
    turn_transport = g_match_info_fetch_named (match_info, "transport");
    if (turn_transport != NULL) {
      if (g_strcmp0 ("tcp", turn_transport) == 0) {
        self->priv->turn_transport = NICE_RELAY_TYPE_TURN_TCP;
      } else if (g_strcmp0 ("tls", turn_transport) == 0) {
        self->priv->turn_transport = NICE_RELAY_TYPE_TURN_TLS;
      }
      g_free (turn_transport);
    }

    GST_INFO_OBJECT (self, "TURN server info set (%s)", self->priv->turn_url);
  } else {
    GST_ELEMENT_ERROR (self, RESOURCE, SETTINGS,
        ("URL '%s' not allowed. It must have this format: 'user:password@address:port(?transport=[udp|tcp|tls])'",
            self->priv->turn_url),
        ("URL '%s' not allowed. It must have this format: 'user:password@address:port(?transport=[udp|tcp|tls])'",
            self->priv->turn_url));
  }

  g_match_info_free (match_info);
}

static void
kms_webrtc_endpoint_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (prop_id) {
    case PROP_CERTIFICATE_PEM_FILE:    /* TODO: format */
      g_free (self->priv->certificate_pem_file);
      self->priv->certificate_pem_file = g_value_dup_string (value);
      {
        KmsBaseRtpEndpoint *base_rtp_endpoint = KMS_BASE_RTP_ENDPOINT (self);
        GHashTable *conns =
            kms_base_rtp_endpoint_get_connections (base_rtp_endpoint);
        GHashTableIter iter;
        gpointer key, v;

        g_hash_table_iter_init (&iter, conns);
        while (g_hash_table_iter_next (&iter, &key, &v)) {
          KmsWebRtcBaseConnection *conn = KMS_WEBRTC_BASE_CONNECTION (v);

          kms_webrtc_base_connection_set_certificate_pem_file (conn,
              self->priv->certificate_pem_file);
        }
      }
      break;
    case PROP_STUN_SERVER_IP:
      if (self->priv->agent == NULL) {
        GST_ERROR_OBJECT (self, "ICE agent not initialized.");
        break;
      }

      g_object_set_property (G_OBJECT (self->priv->agent), "stun-server",
          value);
      break;
    case PROP_STUN_SERVER_PORT:
      if (self->priv->agent == NULL) {
        GST_ERROR_OBJECT (self, "ICE agent not initialized.");
        break;
      }

      g_object_set_property (G_OBJECT (self->priv->agent), "stun-server-port",
          value);
      break;
    case PROP_TURN_URL:
      g_free (self->priv->turn_url);
      self->priv->turn_url = g_value_dup_string (value);
      kms_webrtc_endpoint_parse_turn_url (self);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_webrtc_endpoint_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (prop_id) {
    case PROP_CERTIFICATE_PEM_FILE:
      g_value_set_string (value, self->priv->certificate_pem_file);
      break;
    case PROP_STUN_SERVER_IP:
      if (self->priv->agent == NULL) {
        GST_ERROR_OBJECT (self, "ICE agent not initialized.");
        break;
      }

      g_object_get_property (G_OBJECT (self->priv->agent), "stun-server",
          value);
      break;
    case PROP_STUN_SERVER_PORT:
      if (self->priv->agent == NULL) {
        GST_ERROR_OBJECT (self, "ICE agent not initialized.");
        break;
      }

      g_object_get_property (G_OBJECT (self->priv->agent), "stun-server-port",
          value);
      break;
    case PROP_TURN_URL:
      g_value_set_string (value, self->priv->turn_url);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_webrtc_endpoint_dispose (GObject * object)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (object);

  GST_DEBUG_OBJECT (self, "dispose");

  KMS_ELEMENT_LOCK (self);

  g_clear_object (&self->priv->agent);
  g_clear_object (&self->priv->loop);

  KMS_ELEMENT_UNLOCK (self);

  /* chain up */
  G_OBJECT_CLASS (kms_webrtc_endpoint_parent_class)->dispose (object);
}

static void
kms_webrtc_endpoint_finalize (GObject * object)
{
  KmsWebrtcEndpoint *self = KMS_WEBRTC_ENDPOINT (object);

  GST_DEBUG_OBJECT (self, "finalize");

  g_slist_free_full (self->priv->remote_candidates, g_object_unref);

  g_free (self->priv->certificate_pem_file);
  g_free (self->priv->turn_url);
  g_free (self->priv->turn_user);
  g_free (self->priv->turn_password);
  g_free (self->priv->turn_address);

  g_main_context_unref (self->priv->context);

  /* chain up */
  G_OBJECT_CLASS (kms_webrtc_endpoint_parent_class)->finalize (object);
}

static void
kms_webrtc_endpoint_class_init (KmsWebrtcEndpointClass * klass)
{
  GObjectClass *gobject_class;
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class;
  KmsBaseRtpEndpointClass *base_rtp_endpoint_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = kms_webrtc_endpoint_set_property;
  gobject_class->get_property = kms_webrtc_endpoint_get_property;
  gobject_class->dispose = kms_webrtc_endpoint_dispose;
  gobject_class->finalize = kms_webrtc_endpoint_finalize;

  gst_element_class_set_details_simple (GST_ELEMENT_CLASS (klass),
      "WebrtcEndpoint",
      "WEBRTC/Stream/WebrtcEndpoint",
      "WebRTC Endpoint element", "Miguel París Díaz <mparisdiaz@gmail.com>");

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);

  base_sdp_endpoint_class = KMS_BASE_SDP_ENDPOINT_CLASS (klass);
  base_sdp_endpoint_class->start_transport_send =
      kms_webrtc_endpoint_start_transport_send;

  /* Media handler management */
  base_sdp_endpoint_class->create_media_handler =
      kms_webrtc_endpoint_create_media_handler;

  base_sdp_endpoint_class->configure_media =
      kms_webrtc_endpoint_configure_media;
  base_sdp_endpoint_class->connect_input_elements =
      kms_webrtc_endpoint_connect_input_elements;

  base_rtp_endpoint_class = KMS_BASE_RTP_ENDPOINT_CLASS (klass);
  /* Connection management */
  base_rtp_endpoint_class->create_connection =
      kms_webrtc_endpoint_create_connection;
  base_rtp_endpoint_class->create_rtcp_mux_connection =
      kms_webrtc_endpoint_create_rtcp_mux_connection;
  base_rtp_endpoint_class->create_bundle_connection =
      kms_webrtc_endpoint_create_bundle_connection;

  klass->gather_candidates = kms_webrtc_endpoint_gather_candidates;
  klass->add_ice_candidate = kms_webrtc_endpoint_add_ice_candidate;

  g_object_class_install_property (gobject_class, PROP_CERTIFICATE_PEM_FILE,
      g_param_spec_string ("certificate-pem-file",
          "Certificate PEM File",
          "PEM File name containing the certificate and private key",
          NULL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STUN_SERVER_IP,
      g_param_spec_string ("stun-server",
          "StunServer",
          "Stun Server IP Address",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STUN_SERVER_PORT,
      g_param_spec_uint ("stun-server-port",
          "StunServerPort",
          "Stun Server Port",
          1, G_MAXUINT16, 3478, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TURN_URL,
      g_param_spec_string ("turn-url",
          "TurnUrl",
          "TURN server URL with this format: 'user:password@address:port(?transport=[udp|tcp|tls])'."
          "'address' must be an IP (not a domain)."
          "'transport' is optional (UDP by default).",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
  * KmsWebrtcEndpoint::on-ice-candidate:
  * @self: the object which received the signal
  * @candidate: the local candidate gathered
  *
  * Notify of a new gathered local candidate for a #KmsWebrtcEndpoint.
  */
  kms_webrtc_endpoint_signals[SIGNAL_ON_ICE_CANDIDATE] =
      g_signal_new ("on-ice-candidate",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsWebrtcEndpointClass, on_ice_candidate), NULL,
      NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
      KMS_TYPE_ICE_CANDIDATE);

  /**
  * KmsWebrtcEndpoint::on-candidate-gathering-done:
  * @self: the object which received the signal
  *
  * Notify that all candidates have been gathered for a #KmsWebrtcEndpoint
  */
  kms_webrtc_endpoint_signals[SIGNAL_ON_ICE_GATHERING_DONE] =
      g_signal_new ("on-ice-gathering-done",
      G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsWebrtcEndpointClass, on_ice_gathering_done), NULL,
      NULL, NULL, G_TYPE_NONE, 0);

  /**
   * KmsWebrtcEndpoint::on-component-state-changed
   * @self: the object which received the signal
   * @stream_id: The ID of the stream
   * @component_id: The ID of the component
   * @state: The #NiceComponentState of the component
   *
   * This signal is fired whenever a component's state changes
   */
  kms_webrtc_endpoint_signals[SIGNAL_ON_ICE_COMPONENT_STATE_CHANGED] =
      g_signal_new ("on-ice-component-state-changed",
      G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
      G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_INVALID);

  kms_webrtc_endpoint_signals[SIGNAL_ADD_ICE_CANDIDATE] =
      g_signal_new ("add-ice-candidate",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsWebrtcEndpointClass, add_ice_candidate), NULL, NULL,
      __kms_webrtc_marshal_BOOLEAN__OBJECT, G_TYPE_BOOLEAN, 1,
      KMS_TYPE_ICE_CANDIDATE);

  kms_webrtc_endpoint_signals[SIGNAL_GATHER_CANDIDATES] =
      g_signal_new ("gather-candidates",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsWebrtcEndpointClass, gather_candidates), NULL, NULL,
      __kms_webrtc_marshal_BOOLEAN__VOID, G_TYPE_BOOLEAN, 0);

  g_type_class_add_private (klass, sizeof (KmsWebrtcEndpointPrivate));
}

static void
kms_webrtc_endpoint_init (KmsWebrtcEndpoint * self)
{
  g_object_set (G_OBJECT (self), "bundle", TRUE, "rtcp-mux", TRUE, "rtcp-nack",
      TRUE, "rtcp-remb", TRUE, NULL);

  self->priv = KMS_WEBRTC_ENDPOINT_GET_PRIVATE (self);

  self->priv->loop = kms_loop_new ();
  g_object_get (self->priv->loop, "context", &self->priv->context, NULL);

  self->priv->agent =
      nice_agent_new (self->priv->context, NICE_COMPATIBILITY_RFC5245);
  if (self->priv->agent == NULL) {
    GST_ERROR_OBJECT (self, "Cannot create nice agent.");
    return;
  }

  g_object_set (self->priv->agent, "upnp", FALSE, /* "ice-tcp", FALSE, */ NULL);
  g_signal_connect (self->priv->agent, "candidate-gathering-done",
      G_CALLBACK (kms_webrtc_endpoint_gathering_done), self);
  g_signal_connect (self->priv->agent, "new-candidate",
      G_CALLBACK (kms_webrtc_endpoint_new_candidate), self);
  g_signal_connect (self->priv->agent, "component-state-changed",
      G_CALLBACK (kms_webrtc_endpoint_component_state_change), self);
}

gboolean
kms_webrtc_endpoint_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_WEBRTC_ENDPOINT);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    kmswebrtcendpoint,
    "Kurento webrtc endpoint",
    kms_webrtc_endpoint_plugin_init, VERSION, "LGPL",
    "Kurento Elements", "http://kurento.com/")
