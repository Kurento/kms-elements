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

#ifndef __KMS_ICE_BASE_AGENT_H__
#define __KMS_ICE_BASE_AGENT_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define KMS_TYPE_ICE_BASE_AGENT \
  (kms_ice_base_agent_get_type())
#define KMS_ICE_BASE_AGENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),KMS_TYPE_ICE_BASE_AGENT,KmsIceBaseAgent))
#define KMS_ICE_BASE_AGENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),KMS_TYPE_ICE_BASE_AGENT,KmsIceBaseAgentClass))
#define KMS_IS_ICE_BASE_AGENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),KMS_TYPE_ICE_BASE_AGENT))
#define KMS_IS_ICE_BASE_AGENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),KMS_TYPE_ICE_BASE_AGENT))
#define KMS_ICE_BASE_AGENT_CAST(obj) ((KmsIceBaseAgent*)(obj))

#define KMS_ICE_BASE_AGENT_LOCK(conn) \
  (g_rec_mutex_lock (&KMS_ICE_BASE_AGENT_CAST ((conn))->mutex))
#define KMS_ICE_BASE_AGENT_UNLOCK(conn) \
  (g_rec_mutex_unlock (&KMS_ICE_BASE_AGENT_CAST ((conn))->mutex))

typedef enum TurnProtocol
{
  TURN_PROTOCOL_UDP,
  TURN_PROTOCOL_TCP,
  TURN_PROTOCOL_SSLTCP,
  TURN_PROTOCOL_TLS
} TurnProtocol;

typedef enum IceState
{
  ICE_STATE_READY,
  ICE_STATE_GATHERING,
  ICE_STATE_CONNECTING,
  ICE_STATE_CONNECTED,
  ICE_STATE_FAILED,
  ICE_STATE_DISCONNECTED
} IceState;

typedef struct _KmsIceRelayServerInfo KmsIceRelayServerInfo;
typedef struct _KmsIceCandidate KmsIceCandidate;

struct _KmsIceRelayServerInfo
{
  const gchar *stream_id;
  const gchar *server_ip;
  guint server_port;
  const gchar *username;
  const gchar *password;
  TurnProtocol type;
};

typedef struct _KmsIceBaseAgent KmsIceBaseAgent;
typedef struct _KmsIceBaseAgentClass KmsIceBaseAgentClass;

struct _KmsIceBaseAgent
{
  GObject parent;
};

struct _KmsIceBaseAgentClass
{
  GObjectClass parent_class;

  /* signals */

  void (*on_ice_candidate) (KmsIceBaseAgent * self,
                            KmsIceCandidate * candidate);
  void (*on_ice_gathering_done) (KmsIceBaseAgent* self,
                                 const gchar* stream_id);

  /* virtual methods */
  char* (*add_stream) (KmsIceBaseAgent * self,
                      const char *stream_id, guint16 min_port, guint16 max_port);

  void (*remove_stream) (KmsIceBaseAgent * self,
                      const char *stream_id);

  gboolean (*set_remote_credentials) (KmsIceBaseAgent * self,
                                  const char *stream_id,
                                  const char *ufrag,
                                  const char *pwd);

  void (*get_local_credentials) (KmsIceBaseAgent * self,
                                 const char *stream_id,
                                 gchar **ufrag,
                                 gchar **pwd);

  void (*set_remote_description) (KmsIceBaseAgent * self,
                                   const char *remote_description);

  void (*set_local_description) (KmsIceBaseAgent * self,
                                  const char *local_description);

  void (*add_relay_server) (KmsIceBaseAgent * self, KmsIceRelayServerInfo server_info);

  gboolean (*start_gathering_candidates) (KmsIceBaseAgent * self,
                                          const char* stream_id);

  gboolean (*add_ice_candidate) (KmsIceBaseAgent * self,
                                 KmsIceCandidate *candidate,
                                 const char* stream_id);

  KmsIceCandidate* (*get_default_local_candidate) (KmsIceBaseAgent * self,
                                                   const char* stream_id,
                                                   guint component_id);

  void (*run_agent) (KmsIceBaseAgent * self);
};

const gchar* kms_ice_base_agent_state_to_string (IceState state);

char* kms_ice_base_agent_add_stream (KmsIceBaseAgent * self,
                                     const char *stream_id,
                                     guint16 min_port, guint16 max_port);

void kms_ice_base_agent_remove_stream (KmsIceBaseAgent * self,
                                       const char *stream_id);

gboolean kms_ice_base_agent_set_remote_credentials (KmsIceBaseAgent * self,
                                                    const char *stream_id,
                                                    const char *ufrag,
                                                    const char *pwd);

void kms_ice_base_agent_get_local_credentials (KmsIceBaseAgent * self,
                                              const char *stream_id,
                                              gchar **ufrag,
                                              gchar **pwd);

void kms_ice_base_agent_set_remote_description (KmsIceBaseAgent * self,
                                               const char *remote_description);

void kms_ice_base_agent_set_local_description (KmsIceBaseAgent * self,
                                              const char *local_description);

void kms_ice_base_agent_add_relay_server (KmsIceBaseAgent * self,
                                          KmsIceRelayServerInfo server_info);

gboolean kms_ice_base_agent_start_gathering_candidates (KmsIceBaseAgent * self,
                                                        const char* stream_id);

gboolean kms_ice_base_agent_add_ice_candidate (KmsIceBaseAgent * self,
                                               KmsIceCandidate *candidate,
                                               const char* stream_id);

KmsIceCandidate* kms_ice_base_agent_get_default_local_candidate (KmsIceBaseAgent * self,
                                                                const char* stream_id,
                                                                guint component_id);

void kms_ice_base_agent_run_agent (KmsIceBaseAgent * self);

GType kms_ice_base_agent_get_type (void);

G_END_DECLS
#endif /* __KMS_ICE_BASE_AGENT_H__ */

