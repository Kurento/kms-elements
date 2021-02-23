cmake_minimum_required(VERSION 2.8)


set(CUSTOM_PREFIX "kurento")
set(INCLUDE_PREFIX "${CMAKE_INSTALL_INCLUDEDIR}/${CUSTOM_PREFIX}/rtpendpoint")


set(KMS_RTPENDPOINT_SOURCES
  kmsrtpbaseconnection.c
  kmsrtpconnection.c
  kmssrtpconnection.c
  kmsrtpsession.c
  kmssrtpsession.c
  kmsrtpendpoint.c
  kmssocketutils.c
  kmsrandom.c
)

set(KMS_RTPENDPOINT_HEADERS
  kmsrtpbaseconnection.h
  kmsrtpconnection.h
  kmssrtpconnection.h
  kmsrtpsession.h
  kmssrtpsession.h
  kmsrtpendpoint.h
  kmssocketutils.h
)

set(ENUM_HEADERS
  kmsrtpsdescryptosuite.h
)

list(APPEND KMS_RTPENDPOINT_HEADERS ${ENUM_HEADERS})
add_glib_enumtypes(KMS_RTPENDPOINT_SOURCES KMS_RTPENDPOINT_HEADERS kms-rtp-enumtypes KMS ${ENUM_HEADERS})

add_library(rtpendpoint SHARED ${KMS_RTPENDPOINT_SOURCES} ${KMS_RTPENDPOINT_HEADERS})
if(SANITIZERS_ENABLED)
  add_sanitizers(rtpendpoint)
endif()

add_dependencies(rtpendpoint rtcpdemux)

set_property(TARGET rtpendpoint
  PROPERTY INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/../../..
    ${KmsGstCommons_INCLUDE_DIRS}
    ${gstreamer-1.5_INCLUDE_DIRS}
)

target_link_libraries(rtpendpoint
  ${KmsGstCommons_LIBRARIES}
  ${gstreamer-1.5_LIBRARIES}
  ${gstreamer-base-1.5_LIBRARIES}
  ${gstreamer-sdp-1.5_LIBRARIES}
  ${gstreamer-pbutils-1.5_LIBRARIES}
  ${nice_LIBRARIES}
)

set_target_properties(rtpendpoint PROPERTIES PUBLIC_HEADER "${KMS_RTPENDPOINT_HEADERS}")
set_target_properties(rtpendpoint PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

install(
  TARGETS rtpendpoint
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${INCLUDE_PREFIX}
)
