/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
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
#ifndef _KMS_SELECTABLE_MIXER_H_
#define _KMS_SELECTABLE_MIXER_H_

#include <commons/kmsbasehub.h>

#define AUDIO_SINK_PAD_PREFIX  "sink_"
#define AUDIO_SRC_PAD_PREFIX  "src_"
#define LENGTH_AUDIO_SINK_PAD_PREFIX (sizeof(AUDIO_SINK_PAD_PREFIX) - 1)
#define LENGTH_AUDIO_SRC_PAD_PREFIX (sizeof(AUDIO_SRC_PAD_PREFIX) - 1)
#define AUDIO_SINK_PAD AUDIO_SINK_PAD_PREFIX "%u"
#define AUDIO_SRC_PAD AUDIO_SRC_PAD_PREFIX "%u"

G_BEGIN_DECLS
#define KMS_TYPE_SELECTABLE_MIXER kms_selectable_mixer_get_type()
#define KMS_SELECTABLE_MIXER(obj) ( \
  G_TYPE_CHECK_INSTANCE_CAST(       \
    (obj),                          \
    KMS_TYPE_SELECTABLE_MIXER,      \
    KmsSelectableMixer              \
  )                                 \
)
#define KMS_SELECTABLE_MIXER_CLASS(klass) ( \
  G_TYPE_CHECK_CLASS_CAST (                 \
    (klass),                                \
    KMS_TYPE_SELECTABLE_MIXER,              \
    KmsSelectableMixerClass                 \
  )                                         \
)
#define KMS_IS_SELECTABLE_MIXER(obj) ( \
  G_TYPE_CHECK_INSTANCE_TYPE (         \
    (obj),                             \
    KMS_TYPE_SELECTABLE_MIXER          \
  )                                    \
)
#define KMS_IS_SELECTABLE_MIXER_CLASS(klass) ( \
  G_TYPE_CHECK_CLASS_TYPE((klass),             \
  KMS_TYPE_SELECTABLE_MIXER)                   \
)

typedef struct _KmsSelectableMixer KmsSelectableMixer;
typedef struct _KmsSelectableMixerClass KmsSelectableMixerClass;
typedef struct _KmsSelectableMixerPrivate KmsSelectableMixerPrivate;

struct _KmsSelectableMixer
{
  KmsBaseHub parent;

  /*< private > */
  KmsSelectableMixerPrivate *priv;
};

struct _KmsSelectableMixerClass
{
  KmsBaseHubClass parent_class;

  /* Actions */
  gboolean (*connect_video) (KmsSelectableMixer * self, guint source, guint sink);
  gboolean (*connect_audio) (KmsSelectableMixer * self, guint source, guint sink);
  gboolean (*disconnect_audio) (KmsSelectableMixer * self, guint source, guint sink);
};

GType kms_selectable_mixer_get_type (void);

gboolean kms_selectable_mixer_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* _KMS_SELECTABLE_MIXER_H_ */
