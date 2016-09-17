set(PROJECT ESP)

project(${PROJECT})
cmake_minimum_required(VERSION 3.0)

# =============================================================
#   Build options
# =============================================================
option(BUILD_HEADLESS "Build a headless version" OFF)

if(BUILD_HEADLESS)
  add_definitions(-DHEADLESS)
endif()

# =============================================================
#   openFrameworks
# =============================================================
set(OF_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third-party/openFrameworks)
set(openFrameworks_DIR ${CMAKE_CURRENT_SOURCE_DIR})
find_package(openFrameworks REQUIRED)

# =============================================================
#   Add-ons
# =============================================================
set(ADDONS_PATH ${OF_ROOT}/addons)

set(OFX_DATGUI_SRC
  ${ADDONS_PATH}/ofxDatGui/src/core/ofxDatGuiComponent.cpp
  ${ADDONS_PATH}/ofxDatGui/src/libs/ofxSmartFont/ofxSmartFont.cpp
  ${ADDONS_PATH}/ofxDatGui/src/ofxDatGui.cpp
  )

set(OFX_GRT_SRC
  ${ADDONS_PATH}/ofxGrt/src/ofxGrtTimeseriesPlot.cpp
  )

set(OFX_OSC_SRC
  ${ADDONS_PATH}/ofxOsc/src/ofxOscBundle.cpp
  ${ADDONS_PATH}/ofxOsc/src/ofxOscMessage.cpp
  ${ADDONS_PATH}/ofxOsc/src/ofxOscParameterSync.cpp
  ${ADDONS_PATH}/ofxOsc/src/ofxOscReceiver.cpp
  ${ADDONS_PATH}/ofxOsc/src/ofxOscSender.cpp
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/ip/IpEndpointName.cpp
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/ip/posix/NetworkingUtils.cpp
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/ip/posix/UdpSocket.cpp
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/osc/OscOutboundPacketStream.cpp
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/osc/OscPrintReceivedElements.cpp
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/osc/OscReceivedElements.cpp
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/osc/OscTypes.cpp
  )

set(OFX_NET_SRC
  ${ADDONS_PATH}/ofxNetwork/src/ofxTCPClient.cpp
  ${ADDONS_PATH}/ofxNetwork/src/ofxTCPManager.cpp
  ${ADDONS_PATH}/ofxNetwork/src/ofxTCPServer.cpp
  ${ADDONS_PATH}/ofxNetwork/src/ofxUDPManager.cpp
  )

set(OFX_PARAGRAPH_SRC
  ${ADDONS_PATH}/ofxParagraph/src/ofxParagraph.cpp
  )

list(APPEND ADDONS_SRC
  ${OFX_DATGUI_SRC}
  ${OFX_GRT_SRC}
  ${OFX_PARAGRAPH_SRC}
  ${OFX_NET_SRC}
  ${OFX_OSC_SRC}
  )

list(APPEND ADDONS_INCLUDE_PATH
  ${ADDONS_PATH}/ofxDatGui/src
  ${ADDONS_PATH}/ofxDatGui/src/core
  ${ADDONS_PATH}/ofxDatGui/src/components
  ${ADDONS_PATH}/ofxDatGui/src/themes
  ${ADDONS_PATH}/ofxDatGui/src/libs/ofxSmartFont
  ${ADDONS_PATH}/ofxGrt/src
  ${ADDONS_PATH}/ofxParagraph/src
  ${ADDONS_PATH}/ofxNetwork/src
  ${ADDONS_PATH}/ofxOsc/src
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/osc
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/
  ${ADDONS_PATH}/ofxOsc/libs/oscpack/src/ip
  )

# =============================================================
#   ESP sources
# =============================================================
set(ESP_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Xcode/ESP)
set(ESP_SRC
  ${ESP_PATH}/src/Filter.cpp
  ${ESP_PATH}/src/MFCC.cpp
  ${ESP_PATH}/src/ThresholdDetection.cpp
  ${ESP_PATH}/src/calibrator.cpp
  ${ESP_PATH}/src/iostream.cpp
  ${ESP_PATH}/src/istream.cpp
  ${ESP_PATH}/src/ofApp.cpp
  ${ESP_PATH}/src/ofConsoleFileLoggerChannel.cpp
  ${ESP_PATH}/src/ofYesNoDialog.cpp
  ${ESP_PATH}/src/ostream.cpp
  ${ESP_PATH}/src/plotter.cpp
  ${ESP_PATH}/src/training.cpp
  ${ESP_PATH}/src/training-data-manager.cpp
  ${ESP_PATH}/src/tuneable.cpp
  ${ESP_PATH}/src/user.cpp
)

set(ESP_MAIN ${ESP_PATH}/src/main.cpp)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(OBJECTIVE_CPP_SRC ${ESP_PATH}/src/ofYesNoDialog.cpp)
  set_source_files_properties(${OBJECTIVE_CPP_SRC} PROPERTIES COMPILE_FLAGS "-x objective-c++")
  set(GRT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/grt/)
  set(GRT_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/Xcode/ESP/libgrt.dylib)
  set(SYS_LIBS "-lobjc")
else()  # Linux
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

  find_package(Grt REQUIRED)
  if(GRT_FOUND)
    message("-- GRT found")
  else(GRT_FOUND)
    ## If there is no GRT installed on the system, we use Xcode/ESP/libgrt.a
    message("-- GRT not installed, using Xcode/ESP/libgrt.a")
    set(GRT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third-party/grt/)
    set(GRT_LIBRARY "-L${CMAKE_CURRENT_SOURCE_DIR}/Xcode/ESP/ -lgrt")
  endif(GRT_FOUND)
  set(SYS_LIBS "-L/usr/local/lib -lblas")
endif()


# =============================================================
#   Create library and executables
# =============================================================
add_executable(${PROJECT} ${ADDONS_SRC} ${ESP_SRC} ${ESP_MAIN})

target_include_directories(${PROJECT} PUBLIC
  ${openFrameworks_INCLUDES}
  ${ADDONS_INCLUDE_PATH}
  ${ESP_PATH}/src
  ${GRT_INCLUDE_DIR}
  )

target_link_libraries(${PROJECT} PUBLIC
  ${openFrameworks_LIBRARIES}
  ${ADDONS_LIBS}
  ${GRT_LIBRARY}
  ${SYS_LIBS}
  )

# =============================================================
#   Additional Flags (supress warnings)
# =============================================================
set_target_properties(${PROJECT} PROPERTIES COMPILE_FLAGS
  "-Wno-unused-value -Wno-deprecated-declarations")

set_source_files_properties(
  ${ESP_PATH}/src/ostream.cpp
  PROPERTIES
  COMPILE_FLAGS
  "-Wno-deprecated-declarations"
  )

# =============================================================
#   Package and copy dylib/assets (OS dependent)
# =============================================================
set_target_properties(${PROJECT} PROPERTIES
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_INFO_PLIST ${ESP_PATH}/Info.plist
  )

set(OFX_ASSETS ${ADDONS_PATH}/ofxDatGui/ofxbraitsch)
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  message( STATUS "" ${PROJECT}.app/Contents)
  FILE(COPY ${fmodex_LIBRARY} DESTINATION ${PROJECT}.app/Contents/MacOS)
  FILE(COPY ${OFX_ASSETS} DESTINATION ${PROJECT}.app/Contents/Resources/data/)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  FILE(COPY ${OFX_ASSETS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

# =============================================================
#   Doxygen and documentations
# =============================================================
find_package(Doxygen)
if(DOXYGEN_FOUND)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} Doxyfile
    working_directory ${CMAKE_CURRENT_SOURCE_DIR}/Xcode/ESP
    comment "Generating API documentation with Doxygen" VERBATIM
    )
endif(DOXYGEN_FOUND)

# =============================================================
#   GoogleTest
#   Borrowed from github.com/dmonopoly/gtest-cmake-example
# =============================================================
if(test)
  add_subdirectory(third-party/googletest/googletest)
  enable_testing()

  set(ESP_TO_TEST_SRC
    ${ESP_PATH}/src/training-data-manager.cpp
    )

  set(TEST_SRC
    ${ESP_PATH}/src/training-data-manager-test.cpp
    )

  include_directories(
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}
    ${GRT_INCLUDE_DIR}
    )

  add_executable(runUnitTests ${ESP_TO_TEST_SRC} ${TEST_SRC})
  target_link_libraries(runUnitTests gtest gtest_main)
  ## Extra linking (mainly GRT)
  target_link_libraries(runUnitTests ${GRT_LIBRARY})

  add_custom_command(
    TARGET runUnitTests
    POST_BUILD COMMAND
    ${CMAKE_INSTALL_NAME_TOOL} -change
    "@executable_path/../Frameworks/libgrt.dylib"
    "@executable_path/../Xcode/ESP/libgrt.dylib"
    $<TARGET_FILE:runUnitTests>
    )

  add_test(esp-test runUnitTests)
endif()

# =============================================================
#  esp-cli
# =============================================================
option(WITH_CLI "Build command line program" OFF)
if(WITH_CLI)
  set(CLI esp-cli)

  set(CLI_SRC
    ${ESP_PATH}/src/Filter.cpp
    ${ESP_PATH}/src/MFCC.cpp
    ${ESP_PATH}/src/ThresholdDetection.cpp
    ${ESP_PATH}/src/training-data-manager.cpp
    )

  set(CLI_MAIN ${ESP_PATH}/src/cli.cpp)
  add_executable(${CLI} ${CLI_SRC} ${CLI_MAIN})

  target_include_directories(${CLI} PUBLIC
    ${ESP_PATH}/src
    ${GRT_INCLUDE_DIR}
    )

  target_link_libraries(${CLI} PUBLIC
    ${GRT_LIBRARY}
    ${SYS_LIBS}
    "-framework Accelerate"
    "-lpython2.7"
    )

  set_target_properties(${CLI} PROPERTIES COMPILE_FLAGS
    "-Wno-unused-value -Wno-deprecated-declarations")
endif()
