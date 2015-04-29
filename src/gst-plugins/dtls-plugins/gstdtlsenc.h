/*
 * GStreamer
 *
 *  Copyright 2013 Collabora Ltd
 *   @author: Olivier Crete <olivier.crete@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */


#ifndef __GST_DTLS_ENC_H__
#define __GST_DTLS_ENC_H__

#include <gst/gst.h>

#include "gstdtlsbase.h"

G_BEGIN_DECLS
#define GST_TYPE_DTLS_ENC            (gst_dtls_enc_get_type())
#define GST_DTLS_ENC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DTLS_ENC,KmsGstDtlsEnc))
#define GST_IS_DTLS_ENC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DTLS_ENC))
#define GST_DTLS_ENC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_DTLS_ENC,KmsGstDtlsEncClass))
#define GST_IS_DTLS_ENC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_DTLS_ENC))
#define GST_DTLS_ENC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_DTLS_ENC,KmsGstDtlsEncClass))


/**
 * KmsGstDtlsEnc:
 *
 * The adder object structure.
 */
struct _KmsGstDtlsEnc
{
  /*< private >*/
  KmsGstDtlsBase parent;

  GstBuffer *src_buffer;
  GThread *running_thread;
};

struct _KmsGstDtlsEncClass
{
  KmsGstDtlsBaseClass parent_class;
};

GType gst_dtls_enc_get_type (void);

G_END_DECLS
#endif /* __GST_DTLS_ENC_H__ */
