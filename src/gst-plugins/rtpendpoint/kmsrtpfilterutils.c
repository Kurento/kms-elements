/*
 * (C) Copyright 2015 Kurento (http://kurento.org/)
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


#include "kmsrtpfilterutils.h"
#include <commons/constants.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>


static void
adjust_filter_info_ts_info (SipFilterSsrcInfo* filter_info, guint16 seq, guint32 ts)
{
	g_rec_mutex_lock (&filter_info->mutex);
	if (filter_info->last_seq < seq) {
		filter_info->last_ts_delta = (ts - filter_info->last_ts) / (seq - filter_info->last_seq);
		filter_info->last_seq = seq;
		filter_info->last_ts = ts;
	}
	g_rec_mutex_unlock (&filter_info->mutex);
}

static gboolean
check_ssrc (guint32 ssrc, SipFilterSsrcInfo* filter_info, guint16 seq, guint32 ts)
{
	GST_DEBUG("Check_ssrc %u, expected %u, current %u", ssrc, filter_info->expected, filter_info->current);
	if (filter_info->expected == 0) {
		gboolean result, init;

		init = FALSE;
		g_rec_mutex_lock (&filter_info->mutex);
		if (filter_info->expected == 0) {
			// Not yet received first SSRC
			init = TRUE;

			// If SSRC is in list of old ones, we discard buffer
			if (g_list_index(filter_info->old, GUINT_TO_POINTER(ssrc)) != -1) {
				result = TRUE;
			} else {
				// If not, first SSRc will be fixed for pipeline in current media connection
				filter_info->expected = ssrc;
				filter_info->current = ssrc;
				filter_info->last_seq = seq;
				filter_info->last_ts = ts;
				result = FALSE;
			}
		}
		g_rec_mutex_unlock  (&filter_info->mutex);
		if (init) {
			// If not init then a concurrent thread has just initted.
			return result;
		}
	}

	if (ssrc == filter_info->current) {
		adjust_filter_info_ts_info (filter_info, seq, ts);

		// SSRC is expected one we let buffer to continue processing
		return FALSE;
	} else  {
		// If SSRC is in list of old ones, we discard buffer
		g_rec_mutex_lock (&filter_info->mutex);
		if (g_list_index(filter_info->old, GUINT_TO_POINTER(ssrc)) != -1) {
			g_rec_mutex_unlock (&filter_info->mutex);
			return TRUE;
		}
		g_rec_mutex_unlock (&filter_info->mutex);


		// SSRC is not expected one, but also does not seem late packets from previous media connections
		// We can assume peer has just switched SSRC (VoIP PBX?), and buffer should be affected
		// In this case SSRC in buffer should be changed to expected one so that pipeline does not complain
		// and stop processing due to not linked error

		// We cannot let the buffer continue as it would pause the streaming task,
		// But this is fixed later, by now we let the buffer go
		return FALSE;
	}
}

static gboolean
check_ssrc_rtcp (guint32 ssrc, SipFilterSsrcInfo* filter_info)
{
	if (filter_info->expected == 0) {
		gboolean result, init;

		init = FALSE;
		g_rec_mutex_lock (&filter_info->mutex);
		if (filter_info->expected == 0) {
			// Not yet received first SSRC
			init = TRUE;

			// If SSRC is in list of old ones, we discard buffer
			if (g_list_index(filter_info->old, GUINT_TO_POINTER(ssrc)) != -1) {
				result = TRUE;
			} else {
				// If not, first SSRc will be fixed for pipeline in current media connection
				filter_info->expected = ssrc;
				filter_info->current = ssrc;
				result = FALSE;
			}
		}
		g_rec_mutex_unlock  (&filter_info->mutex);
		if (init) {
			// If not init then a concurrent thread has just initted.
			return result;
		}
	}

	if (ssrc == filter_info->current) {
		// SSRC is expected one we let buffer to continue processing
		return FALSE;
	} else  {
		return TRUE;
	}
}

static void
fix_rtp_buffer_voip_switched_ssrc (GstRTPBuffer *rtp_buffer, SipFilterSsrcInfo* filter_info)
{
	guint32 seq_number = gst_rtp_buffer_get_seq  (rtp_buffer);
	guint32 ts = gst_rtp_buffer_get_timestamp (rtp_buffer);
	guint32 fixed_ts;

	// Fix SSRC to keep pipelime happy
	gst_rtp_buffer_set_ssrc (rtp_buffer, filter_info->expected);

	// We fix timestamping to keep kmsrtpsynchronizer happy
	if (filter_info->jump_ts != 0) {
		gint64 aux_ts;

		aux_ts = ts;
		aux_ts -= filter_info->jump_ts;

		if (aux_ts < 0)
			aux_ts += G_MAXUINT32;

		fixed_ts = aux_ts;
		gst_rtp_buffer_set_timestamp (rtp_buffer, fixed_ts);
	}

	GST_DEBUG ("Fixing RTP info: ssrc %u, sequence %u and ts %u", filter_info->expected, seq_number, fixed_ts);
}

static gint64
calculate_jump_ts (SipFilterSsrcInfo* filter_info, guint32 seq, guint32 ts)
{
	gint64 new_jump;

	new_jump = filter_info->jump_ts;
	if (filter_info->last_ts < ts) {
		new_jump += (ts - filter_info->last_ts) - filter_info->last_ts_delta;
	} else {
		new_jump -= (filter_info->last_ts - ts) + filter_info->last_ts_delta;
	}

	return new_jump;
}

static GstPadProbeReturn
filter_ssrc_rtp_buffer (GstBuffer *buffer, SipFilterSsrcInfo* filter_info)
{
	GstRTPBuffer rtp_buffer =  GST_RTP_BUFFER_INIT;
	GstPadProbeReturn result = GST_PAD_PROBE_OK;

	if (gst_rtp_buffer_map (buffer, GST_MAP_READWRITE, &rtp_buffer)) {
		GST_DEBUG ("filter old ssrc RTP buffer");
		guint32 checked_ssrc = gst_rtp_buffer_get_ssrc (&rtp_buffer);
		guint32 seq_number = gst_rtp_buffer_get_seq  (&rtp_buffer);
		guint32 ts = gst_rtp_buffer_get_timestamp  (&rtp_buffer);

		GST_DEBUG("Filtering RTP buffer with ssrc %u and sequence %u, and ts %u", checked_ssrc, seq_number, ts);
		if (check_ssrc (checked_ssrc, filter_info, seq_number, ts)) {
			GST_INFO ("RTP packet dropped from a previous RTP flow with SSRC %u", checked_ssrc);
			gst_rtp_buffer_unmap (&rtp_buffer);
			return GST_PAD_PROBE_DROP;
		} else {
			// We are pushing an EXPECTED SSRC, so after its processing this probe is no longer needed
			GST_DEBUG ("filter old ssrc forwarded buffer %u", checked_ssrc);
			if (checked_ssrc != filter_info->expected) {
				gboolean ssrc_switched = FALSE;

				// SSRC not expected and not from last stream, stream switching is happening
				if (checked_ssrc != filter_info->current) {
					g_rec_mutex_lock (&filter_info->mutex);
					if (checked_ssrc != filter_info->current) {
						// Old stream must be filtered out from now on
						filter_info->old = g_list_append (filter_info->old, GUINT_TO_POINTER(filter_info->current));

						// Get sure next Buffers will be easily checked for new current stream
						filter_info->current = checked_ssrc;
						// Calculate ts jump so that we can adapt RTP buffers and SR for new stream to keep kmsrtpsynchronizer happy
						filter_info->jump_ts = calculate_jump_ts (filter_info, seq_number, ts);

						GST_DEBUG ("SSRC switched, calculated ts jump %ld, last ts %u, current ts %u, seq number: %u", filter_info->jump_ts, filter_info->last_ts, ts, seq_number);
						ssrc_switched = TRUE;
					}
					g_rec_mutex_unlock (&filter_info->mutex);
				}

				// We have just switched SSRCs some anomalous situation
				// Kind of hack: we will change SSRC in buffer to original one so that
				// pipeline does not get disrupted and media continue flowing to already connected elements
				if (filter_info->media_session == VIDEO_RTP_SESSION) {
					// if this is video, this is an unexpected media switching, to allow further media comm
					// We just correct SSRC and let buffer continue flow, otherwise streaming task would be stopped
					GST_DEBUG("Switching SSRC, original: %u, switched: %u", filter_info->expected, checked_ssrc);
					gst_rtp_buffer_set_ssrc (&rtp_buffer, filter_info->expected);
				} else if (filter_info->media_session == AUDIO_RTP_SESSION) {
					// If this is audio, it may be a VoIP situation of media switching.
					// IT is marked by marker and SSRC is switched and timestamping process is restarted to a random point
					if (ssrc_switched) {
						GST_INFO("VoIP RTP flow internally switched, old SSRC %u, new one %u", filter_info->expected, checked_ssrc);
					}

					// We fix ssrc and timestamping
					fix_rtp_buffer_voip_switched_ssrc (&rtp_buffer, filter_info);

					// Let buffer continue processing
					result = GST_PAD_PROBE_OK;
				}

			}
			gst_rtp_buffer_unmap (&rtp_buffer);
			return result;
		}
	}

	GST_WARNING ("Buffer not mapped to RTP");
	return GST_PAD_PROBE_OK;
}


static gboolean
filter_buffer (GstBuffer ** buffer, guint idx, gpointer user_data)
{
	SipFilterSsrcInfo* filter_info = (SipFilterSsrcInfo*)user_data;

	if (filter_ssrc_rtp_buffer(*buffer, filter_info) == GST_PAD_PROBE_DROP)
		*buffer = NULL;

	return TRUE;
}

static GstPadProbeReturn
filter_ssrc_rtp (GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
	SipFilterSsrcInfo* filter_info = (SipFilterSsrcInfo*) user_data;
	GstBuffer *buffer;

	GST_DEBUG ("Filtering RTP packets from previous flows to this receiver");
	buffer = GST_PAD_PROBE_INFO_BUFFER (info);
	if (buffer != NULL) {
		GST_DEBUG ("RTP buffer received from Filtering RTP packets from previous flows to this receiver");

		return filter_ssrc_rtp_buffer (buffer, filter_info);
	} else  {
		GstBufferList *buffer_list;

		buffer_list = gst_pad_probe_info_get_buffer_list (info);

		if (buffer_list != NULL) {
			GST_DEBUG ("filter old ssrc buffer list RTP");
			if (!gst_buffer_list_foreach(buffer_list, filter_buffer, user_data))
				GST_WARNING("Filtering buffer list for old ssrc failed");
		}
	}
	return GST_PAD_PROBE_OK;
}


static guint32
fix_rtcp_ts (SipFilterSsrcInfo* filter_info, guint32 rtptime)
{
	gint64 aux_ts = rtptime;

	// We fix timestamping to keep kmsrtpsynchronizer happy
	if (filter_info->jump_ts != 0) {
		aux_ts -= filter_info->jump_ts;

		if (aux_ts < 0)
			aux_ts += G_MAXUINT32;
	}

	GST_DEBUG ("Fixing RTCP TS: original %u, fixed %ld", rtptime, aux_ts);
	return aux_ts;
}

static GstPadProbeReturn
filter_ssrc_rtcp (GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
	SipFilterSsrcInfo* filter_info = (SipFilterSsrcInfo*)user_data;
	GstBuffer *buffer;

	buffer = GST_PAD_PROBE_INFO_BUFFER (info);

	GstRTCPBuffer rtcp_buffer = GST_RTCP_BUFFER_INIT;

	GST_DEBUG ("Filtering RTCP buffer from previous flows to this receiver");
    if (gst_rtcp_buffer_map (buffer, GST_MAP_READWRITE, &rtcp_buffer)) {
    	GstRTCPPacket packet;
		gboolean has_packet;

		has_packet = gst_rtcp_buffer_get_first_packet (&rtcp_buffer, &packet);

		GST_DEBUG ("Filtering RTCP packets from previous flows to this receiver");

    	if (has_packet) {
    		GstRTCPType  packet_type = gst_rtcp_packet_get_type (&packet);

    		if (packet_type == GST_RTCP_TYPE_SR) {
        		guint32 ssrc, rtptime, packet_count, octet_count;
        		guint64 ntptime;

    			gst_rtcp_packet_sr_get_sender_info    (&packet, &ssrc, &ntptime, &rtptime, &packet_count, &octet_count);
    			GST_DEBUG ("Got RTCP ssrc: %u ntptime: %lu, rtptime: %u, packet count: %u, octect_count: %u", ssrc, ntptime, rtptime, packet_count, octet_count);
    			if (check_ssrc_rtcp (ssrc, filter_info)) {
    				GST_DEBUG("Unexpected SSRC RTCP packet received: %u, expected: %u", ssrc, filter_info->expected);
    				// If any packet in a buffer has an unexpectd SSRc, all buffer can be dropped
    		    	gst_rtcp_buffer_unmap (&rtcp_buffer);
    				return GST_PAD_PROBE_DROP;
    			} else {
    				if (ssrc != filter_info->expected) {
    					if (filter_info->media_session == AUDIO_RTP_SESSION) {
        					while (has_packet) {
        						switch (packet_type) {
        						case GST_RTCP_TYPE_SR:
        							{
										guint32 fixed_rtptime = fix_rtcp_ts (filter_info, rtptime);
										GST_DEBUG ("Fixed RTCP ssrc: %u ntptime: %lu, rtptime: %u, packet count: %u, octect_count: %u", filter_info->expected, ntptime, fixed_rtptime, packet_count, octet_count);
										gst_rtcp_packet_sr_set_sender_info  (&packet, filter_info->expected, ntptime, fixed_rtptime, packet_count, octet_count);
        							}
                					break;
        						case GST_RTCP_TYPE_BYE:
        							gst_rtcp_packet_bye_add_ssrc  (&packet, filter_info->expected);
        							break;

        						// Feedback packets, should let them go
        						case GST_RTCP_TYPE_APP:
        						case GST_RTCP_TYPE_RR:
        						case GST_RTCP_TYPE_XR:
        						case GST_RTCP_TYPE_RTPFB:
        						case GST_RTCP_TYPE_PSFB:
        							break;
        						case GST_RTCP_TYPE_SDES:
        						case GST_RTCP_TYPE_INVALID:
        						default:
        							gst_rtcp_packet_remove (&packet);
        							break;
        						}
            		    		has_packet = gst_rtcp_packet_move_to_next (&packet);
            		    		packet_type = gst_rtcp_packet_get_type (&packet);
        					}
    					} else if (filter_info->media_session == VIDEO_RTP_SESSION) {
        					while (has_packet) {
        						if (packet_type == GST_RTCP_TYPE_SR) {
                					gst_rtcp_packet_sr_set_sender_info  (&packet, filter_info->expected, ntptime, rtptime, packet_count, octet_count);
        						} else {
        							gst_rtcp_packet_remove (&packet);
        						}
            		    		has_packet = gst_rtcp_packet_move_to_next (&packet);
            		    		packet_type = gst_rtcp_packet_get_type (&packet);
        					}
    					}
    				}
    		    	gst_rtcp_buffer_unmap (&rtcp_buffer);
    				return GST_PAD_PROBE_OK;
    			}
    		}
    	}
    	gst_rtcp_buffer_unmap (&rtcp_buffer);
	}

    return GST_PAD_PROBE_OK;
}


gulong
kms_sip_rtp_filter_setup_probe_rtp (GstPad *pad, SipFilterSsrcInfo* filter_info)
{
	if (filter_info != NULL) {
		GST_DEBUG("Installing RTP probe for %s", GST_ELEMENT_NAME(gst_pad_get_parent_element (pad)));
		return gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_BUFFER_LIST | GST_PAD_PROBE_TYPE_PUSH | GST_PAD_PROBE_TYPE_PULL,
				(GstPadProbeCallback) filter_ssrc_rtp, GUINT_TO_POINTER(filter_info), NULL);
	} else {
	    GST_DEBUG("No RTP probe installed for %s", GST_ELEMENT_NAME(gst_pad_get_parent_element (pad)));
	    return 0;
	}
}

gulong
kms_sip_rtp_filter_setup_probe_rtcp (GstPad *pad, SipFilterSsrcInfo* filter_info)
{
	if (filter_info != NULL) {
	    GST_DEBUG("Installing RTCP probe for %s", GST_ELEMENT_NAME(gst_pad_get_parent_element (pad)));
	    return gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
	        (GstPadProbeCallback) filter_ssrc_rtcp, filter_info, NULL);
	} else {
	    GST_DEBUG("No RTCP probe installed for %s", GST_ELEMENT_NAME(gst_pad_get_parent_element (pad)));
	    return 0;
	}
}


void
kms_sip_rtp_filter_release_probe_rtp (GstPad *pad, gulong probe_id)
{
	if (probe_id == 0)
		return;

    GST_DEBUG("Removing RTP probe for %s", GST_ELEMENT_NAME(gst_pad_get_parent_element (pad)));
    gst_pad_remove_probe (pad, probe_id);

}

void
kms_sip_rtp_filter_release_probe_rtcp (GstPad *pad, gulong probe_id)
{
	if (probe_id == 0)
		return;

    GST_DEBUG("Removing RTCP probe for %s", GST_ELEMENT_NAME(gst_pad_get_parent_element (pad)));
    gst_pad_remove_probe (pad, probe_id);

}

static void
filtering_info_add_ssrc_info (GList** target, GList* source)
{
	GList* it = source;

	while (it != NULL) {
		GST_DEBUG("add_ssrc_info, setting old ssrc %u", GPOINTER_TO_UINT(it->data));
		*target = g_list_append (*target, it->data);
		it = it->next;
	}
}

SipFilterSsrcInfo*
kms_sip_rtp_filter_create_filtering_info (guint32 expected, SipFilterSsrcInfo* previous, guint32 media_session, gboolean continue_stream)
{
	SipFilterSsrcInfo* info = g_new (SipFilterSsrcInfo, 1);

	// Initialize filter_info
	info->expected = expected;
	info->current = expected;
	info->old = NULL;
	info->media_session = media_session;
	info->last_seq = 0;
	info->last_ts = 0;
	info->last_ts_delta = 0;
	info->jump_ts = 0;

	g_rec_mutex_init (&info->mutex);

	GST_DEBUG("create_filtering_info, setting expected ssrc: %u", expected);
	if (previous != NULL) {
		GST_DEBUG("create_filtering_info, setting old ssrc: %u", previous->expected);
		// If we have previous media connection, we need to take note of previous SSRC to discard late packets
		if ((previous->expected != 0) && !continue_stream) {
			info->old = g_list_append (info->old, GUINT_TO_POINTER(previous->expected));
		}
		// We add all previous media connections old SSRC as old ones for current media connection (just in case old packets happen)
		filtering_info_add_ssrc_info (&(info->old), previous->old);
	}

	return info;
}

void kms_sip_rtp_filter_release_filtering_info (SipFilterSsrcInfo* info)
{
	g_rec_mutex_clear (&info->mutex);
	if (info->old != NULL) {
		g_list_free (info->old);
	}
	g_free (info);
}


