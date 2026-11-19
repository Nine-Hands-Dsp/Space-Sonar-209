# Outil hors plugin : compile le processeur + la fenetre de Space Sonar 209 sous Linux, verifie le moteur et exporte des captures PNG
cmake_minimum_required(VERSION 3.22)
project(SpaceSonarRender VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(${JUCE_DIR} JUCE)
set(W ${CMAKE_CURRENT_SOURCE_DIR}/../..)
file(STRINGS ${W}/CMakeLists.txt SS_PRJ REGEX "^project\\(SpaceSonar209 VERSION")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" SS_VER "${SS_PRJ}")
juce_add_binary_data(SpaceSonarAssets SOURCES ${W}/logo/NineHandsLogoBlack.svg
  ${W}/fonts/Poppins-Regular.ttf ${W}/fonts/Poppins-Medium.ttf ${W}/fonts/Poppins-Bold.ttf ${W}/fonts/Poppins-Light.ttf
  ${W}/assets/background.jpg
  ${W}/assets/knob_chrome_shadow.png ${W}/assets/knob_chrome_crown.png ${W}/assets/knob_chrome_cap.png ${W}/assets/knob_chrome_marker.png ${W}/assets/knob_chrome_highlight.png
  ${W}/assets/knob_selector_marker.png
  ${W}/assets/knob_black_shadow.png ${W}/assets/knob_black_cap.png ${W}/assets/knob_black_marker.png ${W}/assets/knob_black_highlight.png
  ${W}/assets/toggle_strip.png ${W}/assets/led_red_off.png ${W}/assets/led_red_on.png ${W}/assets/slide_button.png)
juce_add_console_app(SpaceSonarRender PRODUCT_NAME "SpaceSonarRender")
target_sources(SpaceSonarRender PRIVATE main.cpp ${W}/PluginProcessor.cpp ${W}/PluginEditor.cpp ${W}/WindowMinSize.cpp)
target_include_directories(SpaceSonarRender PRIVATE ${W})
target_compile_definitions(SpaceSonarRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_ALSA=0 JUCE_JACK=0
  JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0 JucePlugin_Name="Space Sonar 209" JucePlugin_VersionString="${SS_VER}"
  JucePlugin_Build_VST3=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(SpaceSonarRender PRIVATE SpaceSonarAssets juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
