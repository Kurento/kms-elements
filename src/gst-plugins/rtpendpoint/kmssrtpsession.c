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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "kmssrtpsession.h"

#define GST_DEFAULT_NAME "kmssrtpsession"
#define GST_CAT_DEFAULT kms_srtp_session_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_srtp_session_parent_class parent_class

G_DEFINE_TYPE (KmsSrtpSession, kms_srtp_session, KMS_TYPE_BASE_RTP_SESSION);

KmsSrtpSession *
kms_srtp_session_new (KmsBaseSdpEndpoint * ep, guint id,
    KmsIRtpSessionManager * manager, gboolean use_ipv6)
{
  GObject *obj;
  KmsSrtpSession *self;

  obj = g_object_new (KMS_TYPE_SRTP_SESSION, NULL);
  self = KMS_SRTP_SESSION (obj);
  KMS_SRTP_SESSION_CLASS (G_OBJECT_GET_CLASS (self))->post_constructor
      (self, ep, id, manager, use_ipv6);

  return self;
}

/* Connection management begin */

KmsRtpBaseConnection *
kms_srtp_session_get_connection (KmsSrtpSession * self, SdpMediaConfig * mconf)
{
  KmsBaseRtpSession *base_rtp_sess = KMS_BASE_RTP_SESSION (self);
  KmsIRtpConnection *conn;

  conn = kms_base_rtp_session_get_connection (base_rtp_sess, mconf);
  if (conn == NULL) {
    return NULL;
  }

  return KMS_RTP_BASE_CONNECTION (conn);
}

static KmsIRtpConnection *
kms_srtp_session_create_connection (KmsBaseRtpSession * base_rtp_sess,
    SdpMediaConfig * mconf, const gchar * name, guint16 min_port,
    guint16 max_port)
{
  KmsSrtpConnection *conn = kms_srtp_connection_new (min_port, max_port,
      KMS_SRTP_SESSION (base_rtp_sess)->use_ipv6);

  return KMS_I_RTP_CONNECTION (conn);
}

/* Connection management end */

static void
kms_srtp_session_post_constructor (KmsSrtpSession * self,
    KmsBaseSdpEndpoint * ep, guint id, KmsIRtpSessionManager * manager,
    gboolean use_ipv6)
{
  KmsBaseRtpSession *base_rtp_session = KMS_BASE_RTP_SESSION (self);

  self->use_ipv6 = use_ipv6;
  KMS_BASE_RTP_SESSION_CLASS (parent_class)->post_constructor (base_rtp_session,
      ep, id, manager);
}

static void
kms_srtp_session_init (KmsSrtpSession * self)
{
  /* nothing to do */
}

static void
kms_srtp_session_class_init (KmsSrtpSessionClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  KmsBaseRtpSessionClass *base_rtp_session_class;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  klass->post_constructor = kms_srtp_session_post_constructor;

  base_rtp_session_class = KMS_BASE_RTP_SESSION_CLASS (klass);
  /* Connection management */
  base_rtp_session_class->create_connection =
      kms_srtp_session_create_connection;

  gst_element_class_set_details_simple (gstelement_class,
      "SrtpSession",
      "Generic",
      "Base bin to manage elements related with a SRTP session.",
      "antiago Carot-Nemesio <sancane _dot_ kurento _at_ gmail _dot_ com>");
}
