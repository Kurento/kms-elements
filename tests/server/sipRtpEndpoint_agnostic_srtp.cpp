/*
 * (C) Copyright 2016 Kurento (http://kurento.org/)
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

#define BOOST_TEST_STATIC_LINK
#define BOOST_TEST_PROTECTED_VIRTUAL

#include <boost/test/included/unit_test.hpp>
#include <MediaPipelineImpl.hpp>
#include <objects/FacadeRtpEndpointImpl.hpp>
#include <mutex>
#include <condition_variable>
#include <ModuleManager.hpp>
#include <KurentoException.hpp>
#include <MediaSet.hpp>
#include <MediaElementImpl.hpp>
#include <ConnectionState.hpp>
#include <MediaState.hpp>
#include "SDES.hpp"
#include "CryptoSuite.hpp"

#include <sigc++/connection.h>

#include <RegisterParent.hpp>

using namespace kurento;
using namespace boost::unit_test;

boost::property_tree::ptree config;
std::string mediaPipelineId;
ModuleManager moduleManager;

struct GF {
  GF();
  ~GF();
};

BOOST_GLOBAL_FIXTURE (GF);

GF::GF()
{
  boost::property_tree::ptree ac, audioCodecs, vc, videoCodecs;
  gst_init(nullptr, nullptr);

//  moduleManager.loadModulesFromDirectories ("./src/server:../../kms-omni-build:../../src/server:../../../../kms-omni-build");
  moduleManager.loadModulesFromDirectories ("../../src/server");

  config.add ("configPath", "../../../tests" );
  config.add ("modules.kurento.SdpEndpoint.numAudioMedias", 1);
  config.add ("modules.kurento.SdpEndpoint.numVideoMedias", 1);

  ac.put ("name", "opus/48000/2");
  audioCodecs.push_back (std::make_pair ("", ac) );
  config.add_child ("modules.kurento.SdpEndpoint.audioCodecs", audioCodecs);

  vc.put ("name", "VP8/90000");
  videoCodecs.push_back (std::make_pair ("", vc) );
  config.add_child ("modules.kurento.SdpEndpoint.videoCodecs", videoCodecs);

  mediaPipelineId = moduleManager.getFactory ("MediaPipeline")->createObject (
                      config, "",
                      Json::Value() )->getId();
}

GF::~GF()
{
  MediaSet::deleteMediaSet();
}

#define CRYPTOKEY "00108310518720928b30d38f411493"

static std::shared_ptr<SDES> getCrypto ()
{
	std::shared_ptr<SDES> crypto (new SDES());
	std::shared_ptr<CryptoSuite> cryptoSuite (new kurento::CryptoSuite (kurento::CryptoSuite::AES_128_CM_HMAC_SHA1_80));

	crypto->setCrypto(cryptoSuite);
	crypto->setKey(CRYPTOKEY);
	return crypto;
}


static Json::Value
createSdesJson (std::shared_ptr<SDES> sdes)
{
	Json::Value sdesParams;

	sdesParams ["key"] = sdes->getKey();
	sdesParams ["crypto"] = sdes->getCrypto()->getString();

	return sdesParams;
}

static std::shared_ptr <FacadeRtpEndpointImpl>
createRtpEndpoint (bool useCrypto, bool cryptoAgnostic)
{
  std::shared_ptr <kurento::MediaObjectImpl> rtpEndpoint;
  Json::Value constructorParams;

  constructorParams ["mediaPipeline"] = mediaPipelineId;
  constructorParams ["cryptoAgnostic"] = cryptoAgnostic;
  if (useCrypto) {
	  constructorParams ["crypto"] = createSdesJson (getCrypto ());
  }

  rtpEndpoint = moduleManager.getFactory ("SipRtpEndpoint")->createObject (
                  config, "",
                  constructorParams );

  return std::dynamic_pointer_cast <FacadeRtpEndpointImpl> (rtpEndpoint);
}

static std::shared_ptr<MediaElementImpl> createTestAudioSrc() {
  std::shared_ptr <MediaElementImpl> src = std::dynamic_pointer_cast
      <MediaElementImpl> (MediaSet::getMediaSet()->ref (new  MediaElementImpl (
                            boost::property_tree::ptree(),
                            MediaSet::getMediaSet()->getMediaObject (mediaPipelineId),
                            "dummysrc") ) );

  g_object_set (src->getGstreamerElement(), "audio", TRUE, "video", FALSE, NULL);

  return std::dynamic_pointer_cast <MediaElementImpl> (src);
}


static void
releaseRtpEndpoint (std::shared_ptr<FacadeRtpEndpointImpl> &ep)
{
  std::string id = ep->getId();

  ep.reset();
  MediaSet::getMediaSet ()->release (id);
}

static std::shared_ptr <PassThroughImpl>
createPassThrough ()
{
  std::shared_ptr <kurento::MediaObjectImpl> pt;
  Json::Value constructorParams;

  constructorParams ["mediaPipeline"] = mediaPipelineId;

  pt = moduleManager.getFactory ("PassThrough")->createObject (
                  config, "",
                  constructorParams );

  return std::dynamic_pointer_cast <PassThroughImpl> (pt);
}

static void
releasePassTrhough (std::shared_ptr<PassThroughImpl> &ep)
{
  std::string id = ep->getId();

  ep.reset();
  MediaSet::getMediaSet ()->release (id);
}



static std::shared_ptr<MediaElementImpl> createTestSrc() {
  std::shared_ptr <MediaElementImpl> src = std::dynamic_pointer_cast
      <MediaElementImpl> (MediaSet::getMediaSet()->ref (new  MediaElementImpl (
                            boost::property_tree::ptree(),
                            MediaSet::getMediaSet()->getMediaObject (mediaPipelineId),
                            "dummysrc") ) );

  g_object_set (src->getGstreamerElement(), "audio", TRUE, "video", TRUE, NULL);

  return std::dynamic_pointer_cast <MediaElementImpl> (src);
}

static std::string sdp_test_1 = "v=0\r\n"
								"o=iPECSCM 7619561 7619561 IN IP4 192.168.131.114\r\n"
								"s=SIP Call\r\n"
								"c=IN IP4 192.168.131.114\r\n"
								"t=0 0\r\n"
								"m=audio 0 RTP/SAVP 0\r\n"
								"a=crypto:1 AES_CM_128_HMAC_SHA1_80 dummy\r\n"
								"a=inactive\r\n"
								"m=video 0 RTP/SAVP 98\r\n"
								"a=crypto:1 AES_CM_128_HMAC_SHA1_80 dummy\r\n"
								"a=inactive\r\n"
								"m=audio 23042 RTP/AVP 8 0 18 111\r\n"
								"a=rtpmap:8 PCMA/8000\r\n"
								"a=rtpmap:0 PCMU/8000\r\n"
								"a=rtpmap:18 G729/8000\r\n"
								"a=rtpmap:111 X-nt-inforeq/8000\r\n"
								"a=fmtp:18 annexb=no\r\n"
								"a=ptime:20\r\n"
								"a=sendrecv\r\n"
								"m=video 0 RTP/AVP 98\r\n"
								"a=inactive\r\n";

static std::string sdp_test_2 = "v=0\r\n"
								"o=iPECSCM 7619561 7619561 IN IP4 192.168.131.114\r\n"
								"s=SIP Call\r\n"
								"c=IN IP4 192.168.131.114\r\n"
								"t=0 0\r\n"
								"m=audio 0 RTP/SAVPF 0\r\n"
								"a=crypto:1 AES_CM_128_HMAC_SHA1_80 dummy\r\n"
								"a=inactive\r\n"
								"m=audio 23042 RTP/AVPF 8 0 18 111\r\n"
								"a=rtpmap:8 PCMA/8000\r\n"
								"a=rtpmap:0 PCMU/8000\r\n"
								"a=rtpmap:18 G729/8000\r\n"
								"a=rtpmap:111 X-nt-inforeq/8000\r\n"
								"a=fmtp:18 annexb=no\r\n"
								"a=ptime:20\r\n"
								"a=sendrecv\r\n";


static std::string sdp_test_3 = "v=0\r\n"
								"o=iPECSCM 7619561 7619561 IN IP4 192.168.131.114\r\n"
								"s=SIP Call\r\n"
								"c=IN IP4 192.168.131.114\r\n"
								"t=0 0\r\n"
								"m=audio 23042 RTP/AVP 8 0 18 111\r\n"
								"a=rtpmap:8 PCMA/8000\r\n"
								"a=rtpmap:0 PCMU/8000\r\n"
								"a=rtpmap:18 G729/8000\r\n"
								"a=rtpmap:111 X-nt-inforeq/8000\r\n"
								"a=fmtp:18 annexb=no\r\n"
								"a=ptime:20\r\n"
								"a=sendrecv\r\n";

static std::string sdp_test_4  = "v=0\r\n"
                                 "o=- 3794323608 3794323608 IN IP4 172.17.0.2\r\n"
                                 "s=Kurento Media Server\r\n"
                                 "c=IN IP4 172.17.0.2\r\n"
                                 "t=0 0\r\n"
                                 "m=audio 1268 RTP/SAVPF 96 0 97\r\n"
                                 "a=setup:actpass\r\n"
                                 "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
                                 "a=rtpmap:96 opus/48000/2\r\n"
                                 "a=rtpmap:97 AMR/8000\r\n"
                                 "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:MDAxMDgzMTA1MTg3MjA5MjhiMzBkMzhmNDExNDkz\r\n"
                                 "a=sendrecv\r\n"
                                 "a=mid:audio0\r\n"
                                 "a=ssrc:2630252136 cname:user2385001219@host-3046046\r\n"
                                 "m=video 54142 RTP/SAVPF 102 103\r\n"
                                 "a=setup:actpass\r\n"
                                 "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
                                 "a=rtpmap:102 VP8/90000\r\n"
                                 "a=rtpmap:103 H264/90000\r\n"
                                 "a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:MDAxMDgzMTA1MTg3MjA5MjhiMzBkMzhmNDExNDkz\r\n"
                                 "a=sendrecv\r\n"
                                 "a=mid:video0\r\n"
                                 "a=rtcp-fb:102 nack\r\n"
                                 "a=rtcp-fb:102 nack pli\r\n"
                                 "a=rtcp-fb:102 goog-remb\r\n"
                                 "a=rtcp-fb:102 ccm fir\r\n"
                                 "a=rtcp-fb:103 nack\r\n"
                                 "a=rtcp-fb:103 nack pli\r\n"
                                 "a=rtcp-fb:103 ccm fir\r\n"
                                 "a=ssrc:1395487615 cname:user2385001219@host-3046046\r\n"
                                 "m=audio 1268 RTP/AVPF 96 0 97\r\n"
                                 "a=setup:actpass\r\n"
                                 "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
                                 "a=rtpmap:96 opus/48000/2\r\n"
                                 "a=rtpmap:97 AMR/8000\r\n"
                                 "a=sendrecv\r\n"
                                 "a=mid:audio0\r\n"
                                 "a=ssrc:868439451 cname:user2385001219@host-3046046\r\n"
                                 "m=video 54142 RTP/AVPF 102 103\r\n"
                                 "a=setup:actpass\r\n"
                                 "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n"
                                 "a=rtpmap:102 VP8/90000\r\n"
                                 "a=rtpmap:103 H264/90000\r\n"
                                 "a=sendrecv\r\n"
                                 "a=mid:video0\r\n"
                                 "a=rtcp-fb:102 nack\r\n"
                                 "a=rtcp-fb:102 nack pli\r\n"
                                 "a=rtcp-fb:102 goog-remb\r\n"
                                 "a=rtcp-fb:102 ccm fir\r\n"
                                 "a=rtcp-fb:103 nack\r\n"
                                 "a=rtcp-fb:103 nack pli\r\n"
                                 "a=rtcp-fb:103 ccm fir\r\n"
                                 "a=ssrc:756966127 cname:user2385001219@host-3046046\r\n";


static void
releaseTestSrc (std::shared_ptr<MediaElementImpl> &ep)
{
  std::string id = ep->getId();

  ep.reset();
  MediaSet::getMediaSet ()->release (id);
}

static std::shared_ptr<MediaElementImpl> getMediaElement (std::shared_ptr<PassThroughImpl> element)
{
	return std::dynamic_pointer_cast<MediaElementImpl> (element);
}

static bool
check_valid_answer (std::string sdp_answer)
{
	GstSDPMessage *sdp;
	const GstSDPMedia *media;
	guint medias_number;
	guint idx = 0;

	gst_sdp_message_new (&sdp);
	gst_sdp_message_parse_buffer  ((const guint8 *)sdp_answer.c_str(), sdp_answer.length(), sdp);
	medias_number = gst_sdp_message_medias_len  (sdp);
	while (idx < medias_number) {
		const gchar *attr_value;

		media = gst_sdp_message_get_media (sdp, idx);
		attr_value = gst_sdp_media_get_attribute_val (media, "inactive");
		if (attr_value == NULL) {
			if (gst_sdp_media_get_port (media) != 0)
				return true;
		}

		idx++;
	}
	return false;
}

static void
test_valid_answer (std::string test_sdp)
{
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpAnswerer = createRtpEndpoint (true, true);

  try {
	  std::string answer = rtpEpAnswerer->processOffer (test_sdp);
	  BOOST_TEST_MESSAGE ("answer: " + answer);

	  if (!check_valid_answer (answer)) {
		  BOOST_ERROR ("Ther must be at least one valid media");
	  }

  } catch (kurento::KurentoException& e) {
	 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
  }

  releaseRtpEndpoint (rtpEpAnswerer);
}


static void
reconnection_generate_offer_state_changes_impl (bool cryptoOffer, bool agnosticOffer, bool cryptoAnswer, bool agnosticAnswer, bool mediaShouldFlow)
{
  std::atomic<bool> media_state_changed (false);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpOfferer = createRtpEndpoint (cryptoOffer, agnosticOffer);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpAnswerer = createRtpEndpoint (cryptoAnswer, agnosticAnswer);
  std::shared_ptr <MediaElementImpl> src = createTestSrc();
  std::shared_ptr <PassThroughImpl> pt = createPassThrough ();
  std::atomic<bool> conn_state_changed (false);
  std::condition_variable cv;
  std::mutex mtx;
  std::unique_lock<std::mutex> lck (mtx);

  src->connect(rtpEpOfferer);
  rtpEpAnswerer->connect(pt);

  sigc::connection conn = getMediaElement(pt)->signalMediaFlowInStateChange.connect([&] (
		  MediaFlowInStateChange event) {
	  	  	  std::shared_ptr<MediaFlowState> state = event.getState();
	  	  	  if (state->getValue() == MediaFlowState::FLOWING) {
		  	  	  BOOST_CHECK (state->getValue() == MediaFlowState::FLOWING);
		  	  	  media_state_changed = true;
		  	  	  cv.notify_one();
	  	  	  }
  	  	  }
  );

  try {
	  std::string offer = rtpEpOfferer->generateOffer ();
	  BOOST_TEST_MESSAGE ("offer1: " + offer);

	  std::string answer = rtpEpAnswerer->processOffer (offer);
	  BOOST_TEST_MESSAGE ("answer: " + answer);

	  rtpEpOfferer->processAnswer (answer);

	  cv.wait_for (lck, std::chrono::milliseconds(1500), [&] () {
	    return media_state_changed.load();
	  });

	  conn.disconnect ();
	  if (!media_state_changed && mediaShouldFlow) {
	    BOOST_ERROR ("Not media Flowing");
	  }

  } catch (kurento::KurentoException& e) {
	 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
  }

  if (rtpEpAnswerer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  if (rtpEpOfferer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  src->disconnect(rtpEpOfferer);
  rtpEpAnswerer->disconnect (pt);
  releaseRtpEndpoint (rtpEpOfferer);
  releaseRtpEndpoint (rtpEpAnswerer);
  releasePassTrhough (pt);
  releaseTestSrc (src);
}

static std::string offer_1_group_call_crash =
		"v=0\n"
        "o=iPECSCM 356207 356207 IN IP4 192.168.125.52\n"
		"s=iPECSCM Call\n"
		"c=IN IP4 192.168.125.52\n"
		"t=0 0\n"
		"m=audio 0 RTP/SAVP 0\n"
		"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:YzQ2NTQzOGNhZThhODU4YjA5ZDIzMjkxYjE2NjIy\n"
		"a=inactive\n"
		"m=video 0 RTP/SAVP 98\n"
		"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:ZDU0MTJmODdkNzViYmY4NjQ5MDI5ZDFlMGZhMDhh\n"
		"a=inactive\n"
		"m=audio 8092 RTP/AVP 8 0 18 111\n"
		"a=rtpmap:8 PCMA/8000\n"
		"a=rtpmap:0 PCMU/8000\n"
		"a=rtpmap:18 G729/8000\n"
		"a=rtpmap:111 X-nt-inforeq/8000\n"
		"a=fmtp:18 annexb=no\n"
		"a=ptime:20\n"
		"a=sendrecv\n"
		"m=video 0 RTP/AVP 98\n"
		"a=inactive";

static std::string offer_2_group_call_crash =
		"v=0\n"
        "o=iPECSCM 356272 356272 IN IP4 192.168.125.52\n"
		"s=iPECSCM Call\n"
		"c=IN IP4 192.168.125.52\n"
		"t=0 0\n"
		"m=audio 0 RTP/SAVP 0\n"
		"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:MjcxNzRkZjI2YjJiOTlkYTA3MmNmOWRiNmZlZjk0\n"
		"a=inactive\n"
		"m=video 0 RTP/SAVP 98\n"
		"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:ZjgzOGI1ODY1OTkyNDU1ODc2YWY5MmMxMWM3MTgx\n"
		"a=inactive\n"
		"m=audio 8092 RTP/AVP 0 111\n"
		"a=rtpmap:0 PCMU/8000\n"
		"a=rtpmap:111 X-nt-inforeq/8000\n"
		"a=ptime:20\n"
		"a=sendrecv\n"
		"m=video 0 RTP/AVP 98\n"
		"a=inactive";


static std::string same_port_answer =
		"v=0\n"
		"o=iPECSCM 973597 973597 IN IP4 192.168.122.185\n"
		"s=iPECSCM Call\n"
		"c=IN IP4 192.168.122.185\n"
		"t=0 0\n"
		"m=audio 6050 RTP/AVP 0 111\n"
		"a=rtpmap:0 PCMU/8000\n"
		"a=rtpmap:111 X-nt-inforeq/8000\n"
		"a=ptime:20\n"
		"a=sendrecv";

static void
test_same_port_crash ()
{
	  std::atomic<bool> media_state_changed (false);
	  std::atomic<bool> media_state_changed2 (false);
	  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpOfferer = createRtpEndpoint (true, true);
	  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpAnswerer = createRtpEndpoint (false, true);
	  std::shared_ptr <PassThroughImpl> pt = createPassThrough ();
	  std::shared_ptr <PassThroughImpl> pt2 = createPassThrough ();
	  std::shared_ptr <MediaElementImpl> src = createTestAudioSrc();
	  std::condition_variable cv;
	  std::condition_variable cv2;
	  std::mutex mtx;
	  std::unique_lock<std::mutex> lck (mtx);
	  std::mutex mtx2;
	  std::unique_lock<std::mutex> lck2 (mtx2);

	  rtpEpOfferer->connect(pt);
	  src->connect(rtpEpAnswerer);

	  sigc::connection conn = getMediaElement(pt)->signalMediaFlowInStateChange.connect([&] (
			  MediaFlowInStateChange event) {
		  	  	  std::shared_ptr<MediaFlowState> state = event.getState();
		  	  	  std::shared_ptr<MediaType> media = event.getMediaType();

		  	  	  if ((state->getValue() == MediaFlowState::FLOWING) && (media->getValue() == MediaType::AUDIO)) {
			  	  	  BOOST_CHECK (state->getValue() == MediaFlowState::FLOWING);
			  	  	  media_state_changed = true;
			  	  	  cv.notify_one();
		  	  	  }
	  	  	  }
	  );

try {
	  std::string offer = rtpEpOfferer->generateOffer ();
	  BOOST_TEST_MESSAGE ("offer: " + offer);

	  std::string answer = rtpEpAnswerer->processOffer (offer);
	  BOOST_TEST_MESSAGE ("answer1: " + answer);

	  rtpEpOfferer->processAnswer(answer);

	  // First stream
	  cv.wait (lck, [&] () {
	    return media_state_changed.load();
	  });
	  conn.disconnect ();
	  rtpEpOfferer->disconnect(pt);

	  if (!media_state_changed) {
	    BOOST_ERROR ("Not media Flowing");
	  }

	  rtpEpOfferer->processAnswer (answer);

	  conn = getMediaElement(pt2)->signalMediaFlowInStateChange.connect([&] (
			  MediaFlowInStateChange event) {
		  	  	  std::shared_ptr<MediaFlowState> state = event.getState();
		  	  	  std::shared_ptr<MediaType> media = event.getMediaType();

		  	  	  if ((state->getValue() == MediaFlowState::FLOWING) && (media->getValue() == MediaType::AUDIO)) {
			  	  	  BOOST_CHECK (state->getValue() == MediaFlowState::FLOWING);
			  	  	  media_state_changed2 = true;
			  	  	  cv2.notify_one();
		  	  	  }
	  	  	  }
	  );
	  rtpEpOfferer->connect(pt2);

//	  std::shared_ptr<MediaType> audioTypePtr (new MediaType (MediaType::AUDIO));
//	  while (!(pt->isMediaFlowingIn(audioTypePtr))) {
//		  std::this_thread::sleep_for(std::chrono::milliseconds(500));
//	  }

	  // First stream
	  cv2.wait (lck2, [&] () {
	    return media_state_changed2.load();
	  });
	  conn.disconnect ();

//	  if (!media_state_changed2) {
//	    BOOST_ERROR ("Not media Flowing");
//	  }


} catch (kurento::KurentoException& e) {
	 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
}

if (rtpEpAnswerer->getConnectionState ()->getValue () !=
  ConnectionState::CONNECTED) {
BOOST_ERROR ("Connection must be connected");
}

if (rtpEpOfferer->getConnectionState ()->getValue () !=
  ConnectionState::CONNECTED) {
BOOST_ERROR ("Connection must be connected");
}

releaseRtpEndpoint (rtpEpOfferer);
releaseRtpEndpoint (rtpEpAnswerer);
releasePassTrhough (pt);
releaseTestSrc (src);
}

static void
test_group_call_crash ()
{
	  bool cryptoOffer = false;
	  bool agnosticOffer = true;

	  std::atomic<bool> media_state_changed (false);
	  std::shared_ptr <FacadeRtpEndpointImpl> rtpElement = createRtpEndpoint (cryptoOffer, agnosticOffer);
	  std::shared_ptr <PassThroughImpl> pt = createPassThrough ();


	  rtpElement->connect(pt);
	  try {
		  std::string offer = offer_1_group_call_crash;
		  BOOST_TEST_MESSAGE ("offer1: " + offer);

		  std::string answer = rtpElement->processOffer(offer);

		  BOOST_TEST_MESSAGE ("answer1: " + answer);

		  offer = offer_2_group_call_crash;
		  BOOST_TEST_MESSAGE ("offer1: " + offer);
		  answer = rtpElement->processOffer(offer);
		  BOOST_TEST_MESSAGE ("answer1: " + answer);

	  } catch (kurento::KurentoException& e) {
		 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
	  }

	  rtpElement->disconnect(pt);

	  releaseRtpEndpoint (rtpElement);
	  releasePassTrhough (pt);
}

static void
test_error_on_answer_without_one_media ()
{
  bool cryptoOffer = true;
  bool agnosticOffer = true;
  bool mediaShouldFlow = true;

  std::atomic<bool> media_state_changed (false);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpOfferer = createRtpEndpoint (cryptoOffer, agnosticOffer);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpAnswerer = createRtpEndpoint (true, false);
  std::shared_ptr <MediaElementImpl> src = createTestSrc();
  std::shared_ptr <PassThroughImpl> pt = createPassThrough ();
  std::atomic<bool> conn_state_changed (false);
  std::condition_variable cv;
  std::mutex mtx;
  std::unique_lock<std::mutex> lck (mtx);

  src->connect(rtpEpOfferer);
  rtpEpAnswerer->connect(pt);

  sigc::connection conn = getMediaElement(pt)->signalMediaFlowInStateChange.connect([&] (
		  MediaFlowInStateChange event) {
	  	  	  std::shared_ptr<MediaFlowState> state = event.getState();
	  	  	  if (state->getValue() == MediaFlowState::FLOWING) {
		  	  	  BOOST_CHECK (state->getValue() == MediaFlowState::FLOWING);
		  	  	  media_state_changed = true;
		  	  	  cv.notify_one();
	  	  	  }
  	  	  }
  );

  try {
	  std::string offer = rtpEpOfferer->generateOffer ();
	  BOOST_TEST_MESSAGE ("offer1: " + offer);

	  	std::string mline ("m=");
	  	std::size_t mediaStart, removedMediaStart;

	  	mediaStart = offer.find (mline);
	  	if (mediaStart != std::string::npos) {
	  		removedMediaStart = offer.find(mline, mediaStart+1);
	  		if (removedMediaStart != std::string::npos) {
  				offer = offer.substr(0, removedMediaStart);
	  		}
	  	}

	  std::string answer = rtpEpAnswerer->processOffer(offer);

	  answer = answer.append("m=video 0 RTP/SAVP 98\r\n"
			  	  	  	  	 "a=crypto:1 AES_CM_128_HMAC_SHA1_80 dummy\r\n"
			  	  	  	  	 "a=inactive\r\n"
			  	  	  	  	 "m=audio 0 RTP/AVP 0\r\n"
			  	  	  	  	 "a=inactive\r\n"
			  	  	  	  	 "m=video 0 RTP/AVP 98\r\n"
			  	  	  	  	 "a=inactive");

	  rtpEpOfferer->processAnswer (answer);

	  cv.wait_for (lck, std::chrono::milliseconds(1500), [&] () {
	    return media_state_changed.load();
	  });

	  conn.disconnect ();
	  if (!media_state_changed && mediaShouldFlow) {
	    BOOST_ERROR ("Not media Flowing");
	  }

  } catch (kurento::KurentoException& e) {
	 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
  }

  if (rtpEpAnswerer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  if (rtpEpOfferer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  src->disconnect(rtpEpOfferer);
  rtpEpAnswerer->disconnect (pt);
  releaseRtpEndpoint (rtpEpOfferer);
  releaseRtpEndpoint (rtpEpAnswerer);
  releasePassTrhough (pt);
  releaseTestSrc (src);
}


static std::string
removeCryptoMedias (std::string sdp)
{
	std::string mline ("m=");
	std::size_t mediaStart, nonCryptoMediaStart;

	mediaStart = sdp.find (mline);
	if (mediaStart != std::string::npos) {
		nonCryptoMediaStart = sdp.find(mline, mediaStart+1);
		if (nonCryptoMediaStart != std::string::npos) {
			nonCryptoMediaStart = sdp.find(mline, nonCryptoMediaStart+1);
			if (nonCryptoMediaStart != std::string::npos) {
				return sdp.substr(0, mediaStart).append(sdp.substr (nonCryptoMediaStart));
			}
		}
	}
	return sdp;
}

static std::string
addCryptoMedias (std::string sdp)
{
	std::string mline ("m=");
	std::size_t mediaStart;
	std::string cryptoLines ("m=audio 0 RTP/SAVPF 96\r\na=inactive\r\nm=video 0 RTP/SAVPF 111\r\na=inactive\r\n");

	mediaStart = sdp.find (mline);
	if (mediaStart != std::string::npos) {
		return sdp.substr(0, mediaStart).append(cryptoLines).append(sdp.substr(mediaStart, std::string::npos));
	}
	return sdp;
}

static std::string
removeNonCryptoMedias (std::string sdp)
{
	std::string mline ("m=");
	std::size_t mediaStart, nonCryptoMediaStart;

	mediaStart = sdp.find (mline);
	if (mediaStart != std::string::npos) {
		nonCryptoMediaStart = sdp.find(mline, mediaStart+1);
		if (nonCryptoMediaStart != std::string::npos) {
			nonCryptoMediaStart = sdp.find(mline, nonCryptoMediaStart+1);
			if (nonCryptoMediaStart != std::string::npos) {
				return sdp.substr(0, nonCryptoMediaStart);
			}
		}
	}
	return sdp;
}

static std::string
addNonCryptoMedias (std::string sdp)
{
	std::string mline ("m=");
	std::string nonCryptoLines ("m=audio 0 RTP/AVPF 96\r\na=inactive\r\nm=video 0 RTP/AVPF 111\r\na=inactive\r\n");

	return sdp.append(nonCryptoLines);
}

static void
reconnection_generate_offer_state_changes_impl_alt ()
{
  bool cryptoOffer = true, agnosticOffer = true, cryptoAnswer = false, agnosticAnswer = false, mediaShouldFlow = false;
  std::atomic<bool> media_state_changed (false);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpOfferer = createRtpEndpoint (cryptoOffer, agnosticOffer);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpAnswerer = createRtpEndpoint (cryptoAnswer, agnosticAnswer);
  std::shared_ptr <MediaElementImpl> src = createTestSrc();
  std::shared_ptr <PassThroughImpl> pt = createPassThrough ();
  std::atomic<bool> conn_state_changed (false);
  std::condition_variable cv;
  std::mutex mtx;
  std::unique_lock<std::mutex> lck (mtx);

  src->connect(rtpEpOfferer);
  rtpEpAnswerer->connect(pt);

  sigc::connection conn = getMediaElement(pt)->signalMediaFlowInStateChange.connect([&] (
		  MediaFlowInStateChange event) {
	  	  	  std::shared_ptr<MediaFlowState> state = event.getState();
	  	  	  if (state->getValue() == MediaFlowState::FLOWING) {
		  	  	  BOOST_CHECK (state->getValue() == MediaFlowState::FLOWING);
		  	  	  media_state_changed = true;
		  	  	  cv.notify_one();
	  	  	  }
  	  	  }
  );

  try {
	  std::string offer = rtpEpOfferer->generateOffer ();
	  BOOST_TEST_MESSAGE ("offer1: " + offer);

	  offer = removeCryptoMedias (offer);

	  std::string answer = rtpEpAnswerer->processOffer (offer);
	  BOOST_TEST_MESSAGE ("answer: " + answer);

	  rtpEpOfferer->processAnswer (answer);

	  cv.wait_for (lck, std::chrono::seconds(5), [&] () {
	    return media_state_changed.load();
	  });

	  conn.disconnect ();
	  if (!media_state_changed && mediaShouldFlow) {
	    BOOST_ERROR ("Not media Flowing");
	  }

  } catch (kurento::KurentoException& e) {
	 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
  }

  if (rtpEpAnswerer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  if (rtpEpOfferer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  src->disconnect(rtpEpOfferer);
  rtpEpAnswerer->disconnect (pt);
  releaseRtpEndpoint (rtpEpOfferer);
  releaseRtpEndpoint (rtpEpAnswerer);
  releasePassTrhough (pt);
  releaseTestSrc (src);
}

static void
reconnection_generate_offer_state_changes_impl_alt2 ()
{
  bool cryptoOffer = true, agnosticOffer = true, cryptoAnswer = false, agnosticAnswer = false, mediaShouldFlow = false;
  std::atomic<bool> media_state_changed (false);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpOfferer = createRtpEndpoint (cryptoOffer, agnosticOffer);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpAnswerer = createRtpEndpoint (cryptoAnswer, agnosticAnswer);
  std::shared_ptr <MediaElementImpl> src = createTestSrc();
  std::shared_ptr <PassThroughImpl> pt = createPassThrough ();
  std::atomic<bool> conn_state_changed (false);
  std::condition_variable cv;
  std::mutex mtx;
  std::unique_lock<std::mutex> lck (mtx);

  src->connect(rtpEpOfferer);
  rtpEpAnswerer->connect(pt);

  sigc::connection conn = getMediaElement(pt)->signalMediaFlowInStateChange.connect([&] (
		  MediaFlowInStateChange event) {
	  	  	  std::shared_ptr<MediaFlowState> state = event.getState();
	  	  	  if (state->getValue() == MediaFlowState::FLOWING) {
		  	  	  BOOST_CHECK (state->getValue() == MediaFlowState::FLOWING);
		  	  	  media_state_changed = true;
		  	  	  cv.notify_one();
	  	  	  }
  	  	  }
  );

  try {
	  std::string offer = rtpEpOfferer->generateOffer ();
	  BOOST_TEST_MESSAGE ("offer1: " + offer);

	  offer = removeCryptoMedias (offer);

	  std::string answer = rtpEpAnswerer->processOffer (offer);
	  BOOST_TEST_MESSAGE ("answer: " + answer);

	  answer = addCryptoMedias (answer);

	  rtpEpOfferer->processAnswer (answer);

	  cv.wait_for (lck, std::chrono::seconds(5), [&] () {
	    return media_state_changed.load();
	  });

	  conn.disconnect ();
	  if (!media_state_changed && mediaShouldFlow) {
	    BOOST_ERROR ("Not media Flowing");
	  }

  } catch (kurento::KurentoException& e) {
	 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
  }

  if (rtpEpAnswerer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  if (rtpEpOfferer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  src->disconnect(rtpEpOfferer);
  rtpEpAnswerer->disconnect (pt);
  releaseRtpEndpoint (rtpEpOfferer);
  releaseRtpEndpoint (rtpEpAnswerer);
  releasePassTrhough (pt);
  releaseTestSrc (src);
}

static void
reconnection_generate_offer_state_changes_impl_alt_crypto ()
{
  bool cryptoOffer = true, agnosticOffer = true, cryptoAnswer = true, agnosticAnswer = false, mediaShouldFlow = false;
  std::atomic<bool> media_state_changed (false);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpOfferer = createRtpEndpoint (cryptoOffer, agnosticOffer);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpAnswerer = createRtpEndpoint (cryptoAnswer, agnosticAnswer);
  std::shared_ptr <MediaElementImpl> src = createTestSrc();
  std::shared_ptr <PassThroughImpl> pt = createPassThrough ();
  std::atomic<bool> conn_state_changed (false);
  std::condition_variable cv;
  std::mutex mtx;
  std::unique_lock<std::mutex> lck (mtx);

  src->connect(rtpEpOfferer);
  rtpEpAnswerer->connect(pt);

  sigc::connection conn = getMediaElement(pt)->signalMediaFlowInStateChange.connect([&] (
		  MediaFlowInStateChange event) {
	  	  	  std::shared_ptr<MediaFlowState> state = event.getState();
	  	  	  if (state->getValue() == MediaFlowState::FLOWING) {
		  	  	  BOOST_CHECK (state->getValue() == MediaFlowState::FLOWING);
		  	  	  media_state_changed = true;
		  	  	  cv.notify_one();
	  	  	  }
  	  	  }
  );

  try {
	  std::string offer = rtpEpOfferer->generateOffer ();
	  BOOST_TEST_MESSAGE ("offer1: " + offer);

	  offer = removeNonCryptoMedias (offer);

	  std::string answer = rtpEpAnswerer->processOffer (offer);
	  BOOST_TEST_MESSAGE ("answer: " + answer);

	  rtpEpOfferer->processAnswer (answer);

	  cv.wait_for (lck, std::chrono::seconds(5), [&] () {
	    return media_state_changed.load();
	  });

	  conn.disconnect ();
	  if (!media_state_changed && mediaShouldFlow) {
	    BOOST_ERROR ("Not media Flowing");
	  }

  } catch (kurento::KurentoException& e) {
	 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
  }

  if (rtpEpAnswerer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  if (rtpEpOfferer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  src->disconnect(rtpEpOfferer);
  rtpEpAnswerer->disconnect (pt);
  releaseRtpEndpoint (rtpEpOfferer);
  releaseRtpEndpoint (rtpEpAnswerer);
  releasePassTrhough (pt);
  releaseTestSrc (src);
}

static void
reconnection_generate_offer_state_changes_impl_alt2_crypto ()
{
  bool cryptoOffer = true, agnosticOffer = true, cryptoAnswer = true, agnosticAnswer = false, mediaShouldFlow = false;
  std::atomic<bool> media_state_changed (false);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpOfferer = createRtpEndpoint (cryptoOffer, agnosticOffer);
  std::shared_ptr <FacadeRtpEndpointImpl> rtpEpAnswerer = createRtpEndpoint (cryptoAnswer, agnosticAnswer);
  std::shared_ptr <MediaElementImpl> src = createTestSrc();
  std::shared_ptr <PassThroughImpl> pt = createPassThrough ();
  std::atomic<bool> conn_state_changed (false);
  std::condition_variable cv;
  std::mutex mtx;
  std::unique_lock<std::mutex> lck (mtx);

  src->connect(rtpEpOfferer);
  rtpEpAnswerer->connect(pt);

  sigc::connection conn = getMediaElement(pt)->signalMediaFlowInStateChange.connect([&] (
		  MediaFlowInStateChange event) {
	  	  	  std::shared_ptr<MediaFlowState> state = event.getState();
	  	  	  if (state->getValue() == MediaFlowState::FLOWING) {
		  	  	  BOOST_CHECK (state->getValue() == MediaFlowState::FLOWING);
		  	  	  media_state_changed = true;
		  	  	  cv.notify_one();
	  	  	  }
  	  	  }
  );

  try {
	  std::string offer = rtpEpOfferer->generateOffer ();
	  BOOST_TEST_MESSAGE ("offer1: " + offer);

	  offer = removeNonCryptoMedias (offer);

	  std::string answer = rtpEpAnswerer->processOffer (offer);
	  BOOST_TEST_MESSAGE ("answer: " + answer);

	  answer = addNonCryptoMedias (answer);

	  rtpEpOfferer->processAnswer (answer);

	  cv.wait_for (lck, std::chrono::seconds(5), [&] () {
	    return media_state_changed.load();
	  });

	  conn.disconnect ();
	  if (!media_state_changed && mediaShouldFlow) {
	    BOOST_ERROR ("Not media Flowing");
	  }

  } catch (kurento::KurentoException& e) {
	 BOOST_ERROR("Unwanted Kurento Exception managing offer/answer");
  }

  if (rtpEpAnswerer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  if (rtpEpOfferer->getConnectionState ()->getValue () !=
      ConnectionState::CONNECTED) {
    BOOST_ERROR ("Connection must be connected");
  }

  src->disconnect(rtpEpOfferer);
  rtpEpAnswerer->disconnect (pt);
  releaseRtpEndpoint (rtpEpOfferer);
  releaseRtpEndpoint (rtpEpAnswerer);
  releasePassTrhough (pt);
  releaseTestSrc (src);
}


static void
srtp_agnostic_case_1()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: no crypto no agnostic, answerer: no crypto no agnostic");
	  reconnection_generate_offer_state_changes_impl (false, false, false, false, true);
}

static void
srtp_agnostic_case_2()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: no crypto no agnostic, answerer: no crypto yes agnostic");
	  reconnection_generate_offer_state_changes_impl (false, false, false, true, true);
}

static void
srtp_agnostic_case_3()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: no crypto no agnostic, answerer: yes crypto no agnostic");
	  reconnection_generate_offer_state_changes_impl (false, false, true, false, false);
}

static void
srtp_agnostic_case_4()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: no crypto no agnostic, answerer: yes crypto yes agnostic");
	  reconnection_generate_offer_state_changes_impl (false, false, true, true, true);
}

static void
srtp_agnostic_case_5()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: no crypto yes agnostic, answerer: no crypto no agnostic");
	  reconnection_generate_offer_state_changes_impl (false, true, false, false, true);
}

static void
srtp_agnostic_case_6()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: no crypto yes agnostic, answerer: no crypto yes agnostic");
	  reconnection_generate_offer_state_changes_impl (false, true, false, true, true);
}

static void
srtp_agnostic_case_7()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: no crypto yes agnostic, answerer: yes crypto no agnostic");
	  reconnection_generate_offer_state_changes_impl (false, true, true, false, false);
}

static void
srtp_agnostic_case_8()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: no crypto yes agnostic, answerer: yes crypto yes agnostic");
	  reconnection_generate_offer_state_changes_impl (false, true, true, true, true);
}

static void
srtp_agnostic_case_9()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto no agnostic, answerer: no crypto no agnostic");
	  reconnection_generate_offer_state_changes_impl (true, false, false, false, false);
}

static void
srtp_agnostic_case_10()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto no agnostic, answerer: no crypto yes agnostic");
	  reconnection_generate_offer_state_changes_impl (true, false, false, true, true);
}

static void
srtp_agnostic_case_11()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto no agnostic, answerer: yes crypto no agnostic");
	  reconnection_generate_offer_state_changes_impl (true, false, true, false, true);
}

static void
srtp_agnostic_case_12()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto no agnostic, answerer: yes crypto yes agnostic");
	  reconnection_generate_offer_state_changes_impl (true, false, true, true, true);
}

static void
srtp_agnostic_case_13()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto yes agnostic, answerer: no crypto no agnostic");
	  reconnection_generate_offer_state_changes_impl (true, true, false, false, false);
}

static void
srtp_agnostic_case_13_b()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto yes agnostic, answerer: no crypto no agnostic (alternate version)");
	  reconnection_generate_offer_state_changes_impl_alt ();
}

static void
srtp_agnostic_case_13_c()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto yes agnostic, answerer: no crypto no agnostic (alternate version full media answer)");
	  reconnection_generate_offer_state_changes_impl_alt2 ();
}


static void
srtp_agnostic_case_14()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto yes agnostic, answerer: no crypto yes agnostic");
	  reconnection_generate_offer_state_changes_impl (true, true, false, true, true);
}

static void
srtp_agnostic_case_15()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto yes agnostic, answerer: yes crypto no agnostic");
	  reconnection_generate_offer_state_changes_impl (true, true, true, false, true);
}

static void
srtp_agnostic_case_15_b()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto yes agnostic, answerer: yes crypto no agnostic (alternate version)");
	  reconnection_generate_offer_state_changes_impl_alt_crypto ();
}

static void
srtp_agnostic_case_15_c()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto yes agnostic, answerer: yes crypto no agnostic (alternate veriosn full media answer");
	  reconnection_generate_offer_state_changes_impl_alt2_crypto ();
}

static void
srtp_agnostic_case_16()
{
	  BOOST_TEST_MESSAGE ("Start test: offerer: yes crypto yes agnostic, answerer: yes crypto yes agnostic");
	  reconnection_generate_offer_state_changes_impl (true, true, true, true, true);
}


static void
test_sdp_offer_1()
{
	BOOST_TEST_MESSAGE ("Start test: Testing SDP 1");
	test_valid_answer (sdp_test_1);
}

static void
test_sdp_offer_2()
{
	BOOST_TEST_MESSAGE ("Start test: Testing SDP 2");
	test_valid_answer (sdp_test_2);
}

static void
test_sdp_offer_3()
{
	BOOST_TEST_MESSAGE ("Start test: Testing SDP 3");
	test_valid_answer (sdp_test_3);
}

static void
test_sdp_offer_4()
{
	BOOST_TEST_MESSAGE ("Start test: Testing SDP 4");
	test_valid_answer (sdp_test_4);
}











test_suite *
init_unit_test_suite ( int , char *[] )
{
  test_suite *test = BOOST_TEST_SUITE ( "SipRtpEndpoint" );

  test->add (BOOST_TEST_CASE(&test_sdp_offer_4), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE(&test_sdp_offer_3), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE(&test_sdp_offer_2), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE(&test_sdp_offer_1), 0, /* timeout */ 15000);


  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_1), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_2), 0, /* timeout */ 15000);

  // This should fail as the answerer is configured as crypto (no agnostic) and the offerer is not crypto
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_3), 0, /* timeout */ 15000);

  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_4), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_5), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_6), 0, /* timeout */ 15000);

  // This should fail as the answerer is configured as crypto (no agnostic) and the offerer is non crypto
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_7), 0, /* timeout */ 15000);

  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_8), 0, /* timeout */ 15000);

  // This should fail as the answerer is configured as non crypto (no agnostic) and the offerer is crypto
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_9), 0, /* timeout */ 15000);

  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_10), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_11), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_12), 0, /* timeout */ 15000);

  // Odd case, it should work if answerer could support more than 1 audio media and 1 video media
  // The problem is that there is some "bug" on RtpEndpoint. The problem is that
  // the offer presents 2 audio and 2 video medias, but the answere an only support 1 audio and 1 video media
  // If the two first medias (audio and video) are supported by the answerer, the connection should process ok
  // But if two first medias are not supporte by the answerer, they are rejected, but the two following are outside the boundaries of
  // number of medias accepted and are also rejected
  // RElated: bug in kmssdpagent.c (1817), if an offer is answered with more medias than offered, reaches this point that kills the process
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_13), 0, /* timeout */ 15000);

  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_13_b), 0, /* timeout */ 15000);

  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_13_c), 0, /* timeout */ 15000);

  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_14), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_15), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_15_b), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_15_c), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &srtp_agnostic_case_16), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &test_error_on_answer_without_one_media), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &test_group_call_crash), 0, /* timeout */ 15000);
  test->add (BOOST_TEST_CASE ( &test_same_port_crash), 0, /* timeout */ 15000);
  return test;
}
