################################################################################

file(STRINGS "VERSION" VERSION)

if (${PLUGIN_TYPE} STREQUAL "Instrument")
  set(PLUGIN_NAME "Automate-Instrument")
  set(BUNDLE_ID "com.chromaaudio.automate-instrument")
  set(PLUGIN_CODE Autf)
  set(VST3_PLUGIN_TYPE "Instrument")
  set(AU_PLUGIN_TYPE "kAudioUnitType_MusicDevice")
else()
  set(PLUGIN_NAME "Automate-FX")
  set(BUNDLE_ID "com.chromaaudio.automate-fx")
  set(PLUGIN_CODE Auti)
  set(VST3_PLUGIN_TYPE "Fx")
  set(AU_PLUGIN_TYPE "kAudioUnitType_Effect")
endif()

set(PLUGIN_VERSION ${VERSION})
set(PLUGIN_MANUFACTURER_CODE Chrm)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")

################################################################################

cmake_minimum_required(VERSION 3.22)

set(FORMATS "VST3;AU;Standalone" CACHE STRING "Plugin formats for JUCE to build. Valid formats: VST AU AUv3 Standalone")
message("-- Building formats: ${FORMATS}")

project(${PLUGIN_NAME} VERSION ${PLUGIN_VERSION} LANGUAGES C CXX)

add_subdirectory(extern/JUCE)

juce_add_plugin(${PLUGIN_NAME}
  COMPANY_NAME "Chroma Audio"
  BUNDLE_ID ${BUNDLE_ID} 
  PLUGIN_MANUFACTURER_CODE ${PLUGIN_MANUFACTURER_CODE}
  PLUGIN_CODE ${PLUGIN_CODE}
  FORMATS ${FORMATS}
  PRODUCT_NAME ${PLUGIN_NAME}
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE 
  VST3_CATEGORIES ${VST3_PLUGIN_TYPE}
  AU_MAIN_TYPE ${AU_PLUGIN_TYPE}
  MICROPHONE_PERMISSION_ENABLED FALSE 
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE 
  IS_MIDI_EFFECT FALSE)

juce_generate_juce_header(${PLUGIN_NAME})

juce_add_binary_data(Assets SOURCES
  assets/sofia_pro_light.otf
  assets/sofia_pro_regular.otf
  assets/sofia_pro_medium.otf)

target_sources(${PLUGIN_NAME} PRIVATE src/juce_build.cpp)

target_compile_definitions(${PLUGIN_NAME} PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_PLUGINHOST_VST3=1
  JUCE_PLUGINHOST_AU=1)

target_link_libraries(${PLUGIN_NAME} PRIVATE
  Assets
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags)
