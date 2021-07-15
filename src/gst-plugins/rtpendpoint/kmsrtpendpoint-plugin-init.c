/*
 * (C) Copyright 2013 Kurento (http://kurento.org/)
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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif


#include "kmsrtpendpoint.h"
#include "kmssiprtpendpoint.h"

#define RTP_PLUGIN_NAME "rtpendpoint"
#define SIP_RTP_PLUGIN_NAME "siprtpendpoint"

gboolean
kms_rtp_endpoint_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, RTP_PLUGIN_NAME, GST_RANK_NONE, KMS_TYPE_RTP_ENDPOINT) &&
		  gst_element_register (plugin, SIP_RTP_PLUGIN_NAME, GST_RANK_NONE, KMS_TYPE_SIP_RTP_ENDPOINT);
}


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    kmsrtpendpoint,
    "Kurento rtp endpoint",
    kms_rtp_endpoint_plugin_init, VERSION, GST_LICENSE_UNKNOWN,
    "Kurento Elements", "http://kurento.com/")
