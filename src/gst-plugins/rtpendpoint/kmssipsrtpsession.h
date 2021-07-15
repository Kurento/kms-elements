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

#ifndef __KMS_SIP_SRTP_SESSION_H__
#define __KMS_SIP_SRTP_SESSION_H__

#include <gst/gst.h>
#include <kmssrtpsession.h>
#include "kmssrtpconnection.h"
#include "kmsrtpfilterutils.h"

G_BEGIN_DECLS

typedef struct _KmsIRtpSessionManager KmsIRtpSessionManager;

/* #defines don't like whitespacey bits */
#define KMS_TYPE_SIP_SRTP_SESSION \
  (kms_sip_srtp_session_get_type())
#define KMS_SIP_SRTP_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_SIP_SRTP_SESSION,KmsSipSrtpSession))
#define KMS_SIP_SRTP_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_SIP_SRTP_SESSION,KmsSipSrtpSessionClass))
#define KMS_IS_SIP_SRTP_SESSION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_SIP_SRTP_SESSION))
#define KMS_IS_SIP_SRTP_SESSION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_SIP_SRTP_SESSION))
#define KMS_SIP_SRTP_SESSION_CAST(obj) ((KmsSipSrtpSession*)(obj))

typedef struct _KmsSipSrtpSession KmsSipSrtpSession;
typedef struct _KmsSipSrtpSessionClass KmsSipSrtpSessionClass;
typedef struct _KmsSipSrtpSessionPrivate KmsSipSrtpSessionPrivate;


struct _KmsSipSrtpSession
{
  KmsSrtpSession parent;

  gboolean use_ipv6;

  SipFilterSsrcInfo* audio_filter_info;
  SipFilterSsrcInfo* video_filter_info;

  KmsSipSrtpSessionPrivate *priv;
};

struct _KmsSipSrtpSessionClass
{
  KmsSrtpSessionClass parent_class;

  /* signals */
  void (*clone_connections) (KmsSipSrtpSession *self, GHashTable *conns);

  void (*store_rtp_filtering_info) (KmsSipSrtpSession *ses, KmsSrtpConnection *conn, gulong rtp_probe, gulong rtcp_probe);

};

GType kms_sip_srtp_session_get_type (void);

KmsSipSrtpSession *kms_sip_srtp_session_new (KmsBaseSdpEndpoint * ep, guint id, KmsIRtpSessionManager * manager, gboolean use_ipv6);

G_END_DECLS
#endif /* __KMS_SIP_SRTP_SESSION_H__ */
