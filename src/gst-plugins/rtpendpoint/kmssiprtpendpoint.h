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
#ifndef __KMS_SIP_RTP_ENDPOINT_H__
#define __KMS_SIP_RTP_ENDPOINT_H__

#include <gio/gio.h>
#include <gst/gst.h>
#include <kmsrtpendpoint.h>

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define KMS_TYPE_SIP_RTP_ENDPOINT \
  (kms_sip_rtp_endpoint_get_type())
#define KMS_SIP_RTP_ENDPOINT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_SIP_RTP_ENDPOINT,KmsSipRtpEndpoint))
#define KMS_SIP_RTP_ENDPOINT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_SIP_RTP_ENDPOINT,KmsSipRtpEndpointClass))
#define KMS_IS_SIP_RTP_ENDPOINT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_SIP_RTP_ENDPOINT))
#define KMS_IS_SIP_RTP_ENDPOINT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SIP_RTP_ENDPOINT))
#define KMS_SIP_RTP_ENDPOINT_CAST(obj) ((KmsSipRtpEndpoint*)(obj))
typedef struct _KmsSipRtpEndpoint KmsSipRtpEndpoint;
typedef struct _KmsSipRtpEndpointClass KmsSipRtpEndpointClass;
typedef struct _KmsSipRtpEndpointPrivate KmsSipRtpEndpointPrivate;

#define KMS_SIP_RTP_ENDPOINT_LOCK(elem) \
  (g_rec_mutex_lock (&KMS_SIP_RTP_ENDPOINT_CAST ((elem))->media_mutex))
#define KMS_SIP_RTP_ENDPOINT_UNLOCK(elem) \
  (g_rec_mutex_unlock (&KMS_SIP_RTP_ENDPOINT_CAST ((elem))->media_mutex))

struct _KmsSipRtpEndpoint
{
  KmsRtpEndpoint parent;

  KmsSipRtpEndpointPrivate *priv;
};

struct _KmsSipRtpEndpointClass
{

	KmsRtpEndpointClass parent_class;


  /* signals */
  void (*clone_to_new_ep) (KmsSipRtpEndpoint *obj, KmsSipRtpEndpoint *cloned, const gchar* sdp, gboolean, gboolean);
};

GType kms_sip_rtp_endpoint_get_type (void);

gboolean kms_sip_rtp_endpoint_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __KMS_SIP_RTP_ENDPOINT_H__ */
