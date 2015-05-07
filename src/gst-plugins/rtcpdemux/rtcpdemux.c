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
#include <config.h>
#include <gst/gst.h>

#include <kmsrtcpdemux.h>

static gboolean
rtcpdemuxr_init (GstPlugin * rtcpdemux)
{
  if (!kms_rtcp_demux_plugin_init (rtcpdemux))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    rtcpdemux,
    "RtcpDemuxer plugin",
    rtcpdemuxr_init, VERSION, "LGPL", "Kurento", "http://kurento.com/")
