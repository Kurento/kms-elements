/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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

#include <string.h>
#include <nice/interfaces.h>
#include "kmssiprtpendpoint.h"
#include "kmssiprtpsession.h"
#include "kmssipsrtpsession.h"
#include "kmsrtpconnection.h"
#include "kmssrtpconnection.h"
#include <commons/kmsbasesdpendpoint.h>
#include <commons/constants.h>
#include <gst/sdp/gstsdpmessage.h>

#define PLUGIN_NAME "siprtpendpoint"

#define DEFAULT_AUDIO_SSRC 0
#define DEFAULT_VIDEO_SSRC 0


GST_DEBUG_CATEGORY_STATIC (kms_sip_rtp_endpoint_debug);
#define GST_CAT_DEFAULT kms_sip_rtp_endpoint_debug

#define kms_sip_rtp_endpoint_parent_class parent_class
G_DEFINE_TYPE (KmsSipRtpEndpoint, kms_sip_rtp_endpoint, KMS_TYPE_RTP_ENDPOINT);


#define KMS_SIP_RTP_ENDPOINT_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (              \
    (obj),                                   \
    KMS_TYPE_SIP_RTP_ENDPOINT,                   \
    KmsSipRtpEndpointPrivate                    \
  )                                          \
)


typedef struct _KmsSipRtpEndpointCloneData KmsSipRtpEndpointCloneData;


struct _KmsSipRtpEndpointCloneData
{
	guint32 local_audio_ssrc;
	guint32 local_video_ssrc;

	SipFilterSsrcInfo* audio_filter_info;
	SipFilterSsrcInfo* video_filter_info;

	GHashTable *conns;
};

struct _KmsSipRtpEndpointPrivate
{
  gboolean *use_sdes_cache;

  GList *sessionData;
};

/* Properties */
enum
{
  PROP_0,
  PROP_AUDIO_SSRC,
  PROP_VIDEO_SSRC
};


/* Signals and args */
enum
{
  /* signals */
  SIGNAL_CLONE_TO_NEW_EP,

  LAST_SIGNAL
};

static guint obj_signals[LAST_SIGNAL] = { 0 };

static KmsBaseSdpEndpointClass *base_sdp_endpoint_type;


/*----------- Session cloning ---------------*/

static void
kms_sip_rtp_endpoint_clone_rtp_session (GstElement * rtpbin, guint sessionId, guint32 ssrc, gchar *rtpbin_pad_name)
{
	GObject *rtpSession;
    GstPad *pad;

	/* Create RtpSession requesting the pad */
	pad = gst_element_get_request_pad (rtpbin, rtpbin_pad_name);
	g_object_unref (pad);

	g_signal_emit_by_name (rtpbin, "get-internal-session", sessionId, &rtpSession);
	if (rtpSession != NULL) {
		g_object_set (rtpSession, "internal-ssrc", ssrc, NULL);
	}

	g_object_unref(rtpSession);
}
static GstElement*
kms_sip_rtp_endpoint_get_rtpbin (KmsSipRtpEndpoint * self)
{
	GstElement *result = NULL;
	GList* rtpEndpointChildren = GST_BIN_CHILDREN(GST_BIN(self));

	while (rtpEndpointChildren != NULL) {
		gchar* objectName = gst_element_get_name  (GST_ELEMENT(rtpEndpointChildren->data));

		if (g_str_has_prefix (objectName, "rtpbin")) {
			result = GST_ELEMENT(rtpEndpointChildren->data);
			g_free (objectName);
			break;
		}
		g_free (objectName);
		rtpEndpointChildren = rtpEndpointChildren->next;
	}
	return result;
}

static KmsSipRtpEndpointCloneData*
kms_sip_rtp_endpoint_get_clone_data (GList *sessionData)
{
	if (sessionData == NULL)
		return NULL;
	return ((KmsSipRtpEndpointCloneData*)sessionData->data);
}

static void
kms_sip_rtp_endpoint_preserve_rtp_session_data (KmsSipRtpSession *ses,
		GHashTable *conns)
{
	KMS_SIP_RTP_SESSION_CLASS(G_OBJECT_GET_CLASS(ses))->clone_connections (ses,conns);
}

static void
kms_sip_rtp_endpoint_preserve_srtp_session_data (KmsSipSrtpSession *ses,
		GHashTable *conns)
{
	KMS_SIP_SRTP_SESSION_CLASS(G_OBJECT_GET_CLASS(ses))->clone_connections (ses,conns);
}

static void
kms_sip_rtp_endpoint_clone_session (KmsSipRtpEndpoint * self, KmsSdpSession ** sess)
{
	GstElement *rtpbin = kms_sip_rtp_endpoint_get_rtpbin (self);
	GList *sessionToClone = self->priv->sessionData;

	if (rtpbin != NULL) {
		gboolean is_srtp = FALSE;

		is_srtp = KMS_IS_SIP_SRTP_SESSION (*sess);
		// TODO: Multisession seems not used on RTPEndpoint, anyway we are doing something probably incorrect
		// once multisession is used, that is to assume that creation order of sessions are maintained among all
		// endpoints, and so order can be used to correlate internal rtp sessions.
		KmsBaseRtpSession *clonedSes = KMS_BASE_RTP_SESSION (*sess);
		guint32 ssrc;
		GHashTable *conns;

		conns = kms_sip_rtp_endpoint_get_clone_data(sessionToClone)->conns;

		/* TODO: think about this when multiple audio/video medias */
		// Audio
		//      Clone SSRC
		ssrc = kms_sip_rtp_endpoint_get_clone_data(sessionToClone)->local_audio_ssrc;
		clonedSes->local_audio_ssrc = ssrc;
		kms_sip_rtp_endpoint_clone_rtp_session (rtpbin, AUDIO_RTP_SESSION, ssrc, AUDIO_RTPBIN_SEND_RTP_SINK);

		// Video
		//        Clone SSRC
		ssrc = kms_sip_rtp_endpoint_get_clone_data(sessionToClone)->local_video_ssrc;
		clonedSes->local_video_ssrc = ssrc;
		kms_sip_rtp_endpoint_clone_rtp_session (rtpbin, VIDEO_RTP_SESSION, ssrc, VIDEO_RTPBIN_SEND_RTP_SINK);

		if (is_srtp) {
			kms_sip_rtp_endpoint_preserve_srtp_session_data (KMS_SIP_SRTP_SESSION(*sess), conns);
		} else {
			kms_sip_rtp_endpoint_preserve_rtp_session_data (KMS_SIP_RTP_SESSION(*sess), conns);
		}
	}
}



static gboolean isUseSdes (KmsSipRtpEndpoint * self)
{
	if (self->priv->use_sdes_cache == NULL) {
		gboolean useSdes;

		g_object_get (G_OBJECT(self), "use-sdes", &useSdes, NULL);
		self->priv->use_sdes_cache = g_malloc(sizeof(gboolean));
		*self->priv->use_sdes_cache = useSdes;
	}
	return *self->priv->use_sdes_cache;
}


static void
kms_sip_rtp_endpoint_set_addr (KmsSipRtpEndpoint * self)
{
  GList *ips, *l;
  gboolean done = FALSE;

  ips = nice_interfaces_get_local_ips (FALSE);
  for (l = ips; l != NULL && !done; l = l->next) {
    GInetAddress *addr;
    gboolean is_ipv6 = FALSE;

    GST_DEBUG_OBJECT (self, "Check local address: %s", (const gchar*)l->data);
    addr = g_inet_address_new_from_string (l->data);

    if (G_IS_INET_ADDRESS (addr)) {
      switch (g_inet_address_get_family (addr)) {
        case G_SOCKET_FAMILY_INVALID:
        case G_SOCKET_FAMILY_UNIX:
          /* Ignore this addresses */
          break;
        case G_SOCKET_FAMILY_IPV6:
          is_ipv6 = TRUE;
        case G_SOCKET_FAMILY_IPV4:
        {
          gchar *addr_str;
          gboolean use_ipv6;

          g_object_get (self, "use-ipv6", &use_ipv6, NULL);
          if (is_ipv6 != use_ipv6) {
            GST_DEBUG_OBJECT (self, "Skip address (wanted IPv6: %d)", use_ipv6);
            break;
          }

          addr_str = g_inet_address_to_string (addr);
          if (addr_str != NULL) {
            g_object_set (self, "addr", addr_str, NULL);
            g_free (addr_str);
            done = TRUE;
          }
          break;
        }
      }
    }

    if (G_IS_OBJECT (addr)) {
      g_object_unref (addr);
    }
  }

  g_list_free_full (ips, g_free);

  if (!done) {
    GST_WARNING_OBJECT (self, "Addr not set");
  }
}

static void
kms_sip_rtp_endpoint_create_session_internal (KmsBaseSdpEndpoint * base_sdp,
    gint id, KmsSdpSession ** sess)
{
  KmsIRtpSessionManager *manager = KMS_I_RTP_SESSION_MANAGER (base_sdp);
  KmsSipRtpEndpoint *self = KMS_SIP_RTP_ENDPOINT (base_sdp);
  gboolean use_ipv6 = FALSE;
  KmsSipRtpEndpointCloneData *data = NULL;

  /* Get ip address now that session is being created */
  kms_sip_rtp_endpoint_set_addr (self);

  g_object_get (self, "use-ipv6", &use_ipv6, NULL);
  if (isUseSdes(self)) {
	KmsSipSrtpSession *sip_srtp_ses = kms_sip_srtp_session_new (base_sdp, id, manager, use_ipv6);
    *sess = KMS_SDP_SESSION (sip_srtp_ses);
	if (self->priv->sessionData != NULL) {
		data = (KmsSipRtpEndpointCloneData*) self->priv->sessionData->data;
		sip_srtp_ses->audio_filter_info = data->audio_filter_info;
		sip_srtp_ses->video_filter_info = data->video_filter_info;
	}
  } else {
	KmsSipRtpSession *sip_rtp_ses = kms_sip_rtp_session_new (base_sdp, id, manager, use_ipv6);
    *sess = KMS_SDP_SESSION (sip_rtp_ses);
	if (self->priv->sessionData != NULL) {
		data = (KmsSipRtpEndpointCloneData*) self->priv->sessionData->data;
		sip_rtp_ses->audio_filter_info = data->audio_filter_info;
		sip_rtp_ses->video_filter_info = data->video_filter_info;
	}
  }

  /* Chain up */
  base_sdp_endpoint_type->create_session_internal (base_sdp, id, sess);
//  KMS_BASE_SDP_ENDPOINT_CLASS(
//  (KMS_RTP_ENDPOINT_CLASS
//      (kms_sip_rtp_endpoint_parent_class)->parent_class)->
//	  ->create_session_internal (base_sdp, id, sess);

  if (self->priv->sessionData != NULL) {
	  kms_sip_rtp_endpoint_clone_session (self, sess);
  }

}

/* Internal session management end */


static void
kms_sip_rtp_endpoint_create_media_handler (KmsBaseSdpEndpoint * base_sdp,
    const gchar * media, KmsSdpMediaHandler ** handler)
{
	KMS_BASE_SDP_ENDPOINT_CLASS(kms_sip_rtp_endpoint_parent_class)->create_media_handler (base_sdp, media, handler);

}




/* Configure media SDP begin */
static gboolean
kms_sip_rtp_endpoint_configure_media (KmsBaseSdpEndpoint * base_sdp_endpoint,
    KmsSdpSession * sess, KmsSdpMediaHandler * handler, GstSDPMedia * media)
{
  gboolean ret = TRUE;

  /* Chain up */
  ret = 	KMS_BASE_SDP_ENDPOINT_CLASS(kms_sip_rtp_endpoint_parent_class)->
		  	  configure_media (base_sdp_endpoint, sess, handler, media);
  return ret;
}

/* Configure media SDP end */

//static void
//kms_sip_rtp_endpoint_set_connection_filter_probe (KmsSipRtpEndpoint *self, KmsIRtpConnection *conn, KmsBaseRtpSession *ses, guint32 expected_ssrc)
//{
//	if (KMS_IS_RTP_CONNECTION (conn)) {
//		gulong rtp_probe, rtcp_probe;
//		KmsSipRtpSession *sip_ses = KMS_SIP_RTP_SESSION (ses);
//		KmsRtpConnection *rtp_conn = KMS_RTP_CONNECTION (conn);
//
//		kms_sip_rtp_connection_add_probes (rtp_conn, filter_info, &rtp_probe, &rtcp_probe);
//		KMS_SIP_RTP_SESSION_CLASS(G_OBJECT_GET_CLASS(ses))->store_rtp_filtering_info (sip_ses, rtp_conn, rtp_probe, rtcp_probe);
//	} else if (KMS_IS_SRTP_CONNECTION (conn)) {
//		gulong rtp_probe, rtcp_probe;
//		KmsSipSrtpSession *sip_ses = KMS_SIP_SRTP_SESSION (ses);
//		KmsSrtpConnection *rtp_conn = KMS_SRTP_CONNECTION (conn);
//
//		kms_sip_srtp_connection_add_probes (rtp_conn, filter_info, &rtp_probe, &rtcp_probe);
//		KMS_SIP_SRTP_SESSION_CLASS(G_OBJECT_GET_CLASS(ses))->store_rtp_filtering_info (sip_ses, rtp_conn, rtp_probe, rtcp_probe);
//	}
//}


//static void
//kms_sip_rtp_endpoint_set_filter_probes (KmsSipRtpEndpoint *self, guint32 expected_audio_ssrc, guint32 expected_video_ssrc)
//{
//	GHashTable * sessions = kms_base_sdp_endpoint_get_sessions (KMS_BASE_SDP_ENDPOINT(self));
//	GList *sessionKeys = g_hash_table_get_keys (sessions);
//	gint i;
//	KmsIRtpConnection *conn;
//
//	// In fact SipRtpEndpoint should have only one session, if not, this loop should be revised
//	for (i = 0; i < g_hash_table_size(sessions); i++) {
//		gpointer sesKey = sessionKeys->data;
//		KmsBaseRtpSession *ses = KMS_BASE_RTP_SESSION (g_hash_table_lookup (sessions, sesKey));
//
//		// AUDIO
//		conn = g_hash_table_lookup (ses->conns, AUDIO_RTP_SESSION_STR);
//		kms_sip_rtp_endpoint_set_connection_filter_probe (self, conn, ses, expected_audio_ssrc);
//
//		// VIDEO
//		conn = g_hash_table_lookup (ses->conns, VIDEO_RTP_SESSION_STR);
//		kms_sip_rtp_endpoint_set_connection_filter_probe (self, conn, ses, expected_video_ssrc);
//	}
//	g_list_free(sessionKeys);
//}

static guint
ssrc_str_to_uint (const gchar * ssrc_str)
{
  gint64 val;
  guint ssrc = 0;

  val = g_ascii_strtoll (ssrc_str, NULL, 10);
  if (val > G_MAXUINT32) {
    GST_ERROR ("SSRC %" G_GINT64_FORMAT " not valid", val);
  } else {
    ssrc = val;
  }

  return ssrc;
}

static gchar *
sdp_media_get_ssrc_str (const GstSDPMedia * media)
{
  gchar *ssrc = NULL;
  const gchar *val;
  GRegex *regex;
  GMatchInfo *match_info = NULL;

  val = gst_sdp_media_get_attribute_val (media, "ssrc");
  if (val == NULL) {
    return NULL;
  }

  regex = g_regex_new ("^(?<ssrc>[0-9]+)(.*)?$", 0, 0, NULL);
  g_regex_match (regex, val, 0, &match_info);
  g_regex_unref (regex);

  if (g_match_info_matches (match_info)) {
    ssrc = g_match_info_fetch_named (match_info, "ssrc");
  }
  g_match_info_free (match_info);

  return ssrc;
}

static guint32
kms_sip_rtp_endpoint_get_ssrc (const GstSDPMedia* media)
{
	gchar *ssrc_str;
	guint32 ssrc = 0;

	ssrc_str = sdp_media_get_ssrc_str (media);
	if (ssrc_str == NULL) {
	  return 0;
	}

	ssrc = ssrc_str_to_uint (ssrc_str);
	g_free (ssrc_str);

	return ssrc;
}


static gboolean
kms_sip_rtp_endpoint_get_expected_ssrc (const GstSDPMessage *sdp, guint32 *audio_ssrc, guint32 *video_ssrc)
{
	const GstSDPMedia *media;
	guint idx = 0;
	guint num_medias = 0;
	gboolean result = TRUE;

	// We are expecting an SDP answer with just one audio media and just one video media
	// If this was to change, this function would need reconsidering
	num_medias = gst_sdp_message_medias_len  (sdp);
	while (idx < num_medias) {
		const gchar* media_name;

		media = gst_sdp_message_get_media (sdp, idx);
		media_name = gst_sdp_media_get_media (media);
		GST_DEBUG("Found media %s", media_name);

		if (g_strcmp0 (AUDIO_STREAM_NAME, media_name) == 0) {
			*audio_ssrc = kms_sip_rtp_endpoint_get_ssrc (media);
		} else if (g_strcmp0 (VIDEO_STREAM_NAME, media_name) == 0) {
			*video_ssrc = kms_sip_rtp_endpoint_get_ssrc (media);
		} else  {
			result = FALSE;
		}
		idx++;
	}

	return result;
}



static gboolean
kms_sip_rtp_endpoint_process_answer (KmsBaseSdpEndpoint * ep,
    const gchar * sess_id, GstSDPMessage * answer)
{
//	KmsSipRtpEndpoint *self = KMS_SIP_RTP_ENDPOINT(ep);
//	guint32 expected_audio_ssrc = 0;
//	guint32 expected_video_ssrc = 0;
//
//	kms_sip_rtp_endpoint_get_expected_ssrc (answer, &expected_audio_ssrc, &expected_video_ssrc);
//	kms_sip_rtp_endpoint_set_filter_probes (self);
	return KMS_BASE_SDP_ENDPOINT_CLASS(kms_sip_rtp_endpoint_parent_class)->process_answer (ep, sess_id, answer);
}

static void
kms_sip_rtp_endpoint_start_transport_send (KmsBaseSdpEndpoint *base_sdp_endpoint,
    KmsSdpSession *sess, gboolean offerer)
{
	KMS_BASE_SDP_ENDPOINT_CLASS(kms_sip_rtp_endpoint_parent_class)->start_transport_send (base_sdp_endpoint, sess, offerer);

}

static KmsSipRtpEndpointCloneData*
kms_sip_rtp_endpoint_create_clone_data (KmsSipRtpEndpoint *self, KmsBaseRtpSession *ses, guint32 audio_ssrc, guint32 video_ssrc, gboolean continue_audio_stream, gboolean continue_video_stream)
{
	KmsSipRtpEndpointCloneData *data = g_malloc(sizeof (KmsSipRtpEndpointCloneData));
	SipFilterSsrcInfo* audio_filter_info = NULL;
	SipFilterSsrcInfo* video_filter_info = NULL;

	data->local_audio_ssrc = ses->local_audio_ssrc;
	data->local_video_ssrc = ses->local_video_ssrc;

	if (KMS_IS_SIP_RTP_SESSION (ses)) {
		KmsSipRtpSession* sip_ses = KMS_SIP_RTP_SESSION (ses);

		GST_DEBUG ("kms_sip_rtp_endpoint_create_clone_data audio filter %p, video filter %p", sip_ses->audio_filter_info, sip_ses->video_filter_info);
		audio_filter_info = kms_sip_rtp_filter_create_filtering_info (audio_ssrc, sip_ses->audio_filter_info, AUDIO_RTP_SESSION, continue_audio_stream);
		video_filter_info = kms_sip_rtp_filter_create_filtering_info (video_ssrc, sip_ses->video_filter_info, VIDEO_RTP_SESSION, continue_video_stream);
	} else if (KMS_IS_SIP_SRTP_SESSION (ses)) {
		KmsSipSrtpSession* sip_ses = KMS_SIP_SRTP_SESSION (ses);

		GST_DEBUG ("kms_sip_rtp_endpoint_create_clone_data srtp  audio filter %p, video filter %p", sip_ses->audio_filter_info, sip_ses->video_filter_info);
		audio_filter_info = kms_sip_rtp_filter_create_filtering_info (audio_ssrc, sip_ses->audio_filter_info, AUDIO_RTP_SESSION, continue_audio_stream);
		video_filter_info = kms_sip_rtp_filter_create_filtering_info (video_ssrc, sip_ses->video_filter_info, VIDEO_RTP_SESSION, continue_video_stream);
	}

	data->audio_filter_info = audio_filter_info;
	data->video_filter_info = video_filter_info;
	data->conns = g_hash_table_ref(ses->conns);

	return data;
}

static void
kms_sip_rtp_endpoint_free_clone_data (GList *data)
{
	GList *it = data;

	while (it != NULL) {
		KmsSipRtpEndpointCloneData* data = (KmsSipRtpEndpointCloneData*) it->data;

		if (data->conns != NULL) {
			g_hash_table_unref(data->conns);
			data->conns = NULL;
		}

		it = it->next;
	}

	g_list_free_full (data, g_free);
}

static void
kms_sip_rtp_endpoint_clone_to_new_ep (KmsSipRtpEndpoint *self, KmsSipRtpEndpoint *cloned, const gchar* sdp_str, gboolean continue_audio_stream, gboolean continue_video_stream)
{
	GHashTable * sessions = kms_base_sdp_endpoint_get_sessions (KMS_BASE_SDP_ENDPOINT(self));
	GList *sessionKeys = g_hash_table_get_keys (sessions);
	gint i;
	GList *sessionsData = NULL;
	guint32 remote_audio_ssrc = 0;
	guint32 remote_video_ssrc = 0;
	GstSDPMessage *sdp;

	gst_sdp_message_new (&sdp);
	if (gst_sdp_message_parse_buffer ((const guint8*) sdp_str, strlen (sdp_str), sdp) != GST_SDP_OK)
		GST_ERROR("Could not parse SDP answer");

	if (!kms_sip_rtp_endpoint_get_expected_ssrc (sdp, &remote_audio_ssrc, &remote_video_ssrc)) {
		GST_INFO("Could not find SSRCs on SDP answer, assuming first SSRC different from previous is valid");
	}

	gst_sdp_message_free (sdp);

	// In fact SipRtpEndpoint should have only one session, if not, this loop should be revised
	for (i = 0; i < g_hash_table_size(sessions); i++) {
		gpointer sesKey = sessionKeys->data;
		KmsBaseRtpSession *ses = KMS_BASE_RTP_SESSION (g_hash_table_lookup (sessions, sesKey));
		KmsSipRtpEndpointCloneData *data = kms_sip_rtp_endpoint_create_clone_data (self, ses, remote_audio_ssrc, remote_video_ssrc, continue_audio_stream, continue_video_stream);

		sessionsData = g_list_append (sessionsData, (gpointer)data);
	}
	g_list_free(sessionKeys);

	KMS_ELEMENT_LOCK (cloned);
	if (cloned->priv->sessionData != NULL) {
		kms_sip_rtp_endpoint_free_clone_data (cloned->priv->sessionData);
	}
	cloned->priv->sessionData = sessionsData;
	KMS_ELEMENT_UNLOCK (cloned);
}

static void
kms_sip_rtp_endpoint_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  KmsSipRtpEndpoint *self = KMS_SIP_RTP_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (prop_id) {
	KmsSipRtpEndpointCloneData* clone;
	guint32 ssrc;

	  case PROP_AUDIO_SSRC:
		  clone = kms_sip_rtp_endpoint_get_clone_data (self->priv->sessionData);
		  ssrc = g_value_get_uint (value);

		  if (clone != NULL) {
			  clone->local_audio_ssrc = ssrc;
		  }
		  break;
	  case PROP_VIDEO_SSRC:
		  clone = kms_sip_rtp_endpoint_get_clone_data (self->priv->sessionData);
		  ssrc = g_value_get_uint (value);

		  if (clone != NULL) {
			  clone->local_video_ssrc = ssrc;
		  }
		  break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_sip_rtp_endpoint_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  KmsSipRtpEndpoint *self = KMS_SIP_RTP_ENDPOINT (object);

  KMS_ELEMENT_LOCK (self);

  switch (prop_id) {
	KmsSipRtpEndpointCloneData* clone;

    case PROP_AUDIO_SSRC:
    	clone = kms_sip_rtp_endpoint_get_clone_data (self->priv->sessionData);

    	if (clone != NULL) {
    		g_value_set_uint (value, clone->local_audio_ssrc);
    	}
    	break;
    case PROP_VIDEO_SSRC:
    	clone = kms_sip_rtp_endpoint_get_clone_data (self->priv->sessionData);

    	if (clone != NULL) {
    		g_value_set_uint (value, clone->local_video_ssrc);
    	}
    	break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  KMS_ELEMENT_UNLOCK (self);
}

static void
kms_sip_rtp_endpoint_finalize (GObject * object)
{
  KmsSipRtpEndpoint *self = KMS_SIP_RTP_ENDPOINT (object);

  GST_DEBUG_OBJECT (self, "finalize");

  if (self->priv->use_sdes_cache != NULL)
	  g_free (self->priv->use_sdes_cache);

  if (self->priv->sessionData != NULL)
	  kms_sip_rtp_endpoint_free_clone_data(self->priv->sessionData);

  GST_DEBUG ("Finalizing Sip RTP Endpoint %p", object);

  /* chain up */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
kms_sip_rtp_endpoint_class_init (KmsSipRtpEndpointClass * klass)
{
  GObjectClass *gobject_class;
  KmsBaseSdpEndpointClass *base_sdp_endpoint_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->set_property = kms_sip_rtp_endpoint_set_property;
  gobject_class->get_property = kms_sip_rtp_endpoint_get_property;
  gobject_class->finalize = kms_sip_rtp_endpoint_finalize;

  gstelement_class = GST_ELEMENT_CLASS (klass);
  gst_element_class_set_details_simple (gstelement_class,
      "SipRtpEndpoint",
      "SIP RTP/Stream/RtpEndpoint",
      "Sip Rtp Endpoint element",
      "Saul Pablo Labajo Izquierdo <slabajo@naevatec.com>");
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, PLUGIN_NAME, 0, PLUGIN_NAME);

  base_sdp_endpoint_class = KMS_BASE_SDP_ENDPOINT_CLASS (klass);
  base_sdp_endpoint_class->create_session_internal =
      kms_sip_rtp_endpoint_create_session_internal;
  base_sdp_endpoint_class->start_transport_send =
      kms_sip_rtp_endpoint_start_transport_send;

  base_sdp_endpoint_class->process_answer =
		  kms_sip_rtp_endpoint_process_answer;

  /* Media handler management */
  base_sdp_endpoint_class->create_media_handler =
      kms_sip_rtp_endpoint_create_media_handler;


  base_sdp_endpoint_class->configure_media = kms_sip_rtp_endpoint_configure_media;

  klass->clone_to_new_ep = kms_sip_rtp_endpoint_clone_to_new_ep;

  g_object_class_install_property (gobject_class, PROP_AUDIO_SSRC,
      g_param_spec_uint ("audio-ssrc",
          "Audio SSRC", "Set to assign the local audio SSRC",
          0, G_MAXUINT, DEFAULT_AUDIO_SSRC,
		  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VIDEO_SSRC,
      g_param_spec_uint ("video-ssrc",
          "Video SSRC", "Set to assign the local video SSRC",
		  0, G_MAXUINT, DEFAULT_VIDEO_SSRC,
		  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  obj_signals[SIGNAL_CLONE_TO_NEW_EP] =
      g_signal_new ("clone-to-new-ep",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_ACTION | G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (KmsSipRtpEndpointClass, clone_to_new_ep), NULL, NULL,
      NULL, G_TYPE_NONE, 4, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);


  g_type_class_add_private (klass, sizeof (KmsSipRtpEndpointPrivate));

  // Kind of hack to use GLib type system in an unusual way:
  //  RTPEndpoint implementation is very final in the sense that it does not
  //  intend to be subclassed, this makes difficult to reimplement virtual
  //  methods that need chaining up like create_session_internal. The only way
  //  is to call directly the virtual method in the grandparent class
  //  Well, there is another way, to enrich base class implementation to allow
  //  subclasses to reimplement the virtual method (in the particular case of
  //  create_session_internal just need to skip session creation if already created.
  // TODO: When integrate on kms-elements get rid off this hack changing kms_rtp_endpoint_create_session_internal
  GType type =   g_type_parent  (g_type_parent (G_TYPE_FROM_CLASS (klass)));
  // TODO: This introduces a memory leak, this is reserved and never freed, but it is just a pointer (64 bits)
  //       A possible alternative would be to implement the class_finalize method
  gpointer typePointer = g_type_class_ref(type);
  base_sdp_endpoint_type = KMS_BASE_SDP_ENDPOINT_CLASS(typePointer);
}

/* TODO: not add abs-send-time extmap */

static void
kms_sip_rtp_endpoint_init (KmsSipRtpEndpoint * self)
{
  self->priv = KMS_SIP_RTP_ENDPOINT_GET_PRIVATE (self);

  self->priv->use_sdes_cache = NULL;
  self->priv->sessionData = NULL;

  GST_DEBUG ("Initialized RTP Endpoint %p", self);
}
