/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
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
#ifndef _KMS_JPEG_MUXER_H_
#define _KMS_JPEG_MUXER_H_

#include <gst/gst.h>
#include "kmsbasemediamuxer.h"

G_BEGIN_DECLS
#define KMS_TYPE_JPEG_MUXER               \
  (kms_jpeg_muxer_get_type())
#define KMS_JPEG_MUXER_CAST(obj)          \
  ((KmsJPEGMuxer *)(obj))
#define KMS_JPEG_MUXER(obj)               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),    \
  KMS_TYPE_JPEG_MUXER,KmsJPEGMuxer))
#define KMS_JPEG_MUXER_CLASS(klass)        \
  (G_TYPE_CHECK_CLASS_CAST((klass),      \
  KMS_TYPE_JPEG_MUXER,                     \
  KmsJPEGMuxerClass))
#define KMS_IS_JPEG_MUXER(obj)             \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),     \
  KMS_TYPE_JPEG_MUXER))
#define KMS_IS_JPEG_MUXER_CLASS(klass)     \
  (G_TYPE_CHECK_CLASS_TYPE((klass),      \
  KMS_TYPE_JPEG_MUXER))

#define KMS_JPEG_MUXER_PROFILE "profile"

typedef struct _KmsJPEGMuxer KmsJPEGMuxer;
typedef struct _KmsJPEGMuxerClass KmsJPEGMuxerClass;
typedef struct _KmsJPEGMuxerPrivate KmsJPEGMuxerPrivate;

struct _KmsJPEGMuxer
{
  KmsBaseMediaMuxer parent;

  /*< private > */
  KmsJPEGMuxerPrivate *priv;
};

struct _KmsJPEGMuxerClass
{
  KmsBaseMediaMuxerClass parent_class;
};

GType kms_jpeg_muxer_get_type ();

KmsJPEGMuxer * kms_jpeg_muxer_new (const char *optname1, ...);

G_END_DECLS
#endif
