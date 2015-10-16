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

#ifndef __KMS_RTP_CONNECTION_H__
#define __KMS_RTP_CONNECTION_H__

#include "kmsrtpbaseconnection.h"

G_BEGIN_DECLS

#define KMS_TYPE_RTP_CONNECTION \
  (kms_rtp_connection_get_type())
#define KMS_RTP_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_RTP_CONNECTION,KmsRtpConnection))
#define KMS_RTP_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_RTP_CONNECTION,KmsRtpConnectionClass))
#define KMS_IS_RTP_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_RTP_CONNECTION))
#define KMS_IS_RTP_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_RTP_CONNECTION))
#define KMS_RTP_CONNECTION_CAST(obj) ((KmsRtpConnection*)(obj))

typedef struct _KmsRtpConnectionPrivate KmsRtpConnectionPrivate;
typedef struct _KmsRtpConnection KmsRtpConnection;
typedef struct _KmsRtpConnectionClass KmsRtpConnectionClass;

struct _KmsRtpConnection
{
  KmsRtpBaseConnection parent;

  KmsRtpConnectionPrivate *priv;
};

struct _KmsRtpConnectionClass
{
  KmsRtpBaseConnectionClass parent_class;
};

GType kms_rtp_connection_get_type (void);

KmsRtpConnection * kms_rtp_connection_new (guint16 min_port, guint16 max_port);

G_END_DECLS
#endif /* __KMS_RTP_CONNECTION_H__ */
