/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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

#include "kmscompositemixer.h"
#include <commons/kmsagnosticcaps.h>
#include <commons/kmshubport.h>
#include <commons/kmsloop.h>
#include <commons/kmsrefstruct.h>

#define N_ELEMENTS_WIDTH 2

#define PLUGIN_NAME "compositemixer"

#define KMS_COMPOSITE_MIXER_LOCK(mixer) \
  (g_rec_mutex_lock (&( (KmsCompositeMixer *) mixer)->priv->mutex))

#define KMS_COMPOSITE_MIXER_UNLOCK(mixer) \
  (g_rec_mutex_unlock (&( (KmsCompositeMixer *) mixer)->priv->mutex))

GST_DEBUG_CATEGORY_STATIC (kms_composite_mixer_debug_category);
#define GST_CAT_DEFAULT kms_composite_mixer_debug_category

#define KMS_COMPOSITE_MIXER_GET_PRIVATE(obj) (\
  G_TYPE_INSTANCE_GET_PRIVATE (               \
    (obj),                                    \
    KMS_TYPE_COMPOSITE_MIXER,                 \
    KmsCompositeMixerPrivate                  \
  )                                           \
)

#define AUDIO_SINK_PAD_PREFIX_COMP "audio_sink_"
#define VIDEO_SINK_PAD_PREFIX_COMP "video_sink_"
#define AUDIO_SRC_PAD_PREFIX_COMP "audio_src_"
#define VIDEO_SRC_PAD_PREFIX_COMP "video_src_"
#define AUDIO_SINK_PAD_NAME_COMP AUDIO_SINK_PAD_PREFIX_COMP "%u"
#define VIDEO_SINK_PAD_NAME_COMP VIDEO_SINK_PAD_PREFIX_COMP "%u"
#define AUDIO_SRC_PAD_NAME_COMP AUDIO_SRC_PAD_PREFIX_COMP "%u"
#define VIDEO_SRC_PAD_NAME_COMP VIDEO_SRC_PAD_PREFIX_COMP "%u"

static GstStaticPadTemplate audio_sink_factory =
GST_STATIC_PAD_TEMPLATE (AUDIO_SINK_PAD_NAME_COMP,
    GST_PAD_SINK,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_RAW_AUDIO_CAPS)
    );

static GstStaticPadTemplate video_sink_factory =
GST_STATIC_PAD_TEMPLATE (VIDEO_SINK_PAD_NAME_COMP,
    GST_PAD_SINK,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_RAW_VIDEO_CAPS)
    );

static GstStaticPadTemplate audio_src_factory =
GST_STATIC_PAD_TEMPLATE (AUDIO_SRC_PAD_NAME_COMP,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_RAW_AUDIO_CAPS)
    );

static GstStaticPadTemplate video_src_factory =
GST_STATIC_PAD_TEMPLATE (VIDEO_SRC_PAD_NAME_COMP,
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS (KMS_AGNOSTIC_RAW_VIDEO_CAPS)
    );

struct _KmsCompositeMixerPrivate
{
  GstElement *videomixer;
  GstElement *audiomixer;
  GstElement *videotestsrc;
  GHashTable *ports;
  GstElement *mixer_audio_agnostic;
  GstElement *mixer_video_agnostic;
  KmsLoop *loop;
  GRecMutex mutex;
  gint n_elems;
  gint output_width, output_height;
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (KmsCompositeMixer, kms_composite_mixer,
    KMS_TYPE_BASE_HUB,
    GST_DEBUG_CATEGORY_INIT (kms_composite_mixer_debug_category, PLUGIN_NAME,
        0, "debug category for compositemixer element"));

typedef struct _KmsCompositeMixerData
{
  KmsRefStruct parent;
  gint id;
  KmsCompositeMixer *mixer;
  GstElement *capsfilter;
  GstElement *input_capsfilter;
  GstElement *videoconvert;
  GstElement *videorate;
  GstElement *queue;
  GstElement *videoscale;
  gboolean input;
  gboolean removing;
  gboolean eos_managed;
  gint probe_id;
  gint link_probe_id;
  GstPad *video_mixer_pad;
  GstPad *videoconvert_sink_pad;
} KmsCompositeMixerData;

#define KMS_COMPOSITE_MIXER_REF(data) \
  kms_ref_struct_ref (KMS_REF_STRUCT_CAST (data))
#define KMS_COMPOSITE_MIXER_UNREF(data) \
  kms_ref_struct_unref (KMS_REF_STRUCT_CAST (data))

static void
kms_destroy_composite_mixer_data (KmsCompositeMixerData * data)
{
  g_slice_free (KmsCompositeMixerData, data);
}

static KmsCompositeMixerData *
kms_create_composite_mixer_data ()
{
  KmsCompositeMixerData *data;

  data = g_slice_new0 (KmsCompositeMixerData);
  kms_ref_struct_init (KMS_REF_STRUCT_CAST (data),
      (GDestroyNotify) kms_destroy_composite_mixer_data);

  return data;
}

static gint
compare_port_data (gconstpointer a, gconstpointer b)
{
  KmsCompositeMixerData *port_data_a = (KmsCompositeMixerData *) a;
  KmsCompositeMixerData *port_data_b = (KmsCompositeMixerData *) b;

  return port_data_a->id - port_data_b->id;
}

static void
kms_composite_mixer_recalculate_sizes (gpointer data)
{
  KmsCompositeMixer *self = KMS_COMPOSITE_MIXER (data);
  GstCaps *filtercaps;
  gint width, height, top, left, counter;
  GList *l;
  GList *values = g_hash_table_get_values (self->priv->ports);

  counter = 0;
  values = g_list_sort (values, compare_port_data);

  for (l = values; l != NULL; l = l->next) {
    KmsCompositeMixerData *port_data = l->data;

    if (port_data->input == FALSE) {
      continue;
    }

    if (self->priv->n_elems == 1) {
      width = self->priv->output_width;
    } else {
      width = self->priv->output_width / N_ELEMENTS_WIDTH;
    }

    if (self->priv->n_elems < N_ELEMENTS_WIDTH) {
      height = self->priv->output_height;
    } else {
      height =
          self->priv->output_height / ((self->priv->n_elems /
              N_ELEMENTS_WIDTH) + (self->priv->n_elems % N_ELEMENTS_WIDTH));
    }
    filtercaps =
        gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, "AYUV",
        "width", G_TYPE_INT, width, "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
    g_object_set (port_data->capsfilter, "caps", filtercaps, NULL);
    gst_caps_unref (filtercaps);

    top = ((counter / N_ELEMENTS_WIDTH) * height);
    left = ((counter % N_ELEMENTS_WIDTH) * width);

    g_object_set (port_data->video_mixer_pad, "xpos", left, "ypos", top,
        "alpha", 1.0, NULL);
    counter++;

    GST_DEBUG ("counter %d id_port %d ", counter, port_data->id);
    GST_DEBUG ("top %d left %d width %d height %d", top, left, width, height);
  }
  g_list_free (values);
}

static gboolean
remove_elements_from_pipeline (KmsCompositeMixerData * port_data)
{
  KmsCompositeMixer *self = port_data->mixer;

  KMS_COMPOSITE_MIXER_LOCK (self);

  gst_element_unlink (port_data->capsfilter, self->priv->videomixer);

  if (port_data->video_mixer_pad != NULL) {
    gst_element_release_request_pad (self->priv->videomixer,
        port_data->video_mixer_pad);
    g_object_unref (port_data->video_mixer_pad);
    port_data->video_mixer_pad = NULL;
  }

  g_object_unref (port_data->videoconvert_sink_pad);

  gst_bin_remove_many (GST_BIN (self),
      g_object_ref (port_data->input_capsfilter),
      g_object_ref (port_data->videoconvert),
      g_object_ref (port_data->videoscale),
      g_object_ref (port_data->capsfilter), g_object_ref (port_data->videorate),
      g_object_ref (port_data->queue), NULL);

  kms_base_hub_unlink_video_src (KMS_BASE_HUB (self), port_data->id);

  KMS_COMPOSITE_MIXER_UNLOCK (self);

  gst_element_set_state (port_data->input_capsfilter, GST_STATE_NULL);
  gst_element_set_state (port_data->videoconvert, GST_STATE_NULL);
  gst_element_set_state (port_data->videoscale, GST_STATE_NULL);
  gst_element_set_state (port_data->videorate, GST_STATE_NULL);
  gst_element_set_state (port_data->capsfilter, GST_STATE_NULL);
  gst_element_set_state (port_data->queue, GST_STATE_NULL);

  g_object_unref (port_data->input_capsfilter);
  g_object_unref (port_data->videoconvert);
  g_object_unref (port_data->videoscale);
  g_object_unref (port_data->videorate);
  g_object_unref (port_data->capsfilter);
  g_object_unref (port_data->queue);

  port_data->videoconvert_sink_pad = NULL;
  port_data->input_capsfilter = NULL;
  port_data->videoconvert = NULL;
  port_data->videoscale = NULL;
  port_data->capsfilter = NULL;
  port_data->videorate = NULL;
  port_data->queue = NULL;

  return G_SOURCE_REMOVE;
}

static GstPadProbeReturn
cb_EOS_received (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  KmsCompositeMixerData *port_data = (KmsCompositeMixerData *) data;
  KmsCompositeMixer *self = port_data->mixer;
  GstEvent *event;

  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_EVENT (info)) != GST_EVENT_EOS) {
    return GST_PAD_PROBE_OK;
  }

  KMS_COMPOSITE_MIXER_LOCK (self);

  if (!port_data->removing) {
    port_data->eos_managed = TRUE;
    KMS_COMPOSITE_MIXER_UNLOCK (self);
    return GST_PAD_PROBE_OK;
  }

  if (port_data->probe_id > 0) {
    gst_pad_remove_probe (pad, port_data->probe_id);
    port_data->probe_id = 0;
  }

  KMS_COMPOSITE_MIXER_UNLOCK (self);

  event = gst_event_new_eos ();
  gst_pad_send_event (pad, event);

  kms_loop_idle_add_full (self->priv->loop, G_PRIORITY_DEFAULT,
      (GSourceFunc) remove_elements_from_pipeline,
      KMS_COMPOSITE_MIXER_REF (port_data),
      (GDestroyNotify) kms_ref_struct_unref);

  return GST_PAD_PROBE_OK;
}

static void
kms_composite_mixer_port_data_destroy (gpointer data)
{
  KmsCompositeMixerData *port_data = (KmsCompositeMixerData *) data;
  KmsCompositeMixer *self = port_data->mixer;
  GstPad *audiosink;
  gchar *padname;

  KMS_COMPOSITE_MIXER_LOCK (self);

  port_data->removing = TRUE;

  kms_base_hub_unlink_video_sink (KMS_BASE_HUB (self), port_data->id);
  kms_base_hub_unlink_audio_sink (KMS_BASE_HUB (self), port_data->id);

  if (port_data->input) {
    GstEvent *event;
    gboolean result;
    GstPad *pad;

    if (port_data->videorate == NULL) {
      KMS_COMPOSITE_MIXER_UNLOCK (self);
      return;
    }

    pad = gst_element_get_static_pad (port_data->videorate, "sink");

    if (pad == NULL) {
      KMS_COMPOSITE_MIXER_UNLOCK (self);
      return;
    }

    if (!GST_OBJECT_FLAG_IS_SET (pad, GST_PAD_FLAG_EOS)) {

      if (GST_PAD_IS_FLUSHING (pad)) {
        gst_pad_send_event (pad, gst_event_new_flush_stop (FALSE));
      }

      event = gst_event_new_eos ();
      result = gst_pad_send_event (pad, event);

      if (port_data->input && self->priv->n_elems > 0) {
        port_data->input = FALSE;
        self->priv->n_elems--;
        kms_composite_mixer_recalculate_sizes (self);
      }
      KMS_COMPOSITE_MIXER_UNLOCK (self);

      if (!result) {
        GST_WARNING ("EOS event did not send");
      }
    } else {
      gboolean remove = FALSE;

      /* EOS callback was triggered before we could remove the port data */
      /* so we have to remove elements to avoid memory leaks. */
      remove = port_data->eos_managed;
      KMS_COMPOSITE_MIXER_UNLOCK (self);

      if (remove) {
        /* Remove pipeline without helding the mutex */
        kms_loop_idle_add_full (self->priv->loop, G_PRIORITY_DEFAULT,
            (GSourceFunc) remove_elements_from_pipeline,
            KMS_COMPOSITE_MIXER_REF (port_data),
            (GDestroyNotify) kms_ref_struct_unref);
      }
    }
    gst_element_unlink (port_data->input_capsfilter, port_data->videoconvert);
    gst_element_unlink (port_data->videoconvert, port_data->videorate);
    g_object_unref (pad);
  } else {
    if (port_data->probe_id > 0) {
      gst_pad_remove_probe (port_data->video_mixer_pad, port_data->probe_id);
    }

    if (port_data->link_probe_id > 0) {
      gst_pad_remove_probe (port_data->videoconvert_sink_pad,
          port_data->link_probe_id);
    }
    KMS_COMPOSITE_MIXER_UNLOCK (self);

    gst_element_unlink (port_data->input_capsfilter, port_data->videoconvert);

    gst_bin_remove (GST_BIN (self), g_object_ref (port_data->input_capsfilter));
    gst_element_set_state (port_data->input_capsfilter, GST_STATE_NULL);
    g_object_unref (port_data->input_capsfilter);
    port_data->input_capsfilter = NULL;

    gst_bin_remove (GST_BIN (self), g_object_ref (port_data->videoconvert));
    gst_element_set_state (port_data->videoconvert, GST_STATE_NULL);
    g_object_unref (port_data->videoconvert);
    port_data->videoconvert = NULL;
  }

  padname = g_strdup_printf (AUDIO_SINK_PAD, port_data->id);
  audiosink = gst_element_get_static_pad (self->priv->audiomixer, padname);
  gst_element_release_request_pad (self->priv->audiomixer, audiosink);
  gst_object_unref (audiosink);
  g_free (padname);
}

static GstPadProbeReturn
link_to_videomixer (GstPad * pad, GstPadProbeInfo * info,
    KmsCompositeMixerData * data)
{
  GstPadTemplate *sink_pad_template;
  KmsCompositeMixer *mixer;

  if (GST_EVENT_TYPE (GST_PAD_PROBE_INFO_EVENT (info)) !=
      GST_EVENT_STREAM_START) {
    return GST_PAD_PROBE_PASS;
  }

  mixer = KMS_COMPOSITE_MIXER (data->mixer);
  GST_DEBUG ("stream start detected %d", data->id);
  KMS_COMPOSITE_MIXER_LOCK (mixer);

  data->link_probe_id = 0;

  sink_pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (mixer->priv->
          videomixer), "sink_%u");

  if (G_UNLIKELY (sink_pad_template == NULL)) {
    GST_ERROR_OBJECT (mixer, "Error taking a new pad from videomixer");
    KMS_COMPOSITE_MIXER_UNLOCK (mixer);
    return GST_PAD_PROBE_DROP;
  }

  if (mixer->priv->videotestsrc == NULL) {
    GstElement *capsfilter;
    GstCaps *filtercaps;
    GstPad *pad;

    mixer->priv->videotestsrc = gst_element_factory_make ("videotestsrc", NULL);
    capsfilter = gst_element_factory_make ("capsfilter", NULL);

    g_object_set (mixer->priv->videotestsrc, "is-live", TRUE, "pattern",
        /*black */ 2, NULL);

    filtercaps =
        gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, "AYUV",
        "width", G_TYPE_INT, mixer->priv->output_width,
        "height", G_TYPE_INT, mixer->priv->output_height,
        "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
    g_object_set (G_OBJECT (capsfilter), "caps", filtercaps, NULL);
    gst_caps_unref (filtercaps);

    gst_bin_add_many (GST_BIN (mixer), mixer->priv->videotestsrc,
        capsfilter, NULL);

    gst_element_link (mixer->priv->videotestsrc, capsfilter);

    /*link capsfilter -> videomixer */
    pad = gst_element_request_pad (mixer->priv->videomixer, sink_pad_template,
        NULL, NULL);

    gst_element_link_pads (capsfilter, NULL,
        mixer->priv->videomixer, GST_OBJECT_NAME (pad));
    g_object_set (pad, "xpos", 0, "ypos", 0, "alpha", 0.0, NULL);
    g_object_unref (pad);

    gst_element_sync_state_with_parent (capsfilter);
    gst_element_sync_state_with_parent (mixer->priv->videotestsrc);
  }

  data->videoscale = gst_element_factory_make ("videoscale", NULL);
  data->capsfilter = gst_element_factory_make ("capsfilter", NULL);
  data->videorate = gst_element_factory_make ("videorate", NULL);
  data->queue = gst_element_factory_make ("queue", NULL);
  data->input = TRUE;

  gst_bin_add_many (GST_BIN (mixer), data->queue, data->videorate,
      data->videoscale, data->capsfilter, NULL);

  g_object_set (data->videorate, "average-period", 200 * GST_MSECOND, NULL);
  g_object_set (data->queue, "flush-on-eos", TRUE, "max-size-buffers", 60,
      NULL);

  gst_element_link_many (data->videorate, data->queue, data->videoscale,
      data->capsfilter, NULL);

  /*link capsfilter -> videomixer */
  data->video_mixer_pad =
      gst_element_request_pad (mixer->priv->videomixer,
      sink_pad_template, NULL, NULL);
  gst_element_link_pads (data->capsfilter, NULL,
      mixer->priv->videomixer, GST_OBJECT_NAME (data->video_mixer_pad));

  gst_element_link (data->videoconvert, data->videorate);

  data->probe_id = gst_pad_add_probe (data->video_mixer_pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      (GstPadProbeCallback) cb_EOS_received,
      KMS_COMPOSITE_MIXER_REF (data), (GDestroyNotify) kms_ref_struct_unref);

  gst_element_sync_state_with_parent (data->videoscale);
  gst_element_sync_state_with_parent (data->capsfilter);
  gst_element_sync_state_with_parent (data->videorate);
  gst_element_sync_state_with_parent (data->queue);

  /*recalculate the output sizes */
  mixer->priv->n_elems++;
  kms_composite_mixer_recalculate_sizes (mixer);

  KMS_COMPOSITE_MIXER_UNLOCK (mixer);

  return GST_PAD_PROBE_REMOVE;
}

static void
release_gint (gpointer data)
{
  g_slice_free (gint, data);
}

static gint *
create_gint (gint value)
{
  gint *p = g_slice_new (gint);

  *p = value;
  return p;
}

static void
kms_composite_mixer_unhandle_port (KmsBaseHub * mixer, gint id)
{
  KmsCompositeMixer *self = KMS_COMPOSITE_MIXER (mixer);

  GST_DEBUG ("unhandle id %d", id);

  KMS_COMPOSITE_MIXER_LOCK (self);

  g_hash_table_remove (self->priv->ports, &id);

  KMS_COMPOSITE_MIXER_UNLOCK (self);

  KMS_BASE_HUB_CLASS (G_OBJECT_CLASS
      (kms_composite_mixer_parent_class))->unhandle_port (mixer, id);
}

static KmsCompositeMixerData *
kms_composite_mixer_port_data_create (KmsCompositeMixer * mixer, gint id)
{
  KmsCompositeMixerData *data;
  gchar *padname;
  GstCaps *filtercaps;

  data = kms_create_composite_mixer_data ();
  data->mixer = mixer;
  data->id = id;
  data->input = FALSE;
  data->removing = FALSE;
  data->eos_managed = FALSE;

  data->videoconvert = gst_element_factory_make ("videoconvert", NULL);
  data->input_capsfilter = gst_element_factory_make ("capsfilter", NULL);

  filtercaps =
      gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, "AYUV",
      "width", G_TYPE_INT, mixer->priv->output_width,
      "height", G_TYPE_INT, mixer->priv->output_height,
      "framerate", GST_TYPE_FRACTION, 15, 1, NULL);
  g_object_set (G_OBJECT (data->input_capsfilter), "caps", filtercaps, NULL);
  gst_caps_unref (filtercaps);

  gst_bin_add_many (GST_BIN (mixer), data->input_capsfilter, data->videoconvert,
      NULL);

  gst_element_sync_state_with_parent (data->videoconvert);
  gst_element_sync_state_with_parent (data->input_capsfilter);

  /*link basemixer -> video_agnostic */
  kms_base_hub_link_video_sink (KMS_BASE_HUB (mixer), data->id,
      data->input_capsfilter, "sink", FALSE);

  gst_element_link (data->input_capsfilter, data->videoconvert);

  padname = g_strdup_printf (AUDIO_SINK_PAD, id);
  kms_base_hub_link_audio_sink (KMS_BASE_HUB (mixer), id,
      mixer->priv->audiomixer, padname, FALSE);
  g_free (padname);

  data->videoconvert_sink_pad = gst_element_get_static_pad (data->videoconvert,
      "sink");

  data->link_probe_id = gst_pad_add_probe (data->videoconvert_sink_pad,
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_BLOCK,
      (GstPadProbeCallback) link_to_videomixer,
      KMS_COMPOSITE_MIXER_REF (data), (GDestroyNotify) kms_ref_struct_unref);

  return data;
}

static gint
get_stream_id_from_padname (const gchar * name)
{
  gint64 id;

  if (name == NULL)
    return -1;

  if (!g_str_has_prefix (name, AUDIO_SRC_PAD_PREFIX))
    return -1;

  id = g_ascii_strtoll (name + LENGTH_AUDIO_SRC_PAD_PREFIX, NULL, 10);
  if (id > G_MAXINT)
    return -1;

  return id;
}

static void
pad_added_cb (GstElement * element, GstPad * pad, gpointer data)
{
  gint id;
  KmsCompositeMixer *self = KMS_COMPOSITE_MIXER (data);

  if (gst_pad_get_direction (pad) != GST_PAD_SRC)
    return;

  id = get_stream_id_from_padname (GST_OBJECT_NAME (pad));

  if (id < 0) {
    GST_ERROR_OBJECT (self, "Invalid HubPort for %" GST_PTR_FORMAT, pad);
    return;
  }

  kms_base_hub_link_audio_src (KMS_BASE_HUB (self), id,
      self->priv->audiomixer, GST_OBJECT_NAME (pad), TRUE);
}

static void
pad_removed_cb (GstElement * element, GstPad * pad, gpointer data)
{
  GST_DEBUG ("Removed pad %" GST_PTR_FORMAT, pad);
}

static gint
kms_composite_mixer_handle_port (KmsBaseHub * mixer,
    GstElement * mixer_end_point)
{
  KmsCompositeMixer *self = KMS_COMPOSITE_MIXER (mixer);
  KmsCompositeMixerData *port_data;
  gint port_id;

  port_id = KMS_BASE_HUB_CLASS (G_OBJECT_CLASS
      (kms_composite_mixer_parent_class))->handle_port (mixer, mixer_end_point);

  if (port_id < 0) {
    return port_id;
  }

  KMS_COMPOSITE_MIXER_LOCK (self);

  if (self->priv->videomixer == NULL) {
    GstElement *videorate_mixer;

    videorate_mixer = gst_element_factory_make ("videorate", NULL);
    self->priv->videomixer = gst_element_factory_make ("compositor", NULL);
    g_object_set (G_OBJECT (self->priv->videomixer), "background", 1, NULL);
    self->priv->mixer_video_agnostic =
        gst_element_factory_make ("agnosticbin", NULL);

    gst_bin_add_many (GST_BIN (mixer), self->priv->videomixer, videorate_mixer,
        self->priv->mixer_video_agnostic, NULL);

    gst_element_sync_state_with_parent (self->priv->videomixer);
    gst_element_sync_state_with_parent (videorate_mixer);
    gst_element_sync_state_with_parent (self->priv->mixer_video_agnostic);

    gst_element_link_many (self->priv->videomixer, videorate_mixer,
        self->priv->mixer_video_agnostic, NULL);
  }

  if (self->priv->audiomixer == NULL) {
    self->priv->audiomixer = gst_element_factory_make ("kmsaudiomixer", NULL);

    gst_bin_add (GST_BIN (mixer), self->priv->audiomixer);

    gst_element_sync_state_with_parent (self->priv->audiomixer);
    g_signal_connect (self->priv->audiomixer, "pad-added",
        G_CALLBACK (pad_added_cb), self);
    g_signal_connect (self->priv->audiomixer, "pad-removed",
        G_CALLBACK (pad_removed_cb), self);
  }
  kms_base_hub_link_video_src (KMS_BASE_HUB (self), port_id,
      self->priv->mixer_video_agnostic, "src_%u", TRUE);

  port_data = kms_composite_mixer_port_data_create (self, port_id);
  g_hash_table_insert (self->priv->ports, create_gint (port_id), port_data);

  KMS_COMPOSITE_MIXER_UNLOCK (self);

  return port_id;
}

static void
kms_composite_mixer_dispose (GObject * object)
{
  KmsCompositeMixer *self = KMS_COMPOSITE_MIXER (object);

  KMS_COMPOSITE_MIXER_LOCK (self);
  g_hash_table_remove_all (self->priv->ports);
  KMS_COMPOSITE_MIXER_UNLOCK (self);
  g_clear_object (&self->priv->loop);

  G_OBJECT_CLASS (kms_composite_mixer_parent_class)->dispose (object);
}

static void
kms_composite_mixer_finalize (GObject * object)
{
  KmsCompositeMixer *self = KMS_COMPOSITE_MIXER (object);

  g_rec_mutex_clear (&self->priv->mutex);

  if (self->priv->ports != NULL) {
    g_hash_table_unref (self->priv->ports);
    self->priv->ports = NULL;
  }

  G_OBJECT_CLASS (kms_composite_mixer_parent_class)->finalize (object);
}

static void
kms_composite_mixer_class_init (KmsCompositeMixerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  KmsBaseHubClass *base_hub_class = KMS_BASE_HUB_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "CompositeMixer", "Generic", "Mixer element that composes n input flows"
      " in one output flow", "David Fernandez <d.fernandezlop@gmail.com>");

  gobject_class->dispose = GST_DEBUG_FUNCPTR (kms_composite_mixer_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (kms_composite_mixer_finalize);

  base_hub_class->handle_port =
      GST_DEBUG_FUNCPTR (kms_composite_mixer_handle_port);
  base_hub_class->unhandle_port =
      GST_DEBUG_FUNCPTR (kms_composite_mixer_unhandle_port);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_sink_factory));

  /* Registers a private structure for the instantiatable type */
  g_type_class_add_private (klass, sizeof (KmsCompositeMixerPrivate));
}

static void
kms_composite_mixer_init (KmsCompositeMixer * self)
{
  self->priv = KMS_COMPOSITE_MIXER_GET_PRIVATE (self);

  g_rec_mutex_init (&self->priv->mutex);

  self->priv->ports = g_hash_table_new_full (g_int_hash, g_int_equal,
      release_gint, kms_composite_mixer_port_data_destroy);
  //TODO:Obtain the dimensions of the bigger input stream
  self->priv->output_height = 600;
  self->priv->output_width = 800;
  self->priv->n_elems = 0;

  self->priv->loop = kms_loop_new ();
}

gboolean
kms_composite_mixer_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, PLUGIN_NAME, GST_RANK_NONE,
      KMS_TYPE_COMPOSITE_MIXER);
}
