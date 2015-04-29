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

#include <gst/gst.h>
#include <gio/gio.h>
#include "gstiostream.h"

typedef struct _KmsGstDtlsConnection KmsGstDtlsConnection;
typedef struct _KmsGstDtlsConnectionClass KmsGstDtlsConnectionClass;
typedef struct _KmsGstDtlsBase KmsGstDtlsBase;
typedef struct _KmsGstDtlsBaseClass KmsGstDtlsBaseClass;
typedef struct _KmsGstDtlsEnc KmsGstDtlsEnc;
typedef struct _KmsGstDtlsEncClass KmsGstDtlsEncClass;
typedef struct _KmsGstDtlsDec KmsGstDtlsDec;
typedef struct _KmsGstDtlsDecClass KmsGstDtlsDecClass;


#include "gstdtlsbase.h"
#include "gstdtlsenc.h"
#include "gstdtlsdec.h"

#define GST_TYPE_DTLS_CONNECTION            (gst_dtls_connection_get_type())
#define GST_DTLS_CONNECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DTLS_CONNECTION,KmsGstDtlsConnection))
#define GST_IS_DTLS_CONNECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DTLS_CONNECTION))
#define GST_DTLS_CONNECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_DTLS_CONNECTION,KmsGstDtlsConnectionClass))
#define GST_IS_DTLS_CONNECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_DTLS_CONNECTION))
#define GST_DTLS_CONNECTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_DTLS_CONNECTION,KmsGstDtlsConnectionClass))

/**
 * KmsGstDtlsConnection:
 *
 * The adder object structure.
 */
struct _KmsGstDtlsConnection
{
  GObject parent;

  GTlsConnection *conn;
  GstIOStream *base_stream;

  /*< private >*/

  gboolean is_client;

  KmsGstDtlsEnc *enc;
  KmsGstDtlsDec *dec;

  GMutex lock;

  gboolean enc_playing;
  gboolean dec_playing;

  gboolean playing;
};

struct _KmsGstDtlsConnectionClass
{
  GObjectClass parent_class;
};

GType gst_dtls_connection_get_type (void);


KmsGstDtlsConnection *
gst_dtls_connection_get_by_id (const gchar * id,  gboolean is_client,
    KmsGstDtlsBase * encdec);

void
gst_dtls_connection_set_playing (KmsGstDtlsConnection *conn,
    KmsGstDtlsBase * encdec, gboolean playing);
