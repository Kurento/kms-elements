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

#include "kmswebrtcbaseconnection.h"
#include <commons/kmsstats.h>
#include "kmsiceniceagent.h"

#include <string.h> // strlen()

// Network interfaces and IP fetching for NiceAgent
#include <ifaddrs.h>
#include <net/if.h>

#define GST_CAT_DEFAULT kmswebrtcbaseconnection
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "kmswebrtcbaseconnection"

G_DEFINE_TYPE (KmsWebRtcBaseConnection, kms_webrtc_base_connection,
    G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_ICE_AGENT,
  PROP_STREAM_ID
};

gboolean
kms_webrtc_base_connection_configure (KmsWebRtcBaseConnection * self,
    KmsIceBaseAgent * agent, const gchar * name)
{
  self->agent = g_object_ref (agent);
  self->name = g_strdup (name);

  self->stream_id =
      kms_ice_base_agent_add_stream (agent, self->name, self->min_port,
      self->max_port);

  if (self->stream_id == NULL) {
    GST_ERROR_OBJECT (self, "Cannot add stream for %s.", self->name);
    return FALSE;
  }

  return TRUE;
}

static gchar *kms_webrtc_base_connection_get_certificate_pem_default
    (KmsWebRtcBaseConnection * self)
{
  KmsWebRtcBaseConnectionClass *klass =
      KMS_WEBRTC_BASE_CONNECTION_CLASS (G_OBJECT_GET_CLASS (self));

  if (klass->get_certificate_pem ==
      kms_webrtc_base_connection_get_certificate_pem_default) {
    GST_WARNING_OBJECT (self,
        "%s does not reimplement 'get_certificate_pem'",
        G_OBJECT_CLASS_NAME (klass));
  }

  return NULL;
}

static void
kms_webrtc_base_connection_finalize (GObject * object)
{
  KmsWebRtcBaseConnection *self = KMS_WEBRTC_BASE_CONNECTION (object);

  GST_DEBUG_OBJECT (self, "finalize");

  kms_ice_base_agent_remove_stream (self->agent, self->stream_id);
  g_free (self->name);
  g_free (self->stream_id);
  g_clear_object (&self->agent);
  g_rec_mutex_clear (&self->mutex);

  /* chain up */
  G_OBJECT_CLASS (kms_webrtc_base_connection_parent_class)->finalize (object);
}

static void
kms_webrtc_base_connection_init (KmsWebRtcBaseConnection * self)
{
  g_rec_mutex_init (&self->mutex);
  self->stats_enabled = FALSE;
}

static void
kms_webrtc_base_connection_set_latency_callback_default (KmsIRtpConnection *
    obj, BufferLatencyCallback cb, gpointer user_data)
{
  KmsWebRtcBaseConnection *self = KMS_WEBRTC_BASE_CONNECTION (obj);

  self->cb = cb;
  self->user_data = user_data;
}

static void
kms_webrtc_base_connection_collect_latency_stats_default (KmsIRtpConnection *
    obj, gboolean enable)
{
  KmsWebRtcBaseConnection *self = KMS_WEBRTC_BASE_CONNECTION (obj);

  self->stats_enabled = enable;
}

static void
kms_webrtc_base_connection_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  KmsWebRtcBaseConnection *self = KMS_WEBRTC_BASE_CONNECTION (object);

  KMS_WEBRTC_BASE_CONNECTION_LOCK (self);

  switch (prop_id) {
    case PROP_ICE_AGENT:
      g_value_set_object (value, self->agent);
      break;
    case PROP_STREAM_ID:
      g_value_set_string (value, self->stream_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  KMS_WEBRTC_BASE_CONNECTION_UNLOCK (self);
}

static void
kms_webrtc_base_connection_class_init (KmsWebRtcBaseConnectionClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = kms_webrtc_base_connection_finalize;
  gobject_class->get_property = kms_webrtc_base_connection_get_property;

  klass->get_certificate_pem =
      kms_webrtc_base_connection_get_certificate_pem_default;

  klass->set_latency_callback =
      kms_webrtc_base_connection_set_latency_callback_default;
  klass->collect_latency_stats =
      kms_webrtc_base_connection_collect_latency_stats_default;

  g_object_class_install_property (gobject_class, PROP_ICE_AGENT,
      g_param_spec_object ("ice-agent", "Ice agent",
          "The Ice agent.", KMS_TYPE_ICE_BASE_AGENT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STREAM_ID,
      g_param_spec_string ("stream-id", "Stream identifier",
          "The stream identifier.", NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);
}

gchar *
kms_webrtc_base_connection_get_certificate_pem (KmsWebRtcBaseConnection * self)
{
  KmsWebRtcBaseConnectionClass *klass =
      KMS_WEBRTC_BASE_CONNECTION_CLASS (G_OBJECT_GET_CLASS (self));

  return klass->get_certificate_pem (self);
}

/**
 * Split comma-separated string.
 */
static GSList *
kms_webrtc_base_connection_split_comma (const gchar * str)
{
  if (str == NULL) {
    return NULL;
  }

  // str == "A, B,C"

  gchar **arr = g_strsplit_set (str, " ,", -1);

  // arr[0] == "A"
  // arr[1] == ""
  // arr[2] == "B"
  // arr[3] == "C"
  // arr[4] == NULL

  GSList *list = NULL;

  for (int i = 0; arr[i] != NULL; ++i) {
    if (strlen (arr[i]) == 0) {
      // arr[i] == ""
      g_free (arr[i]);
      continue;
    }

    list = g_slist_append (list, arr[i]);
  }

  g_free (arr);

  return list;
}

gint
kms_webrtc_base_connection_cmp_ifa (const gchar * n1, const gchar * n2) {
  return strcmp(n1, n2);
}

gboolean
kms_webrtc_base_connection_agent_is_network_interface_valid (struct ifaddrs * ifa) {
  gboolean is_valid = FALSE;
  int sa_family;

  // No IP address assigned to interface, skip
  if (ifa->ifa_addr == NULL) {
    goto end;
  }

  // Interface is either down of not running
  if (!(ifa->ifa_flags && IFF_UP) || !(ifa->ifa_flags && IFF_RUNNING)) {
    goto end;
  }

  sa_family = ifa->ifa_addr->sa_family;

  // Only traverse through interfaces which are from the AF_INET/AF_INET6 families
  if (sa_family != AF_INET && sa_family != AF_INET6) {
    goto end;
  }

  is_valid = TRUE;

end:
  return is_valid;
};

gboolean
kms_webrtc_base_connection_agent_is_interface_ip_valid (const gchar * ip_address,
    GSList * ip_ignore_list) {
  gboolean is_valid = FALSE;

  // Link local IPv4, ignore
  if (!strncmp(ip_address, "169.254.", 8)) {
    goto end;
  }

  // Link local IPv6, ignore
  if (!strncmp(ip_address, "fe80:", 5)) {
    goto end;
  }

  // Check if there's an IP ignore list defined and see if the IP address matches
  // one of them
  if (ip_ignore_list != NULL) {
    if (g_slist_find_custom (ip_ignore_list, ip_address,
        (GCompareFunc) kms_webrtc_base_connection_cmp_ifa)) {
      goto end;
    }
  }

  is_valid = TRUE;

end:
  return is_valid;
};

/**
 * Add new local IP address to NiceAgent instance.
 */
  static void
kms_webrtc_base_connection_agent_add_net_ifs_addrs (NiceAgent * agent,
    GSList * net_list, GSList * ip_ignore_list)
{
  struct ifaddrs *ifaddr, *ifa;
  gchar ip_address[INET6_ADDRSTRLEN];
  GSList *it;
  NiceAddress *nice_address;

  if (getifaddrs(&ifaddr) == -1) {
    GST_ERROR ("Failed to fetch system network interfaces");
    return;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (!kms_webrtc_base_connection_agent_is_network_interface_valid(ifa)) {
      continue;
    }

    // See if the network interface is in the configuration list
    it = g_slist_find_custom (net_list, ifa->ifa_name,
        (GCompareFunc) kms_webrtc_base_connection_cmp_ifa);

    // Current interface is not present in config, skip.
    if (it == NULL) {
      continue;
    }

    if (ifa->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in *in4 = (struct sockaddr_in*) ifa->ifa_addr;
      inet_ntop(AF_INET, &in4->sin_addr, ip_address, sizeof (ip_address));
    } else {
      struct sockaddr_in6 *in6 = (struct sockaddr_in6*) ifa->ifa_addr;
      inet_ntop(AF_INET6, &in6->sin6_addr, ip_address, sizeof (ip_address));
    }

    // Check if the IP in the ignore list or is link local
    if (!kms_webrtc_base_connection_agent_is_interface_ip_valid(ip_address,
          ip_ignore_list)) {
      continue;
    }

    nice_address = nice_address_new ();
    nice_address_set_from_string (nice_address, ip_address);
    nice_agent_add_local_address (agent, nice_address);
    nice_address_free (nice_address);

    GST_DEBUG_OBJECT (agent, "Added interface %s's IP address: %s",
        ifa->ifa_name, ip_address);
  }

  freeifaddrs(ifaddr);
}

void
kms_webrtc_base_connection_set_network_ifs_info (KmsWebRtcBaseConnection *
    self, const gchar * net_names, const gchar * ip_ignore_list)
{
  if (KMS_IS_ICE_NICE_AGENT (self->agent)) {
    KmsIceNiceAgent *nice_agent = KMS_ICE_NICE_AGENT (self->agent);
    NiceAgent *agent = kms_ice_nice_agent_get_agent (nice_agent);

    GSList *net_list = kms_webrtc_base_connection_split_comma (net_names);
    GSList *ip_ignore_glist = kms_webrtc_base_connection_split_comma (ip_ignore_list);
    kms_webrtc_base_connection_agent_add_net_ifs_addrs (agent, net_list, ip_ignore_glist);

    g_slist_free_full (net_list, g_free);
    g_slist_free_full (ip_ignore_glist, g_free);
  }
}

void
kms_webrtc_base_connection_set_ice_tcp (KmsWebRtcBaseConnection *self,
    gboolean ice_tcp)
{
  if (KMS_IS_ICE_NICE_AGENT (self->agent)) {
    KmsIceNiceAgent *nice_agent = KMS_ICE_NICE_AGENT (self->agent);
    g_object_set (
        kms_ice_nice_agent_get_agent (nice_agent), "ice-tcp", ice_tcp, NULL);
  }
}

void
kms_webrtc_base_connection_set_stun_server_info (KmsWebRtcBaseConnection * self,
    const gchar * ip, guint port)
{
  // TODO: This code should be independent of the type of ice agent
  if (KMS_IS_ICE_NICE_AGENT (self->agent)) {
    KmsIceNiceAgent *nice_agent = KMS_ICE_NICE_AGENT (self->agent);

    g_object_set (kms_ice_nice_agent_get_agent (nice_agent),
        "stun-server", ip, "stun-server-port", port, NULL);
  }
}

void
kms_webrtc_base_connection_set_relay_info (KmsWebRtcBaseConnection * self,
    const gchar * server_ip,
    guint server_port,
    const gchar * username, const gchar * password, TurnProtocol type)
{
  KmsIceRelayServerInfo info;

  info.server_ip = server_ip;
  info.server_port = server_port;
  info.username = username;
  info.password = password;
  info.type = type;
  info.stream_id = self->stream_id;

  kms_ice_base_agent_add_relay_server (self->agent, info);
}

void
kms_webrtc_base_connection_set_latency_callback (KmsIRtpConnection * self,
    BufferLatencyCallback cb, gpointer user_data)
{
  KmsWebRtcBaseConnectionClass *klass =
      KMS_WEBRTC_BASE_CONNECTION_CLASS (G_OBJECT_GET_CLASS (self));

  klass->set_latency_callback (self, cb, user_data);
}

void
kms_webrtc_base_connection_collect_latency_stats (KmsIRtpConnection * self,
    gboolean enable)
{
  KmsWebRtcBaseConnectionClass *klass =
      KMS_WEBRTC_BASE_CONNECTION_CLASS (G_OBJECT_GET_CLASS (self));

  klass->collect_latency_stats (self, enable);
}
