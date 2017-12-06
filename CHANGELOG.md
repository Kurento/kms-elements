# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [6.6.3] - 2017-08-10

### Changed
- Prevent frames from building up in the buffer if the CPU falls behind, by @kc7bfi (David Robison).

## [6.6.2] - 2017-07-24

### Added
- REMB: Add "COMEDIA"/automatic port discovery. [TODO Link to documentation].
- Allow AppSrc pipeline DOT diagrams to be returned, by @kc7bfi (David Robison).
- Eat all bus messages from 'queue2', by @kc7bfi (David Robison).

### Changed
- Old ChangeLog.md moved to the new format in this CHANGELOG.md file.
- CMake: Full review of all CMakeLists.txt files to tidy up and homogenize code style and compiler flags.
- CMake: Position Independent Code flags ("-fPIC") were scattered around projects, and are now removed. Instead, the more CMake-idiomatic variable "CMAKE_POSITION_INDEPENDENT_CODE" is used.
- CMake: All projects now compile with "[-std=c11|-std=c++11] -Wall -Werror -pthread".
- CMake: Debug builds now compile with "-g -O0" (while default CMake used "-O1" for Debug builds).
- CMake: include() and import() commands were moved to the code areas where they are actually required.
- CMake: Allow local inclusions in FindKmsWebRtcEndpointLib.
- Improve log messages when STUN or TURN config is missing.

### Fixed
- Bugfix: Out of bound access on SDP medias.
- Use format macros to fix compiler errors on 32bit systems, by @fancycode (Joachim Bauch).
- Debian: Use either "ffmpeg" or "libav" as dependencies (which adds compatibility with Ubuntu Xenial or Trusty, respectively).
- *FIXME* Disable failing IPv6 test.
- Workaround buggy libnice Foundation strings.
- Fix missing header in "HttpEndPointServer.hpp".

## [6.6.1] - 2016-09-30

### Changed
- Simplify recorderEndpoint internal pipeline state changes.
- Improve compilation process

### Fixed
- RecorderEndpoint: Fix media dead lock in avmuxer.
- RecorderEndpoint: Do not allow recording again once stop state is reached as this will erase previous recording.
- PlayerEndpoint: Set valid PTS and DTS values for pushed buffers.

## [6.6.0] - 2016-09-09

### Added
- WebRctEndpoint: ECDSA certificate support.

### Changed
- Improved documentation.

### Fixed
- PlayerEndpoint: Fix PTS assignment.
- RecorderEndpoint: Fix buffer leaks.

## [6.5.0] - 2016-05-30

### Added
- WebRctEndpoint: Add information about ice candidates pair selected.

### Changed
- Changed license to Apache 2.0.
- Updated documentation.

### Fixed
- WebRtcEndpoint: Fix memory leaks on candidates management.
- WebRtcEndpoint: Fix fingerprint generation when certificate is buldled with the key.
- WebRtcEndpoint: Fix bugs when using a custom "pem" file for DTLS.
- RecorderEndpoint: Fix state management.
- RecorderEndpoint: Add StopAndWait method.

### Deprecated
- Changed some event/methods names and deprecated old ones (which will be removed on the next major release).

## [6.4.0] - 2016-02-24

### Added
- RecorderEndpoint: Calculate end-to-end latency stats.
- PlayerEndpoint: Calculate end-to-end latency stats.

### Changed
- WebRtcEndpoint: Update libnice library to 0.1.13.1. TURN is working again now that libnice is updated.

### Fixed
- WebRtcEndpoint: minor issues.
- RecorderEndpoint: Fix problem when recording to HTTP, now MP4 is buffered using and fast start and Webm is recorded as live (no seekable without post-processing).

## [6.3.1] - 2016-01-29

### Fixed
- WebRtcEndpoint: Fix problem with codec names written in lower/upper case.
- PlayerEndpoint: Fix problem in pause introduced in previous release.
- WebRtcEndpoint: Parse candidates present in original offer correctly.
- RecorderEndpoint: Reduce log level for some messages that are not errors.

## [6.3.0] - 2019-01-19

### Added
- RtpEndpoint: Add event to notify when a SRTP key is about to expire.
- PlayerEndpoint: Add seek capability.
- RtpEndpoint, WebRtcEndpoint: Add support and tests for IPv6.

### Changed
- WebRtcEndpoint: Do not use TURN configuration until bug in libnice is fixed; TURN in clients (browsers) can still be used, but KMS will not generate relay candidates.

### Fixed
- RecorderEndpoint: Fix many problems that appeared with the last GStreamer update.
- WebRtcEndpoint: Fix minor problems with datachannels.
- WebRtcEndpoint: Fix problem with chrome 48 candidates.

## 6.2.0 - 2015-11-25

### Added
- RtpEndpoint: Add SDES encryption support.
- WebRtcEndpoint: Report possible error on candidate handling.

### Changed
- RtpEndpoint now inherits from BaseRtpEndpoint.
- WebRtcEndpoint uses BaseRtpEndpoint configuration for port ranges.
- RtpEndpoint uses BaseRtpEndpoint configuration for port ranges.
- RecorderEndpoint: Internal redesign simplifying internal pipeline.

### Fixed
- RecorderEndpoint: Fix problems with negative timestamps that produced empty videos.
- RecorderEndpoint: Fix negotiation problems with MP4 files. Now format changes are not allowed.
- PlayerEndpoint: set correct timestamps when source does not provide them properly.
- Composite: Fix bugs simplifying internal design.

[6.6.3]: https://github.com/Kurento/kms-elements/compare/6.6.2...6.6.3
[6.6.2]: https://github.com/Kurento/kms-elements/compare/6.6.1...6.6.2
[6.6.1]: https://github.com/Kurento/kms-elements/compare/6.6.0...6.6.1
[6.6.0]: https://github.com/Kurento/kms-elements/compare/6.5.0...6.6.0
[6.5.0]: https://github.com/Kurento/kms-elements/compare/6.4.0...6.5.0
[6.4.0]: https://github.com/Kurento/kms-elements/compare/6.3.1...6.4.0
[6.3.1]: https://github.com/Kurento/kms-elements/compare/6.3.0...6.3.1
[6.3.0]: https://github.com/Kurento/kms-elements/compare/6.2.0...6.3.0
