cmake_minimum_required(VERSION 3.22)

# macOS : binaire universel (Apple Silicon + Intel), macOS 10.13 minimum (avant project())
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

project(SpaceSonar209 VERSION 1.0.6)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.15
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(SpaceSonarAssets SOURCES
  logo/NineHandsLogoBlack.svg
  fonts/Poppins-Regular.ttf fonts/Poppins-Medium.ttf fonts/Poppins-Bold.ttf fonts/Poppins-Light.ttf
  assets/background.jpg
  assets/knob_chrome_shadow.png assets/knob_chrome_crown.png assets/knob_chrome_cap.png assets/knob_chrome_marker.png assets/knob_chrome_highlight.png
  assets/knob_selector_marker.png
  assets/knob_black_shadow.png assets/knob_black_cap.png assets/knob_black_marker.png assets/knob_black_highlight.png
  assets/toggle_strip.png assets/led_red_off.png assets/led_red_on.png assets/slide_button.png)

juce_add_plugin(SpaceSonar209
  VERSION ${PROJECT_VERSION}
  COMPANY_NAME "NINE HANDS | DSP"
  PLUGIN_MANUFACTURER_CODE Wiga
  PLUGIN_CODE WgS9
  FORMATS VST3
  VST3_CATEGORIES Fx Delay Reverb
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  PRODUCT_NAME "Space Sonar 209"
  BUNDLE_ID "com.ninehandsdsp.spacesonar209"
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SpaceSonar209 PRIVATE PluginProcessor.cpp PluginEditor.cpp)

# verrou de taille de la fenetre du DAW (jamais sous 60 %, jamais au-dela de 150 %)
if(APPLE)
  target_sources(SpaceSonar209 PRIVATE WindowMinSize.mm)
  target_link_libraries(SpaceSonar209 PRIVATE "-framework Cocoa")
else()
  target_sources(SpaceSonar209 PRIVATE WindowMinSize.cpp)
endif()
if(WIN32)
  target_link_libraries(SpaceSonar209 PRIVATE comctl32 user32)
endif()

target_compile_definitions(SpaceSonar209 PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SpaceSonar209
  PRIVATE
    SpaceSonarAssets
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
