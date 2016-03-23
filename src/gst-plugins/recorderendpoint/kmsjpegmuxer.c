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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <commons/kms-core-enumtypes.h>
#include <commons/kmsrecordingprofile.h>
#include <commons/kmsutils.h>
#include <commons/kmsagnosticcaps.h>

#include "kmsjpegmuxer.h"

#define OBJECT_NAME "jpegmuxer"
#define KMS_JPEG_MUXER_NAME OBJECT_NAME

#define parent_class kms_jpeg_muxer_parent_class
#define KEY_JPEG_MUXER_PAD_PROBE_ID "kms-muxing-pipeline-key-probe-id"

GST_DEBUG_CATEGORY_STATIC (kms_jpeg_muxer_debug_category);
#define GST_CAT_DEFAULT kms_jpeg_muxer_debug_category

#define KMS_JPEG_MUXER_GET_PRIVATE(obj) (  \
  G_TYPE_INSTANCE_GET_PRIVATE (          \
    (obj),                               \
    KMS_TYPE_JPEG_MUXER,                   \
    KmsJPEGMuxerPrivate                    \
  )                                      \
)

struct _KmsJPEGMuxerPrivate
{
  GstElement *videosrc;
  GstElement *mux;
  GstElement *sink;
  GstClockTime lastVideoPts;

  gboolean sink_signaled;
};

typedef struct _BufferListItData
{
  KmsJPEGMuxer *self;
  GstElement *elem;
} BufferListItData;

G_DEFINE_TYPE_WITH_CODE (KmsJPEGMuxer, kms_jpeg_muxer,
    KMS_TYPE_BASE_MEDIA_MUXER,
    GST_DEBUG_CATEGORY_INIT (kms_jpeg_muxer_debug_category, OBJECT_NAME,
        0, "debug category for muxing pipeline object"));

GstStateChangeReturn
kms_jpeg_muxer_set_state (KmsBaseMediaMuxer * obj, GstState state)
{
  KmsJPEGMuxer *self = KMS_JPEG_MUXER (obj);

  if (state == GST_STATE_NULL || state == GST_STATE_READY) {
    self->priv->lastVideoPts = 0;
  }

  return KMS_BASE_MEDIA_MUXER_CLASS (parent_class)->set_state (obj, state);
}

static GstElement *
kms_jpeg_muxer_add_src (KmsBaseMediaMuxer * obj, KmsMediaType type,
    const gchar * id)
{
  KmsJPEGMuxer *self = KMS_JPEG_MUXER (obj);
  GstElement *sink = NULL, *appsrc = NULL;

  KMS_BASE_MEDIA_MUXER_LOCK (self);

  switch (type) {
    case KMS_MEDIA_TYPE_VIDEO:
      appsrc = self->priv->videosrc;
      break;
    default:
      GST_WARNING_OBJECT (obj, "Unsupported media type %u", type);
  }

  if (appsrc != NULL && !self->priv->sink_signaled) {
    sink = g_object_ref (self->priv->sink);
    self->priv->sink_signaled = TRUE;
  }

  KMS_BASE_MEDIA_MUXER_UNLOCK (self);

  if (sink != NULL) {
    KMS_BASE_MEDIA_MUXER_GET_CLASS (self)->emit_on_sink_added
        (KMS_BASE_MEDIA_MUXER (self), sink);
    g_object_unref (sink);
  }

  return appsrc;
}

static gboolean
kms_jpeg_muxer_remove_src (KmsBaseMediaMuxer * obj, const gchar * id)
{
  /* Nothing to remove */
  return FALSE;
}

static void
kms_jpeg_muxer_class_init (KmsJPEGMuxerClass * klass)
{
  KmsBaseMediaMuxerClass *basemediamuxerclass;

  basemediamuxerclass = KMS_BASE_MEDIA_MUXER_CLASS (klass);
  basemediamuxerclass->set_state = kms_jpeg_muxer_set_state;
  basemediamuxerclass->add_src = kms_jpeg_muxer_add_src;
  basemediamuxerclass->remove_src = kms_jpeg_muxer_remove_src;

  g_type_class_add_private (klass, sizeof (KmsJPEGMuxerPrivate));
}

static void
kms_jpeg_muxer_init (KmsJPEGMuxer * self)
{
  self->priv = KMS_JPEG_MUXER_GET_PRIVATE (self);

  self->priv->lastVideoPts = G_GUINT64_CONSTANT (0);
}

static gboolean
kms_jpeg_muxer_check_pts (GstBuffer ** buffer, GstClockTime * lastPts)
{
  if (G_UNLIKELY (!GST_BUFFER_PTS_IS_VALID ((*buffer)))) {
    return TRUE;
  } else if (G_LIKELY (*lastPts <= (*buffer)->pts)) {
    *lastPts = (*buffer)->pts;

    return TRUE;
  } else {
    GST_WARNING ("Buffer pts %" G_GUINT64_FORMAT " is older than last pts %"
        G_GUINT64_FORMAT, GST_BUFFER_PTS (*buffer), *lastPts);

    return FALSE;
  }
}

static gboolean
kms_jpeg_muxer_injector (KmsJPEGMuxer * self, GstElement * elem,
    GstBuffer ** buffer)
{
  GstClockTime *lastPts = NULL;

  if (elem == self->priv->videosrc) {
    lastPts = &self->priv->lastVideoPts;
  }

  if (G_LIKELY (lastPts)) {
    gboolean ret;

    KMS_BASE_MEDIA_MUXER_LOCK (self);
    ret = kms_jpeg_muxer_check_pts (buffer, lastPts);
    KMS_BASE_MEDIA_MUXER_UNLOCK (self);

    return ret;
  }

  return FALSE;
}

static gboolean
kms_jpeg_muxer_injector_probe_it (GstBuffer ** buffer, guint idx,
    gpointer user_data)
{
  BufferListItData *data = user_data;

  return kms_jpeg_muxer_injector (data->self, data->elem, buffer);
}

static GstPadProbeReturn
kms_jpeg_muxer_injector_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer self)
{
  GstElement *elem;
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;

  if (info->type & GST_PAD_PROBE_TYPE_BLOCK) {
    return GST_PAD_PROBE_PASS;
  }

  elem = gst_pad_get_parent_element (pad);

  g_return_val_if_fail (elem != NULL, GST_PAD_PROBE_OK);

  if (info->type & GST_PAD_PROBE_TYPE_BUFFER_LIST) {
    GstBufferList *list = GST_PAD_PROBE_INFO_BUFFER_LIST (info);
    BufferListItData itData;

    itData.self = self;
    itData.elem = elem;

    if (G_UNLIKELY (!gst_buffer_list_foreach (list,
                kms_jpeg_muxer_injector_probe_it, &itData))) {
      ret = GST_PAD_PROBE_DROP;
    }
  } else if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer **buffer = (GstBuffer **) & info->data;

    if (G_UNLIKELY (!kms_jpeg_muxer_injector (self, elem, buffer))) {
      ret = GST_PAD_PROBE_DROP;
    }
  }

  g_object_unref (elem);

  return ret;
}

static void
kms_jpeg_muxer_add_injector_probe (KmsJPEGMuxer * self, GstElement * appsrc)
{
  GstPad *src;

  src = gst_element_get_static_pad (appsrc, "src");

  g_return_if_fail (src != NULL);

  gst_pad_add_probe (src,
      GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST,
      kms_jpeg_muxer_injector_probe, self, NULL);

  g_object_unref (src);
}

static GstElement *
kms_jpeg_muxer_create_muxer (KmsJPEGMuxer * self)
{
  switch (KMS_BASE_MEDIA_MUXER_GET_PROFILE (self)) {
    case KMS_RECORDING_PROFILE_JPEG:{
      return gst_element_factory_make ("jifmux", NULL);
    }
    default:
      GST_ERROR_OBJECT (self, "No valid recording profile set");
      return NULL;
  }
}

static void
kms_jpeg_muxer_prepare_pipeline (KmsJPEGMuxer * self)
{
  self->priv->videosrc = gst_element_factory_make ("appsrc", "videoSrc");

  self->priv->sink =
      KMS_BASE_MEDIA_MUXER_GET_CLASS (self)->create_sink (KMS_BASE_MEDIA_MUXER
      (self), KMS_BASE_MEDIA_MUXER_GET_URI (self));

  g_object_set (self->priv->videosrc, "format", 3 /* GST_FORMAT_TIME */ , NULL);

  kms_jpeg_muxer_add_injector_probe (self, self->priv->videosrc);

  self->priv->mux = kms_jpeg_muxer_create_muxer (self);

  gst_bin_add_many (GST_BIN (KMS_BASE_MEDIA_MUXER_GET_PIPELINE (self)),
      self->priv->videosrc, self->priv->mux, self->priv->sink, NULL);

  if (!gst_element_link (self->priv->mux, self->priv->sink)) {
    GST_ERROR_OBJECT (self, "Could not link elements: %"
        GST_PTR_FORMAT ", %" GST_PTR_FORMAT, self->priv->mux, self->priv->sink);
  }

  if (kms_recording_profile_supports_type (KMS_BASE_MEDIA_MUXER_GET_PROFILE
          (self), KMS_ELEMENT_PAD_TYPE_VIDEO)) {
    if (!gst_element_link_pads (self->priv->videosrc, "src", self->priv->mux,
            "sink")) {
      GST_ERROR_OBJECT (self,
          "Could not link video elements: %" GST_PTR_FORMAT ", %" GST_PTR_FORMAT,
          self->priv->videosrc, self->priv->mux);
    }
  }
}

KmsJPEGMuxer *
kms_jpeg_muxer_new (const char *optname1, ...)
{
  KmsJPEGMuxer *obj;

  va_list ap;

  va_start (ap, optname1);
  obj = KMS_JPEG_MUXER (g_object_new_valist (KMS_TYPE_JPEG_MUXER, optname1, ap));
  va_end (ap);

  kms_jpeg_muxer_prepare_pipeline (obj);

  return obj;
}
