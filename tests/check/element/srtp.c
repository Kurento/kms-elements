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
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <commons/constants.h>

/* Test based on jitterbuffer tests */

#define PCMU_BUF_CLOCK_RATE 8000
#define PCMU_BUF_PT 0
#define PCMU_BUF_SSRC 0x01BADBAD
#define PCMU_BUF_MS  20
#define PCMU_BUF_DURATION (PCMU_BUF_MS * GST_MSECOND)
#define PCMU_BUF_SIZE (64000 * PCMU_BUF_MS / 1000)
#define PCMU_RTP_TS_DURATION (PCMU_BUF_CLOCK_RATE * PCMU_BUF_MS / 1000)

#define SRTP_REPLAY_WINDOW_SIZE G_MAXINT16      /* packets */

/* based on rtpjitterbuffer.c */
static GstCaps *
generate_caps (void)
{
  return gst_caps_new_simple ("application/x-rtp",
      "media", G_TYPE_STRING, "audio",
      "clock-rate", G_TYPE_INT, PCMU_BUF_CLOCK_RATE,
      "encoding-name", G_TYPE_STRING, "PCMU",
      "payload", G_TYPE_INT, PCMU_BUF_PT,
      "ssrc", G_TYPE_UINT, PCMU_BUF_SSRC, NULL);
}

/* based on rtpjitterbuffer.c */
static GstBuffer *
generate_test_buffer_full (GstClockTime gst_ts,
    gboolean marker_bit, guint seq_num, guint32 rtp_ts)
{
  GstBuffer *buf;
  guint8 *payload;
  guint i;
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  buf = gst_rtp_buffer_new_allocate (PCMU_BUF_SIZE, 0, 0);
  GST_BUFFER_DTS (buf) = gst_ts;
  GST_BUFFER_PTS (buf) = gst_ts;

  gst_rtp_buffer_map (buf, GST_MAP_READWRITE, &rtp);
  gst_rtp_buffer_set_payload_type (&rtp, PCMU_BUF_PT);
  gst_rtp_buffer_set_marker (&rtp, marker_bit);
  gst_rtp_buffer_set_seq (&rtp, seq_num);
  gst_rtp_buffer_set_timestamp (&rtp, rtp_ts);
  gst_rtp_buffer_set_ssrc (&rtp, PCMU_BUF_SSRC);

  payload = gst_rtp_buffer_get_payload (&rtp);
  for (i = 0; i < PCMU_BUF_SIZE; i++)
    payload[i] = 0xff;

  gst_rtp_buffer_unmap (&rtp);

  return buf;
}

static GstBuffer *
generate_test_buffer (guint seq_num)
{
  return generate_test_buffer_full (seq_num * PCMU_BUF_DURATION,
      TRUE, seq_num, seq_num * PCMU_RTP_TS_DURATION);
}

GST_START_TEST (test_window_size)
{
  GstElement *srtpenc = gst_element_factory_make ("srtpenc", NULL);
  GstHarness *h;
  GstFlowReturn ret;

  g_object_set (srtpenc, "random-key", TRUE, "allow-repeat-tx", TRUE,
      "replay-window-size", SRTP_REPLAY_WINDOW_SIZE, NULL);
  h = gst_harness_new_with_element (srtpenc, "rtp_sink_0", "rtp_src_0");

  gst_harness_set_src_caps (h, generate_caps ());

  ret =
      gst_harness_push (h, generate_test_buffer (SRTP_REPLAY_WINDOW_SIZE + 1));
  fail_unless (ret == GST_FLOW_OK);

  ret = gst_harness_push (h, generate_test_buffer (RTP_RTX_SIZE));
  fail_unless (ret == GST_FLOW_OK);

  ret = gst_harness_push (h, generate_test_buffer (1));
  fail_unless (ret == GST_FLOW_OK);

  ret = gst_harness_push (h, generate_test_buffer (0));
  fail_unless (ret == GST_FLOW_OK);

  gst_harness_teardown (h);
  g_object_unref (srtpenc);
}

GST_END_TEST;

GST_START_TEST (test_allow_repeat_tx)
{
  GstElement *srtpenc = gst_element_factory_make ("srtpenc", NULL);
  GstHarness *h;
  GstFlowReturn ret;

  g_object_set (srtpenc, "random-key", TRUE, "allow-repeat-tx", TRUE,
      "replay-window-size", 64, NULL);
  h = gst_harness_new_with_element (srtpenc, "rtp_sink_0", "rtp_src_0");

  gst_harness_set_src_caps (h, generate_caps ());

  ret = gst_harness_push (h, generate_test_buffer (0));
  fail_unless (ret == GST_FLOW_OK);

  ret = gst_harness_push (h, generate_test_buffer (1));
  fail_unless (ret == GST_FLOW_OK);

  ret = gst_harness_push (h, generate_test_buffer (65));
  fail_unless (ret == GST_FLOW_OK);

  ret = gst_harness_push (h, generate_test_buffer (1));
  fail_unless (ret == GST_FLOW_OK);

  gst_harness_teardown (h);
  g_object_unref (srtpenc);
}

GST_END_TEST;

static Suite *
srtp_suite (void)
{
  Suite *s = suite_create ("srtp");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_window_size);
  tcase_add_test (tc_chain, test_allow_repeat_tx);

  return s;
}

GST_CHECK_MAIN (srtp);
