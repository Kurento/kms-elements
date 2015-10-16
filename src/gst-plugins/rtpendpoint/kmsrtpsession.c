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

#include "kmsrtpsession.h"

#define GST_DEFAULT_NAME "kmsrtpsession"
#define GST_CAT_DEFAULT kms_rtp_session_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define kms_rtp_session_parent_class parent_class
G_DEFINE_TYPE (KmsRtpSession, kms_rtp_session, KMS_TYPE_BASE_RTP_SESSION);

KmsRtpSession *
kms_rtp_session_new (KmsBaseSdpEndpoint * ep, guint id,
    KmsIRtpSessionManager * manager)
{
  GObject *obj;
  KmsRtpSession *self;

  obj = g_object_new (KMS_TYPE_RTP_SESSION, NULL);
  self = KMS_RTP_SESSION (obj);
  KMS_RTP_SESSION_CLASS (G_OBJECT_GET_CLASS (self))->post_constructor
      (self, ep, id, manager);

  return self;
}

/* Connection management begin */

KmsRtpBaseConnection *
kms_rtp_session_get_connection (KmsRtpSession * self, SdpMediaConfig * mconf)
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
kms_rtp_session_create_connection (KmsBaseRtpSession * base_rtp_sess,
    SdpMediaConfig * mconf, const gchar * name, guint16 min_port,
    guint16 max_port)
{
  KmsRtpConnection *conn = kms_rtp_connection_new ();

  return KMS_I_RTP_CONNECTION (conn);
}

/* Connection management end */

static void
kms_rtp_session_post_constructor (KmsRtpSession * self,
    KmsBaseSdpEndpoint * ep, guint id, KmsIRtpSessionManager * manager)
{
  KmsBaseRtpSession *base_rtp_session = KMS_BASE_RTP_SESSION (self);

  KMS_BASE_RTP_SESSION_CLASS
      (kms_rtp_session_parent_class)->post_constructor (base_rtp_session, ep,
      id, manager);
}

static void
kms_rtp_session_init (KmsRtpSession * self)
{
  /* nothing to do */
}

static void
kms_rtp_session_class_init (KmsRtpSessionClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  KmsBaseRtpSessionClass *base_rtp_session_class;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  klass->post_constructor = kms_rtp_session_post_constructor;

  base_rtp_session_class = KMS_BASE_RTP_SESSION_CLASS (klass);
  /* Connection management */
  base_rtp_session_class->create_connection = kms_rtp_session_create_connection;

  gst_element_class_set_details_simple (gstelement_class,
      "RtpSession",
      "Generic",
      "Base bin to manage elements related with a RTP session.",
      "Miguel París Díaz <mparisdiaz@gmail.com>");
}
