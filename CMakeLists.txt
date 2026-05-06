cmake_minimum_required(VERSION 3.15)
project(VSTVibe2 VERSION 1.0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default VST3 SDK path if not provided
if(NOT DEFINED VST3_SDK_PATH)
  set(VST3_SDK_PATH "C:/code/VST_SDK/vst3sdk" CACHE PATH "Steinberg VST3 SDK root path")
endif()

if(NOT EXISTS "${VST3_SDK_PATH}/public.sdk")
  message(FATAL_ERROR "VST3 SDK not found at ${VST3_SDK_PATH}. Set VST3_SDK_PATH to the correct location.")
endif()

message(STATUS "Using VST3 SDK: ${VST3_SDK_PATH}")

set(VST3_SDK_ROOT ${VST3_SDK_PATH})

# Add VST3 SDK base libraries
add_library(vst3_base STATIC
  ${VST3_SDK_ROOT}/base/source/fobject.cpp
  ${VST3_SDK_ROOT}/base/source/fbuffer.cpp
  ${VST3_SDK_ROOT}/base/source/fstreamer.cpp
  ${VST3_SDK_ROOT}/base/source/fstring.cpp
  ${VST3_SDK_ROOT}/base/source/baseiids.cpp
  ${VST3_SDK_ROOT}/base/source/updatehandler.cpp
  ${VST3_SDK_ROOT}/base/source/timer.cpp
  ${VST3_SDK_ROOT}/pluginterfaces/base/funknown.cpp
  ${VST3_SDK_ROOT}/pluginterfaces/base/ustring.cpp
  ${VST3_SDK_ROOT}/pluginterfaces/base/coreiids.cpp
)
target_include_directories(vst3_base PRIVATE ${VST3_SDK_ROOT})

# Add VST3 SDK plugin framework
add_library(vst3_sdk STATIC
  ${VST3_SDK_ROOT}/public.sdk/source/common/commoniids.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/common/pluginview.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/vstcomponentbase.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/vstcomponent.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/vstaudioeffect.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/vstbus.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/vstparameters.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/vsteditcontroller.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/vstinitiids.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/main/pluginfactory.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp
  ${VST3_SDK_ROOT}/base/thread/source/flock.cpp
)
target_include_directories(vst3_sdk PRIVATE ${VST3_SDK_ROOT})
target_link_libraries(vst3_sdk PRIVATE vst3_base)

add_library(VSTVibe2 SHARED
  source/pluginprocessor.cpp
  source/plugincontroller.cpp
  source/pluginview.cpp
  source/entry.cpp
)

target_compile_definitions(VSTVibe2 PRIVATE NOMINMAX)
target_include_directories(VSTVibe2 PRIVATE ${VST3_SDK_ROOT})
target_link_libraries(VSTVibe2 PRIVATE vst3_sdk vst3_base gdiplus.lib)

if(MSVC)
  target_compile_options(VSTVibe2 PRIVATE /EHsc /W4)
  set_target_properties(VSTVibe2 PROPERTIES LINK_FLAGS "/EXPORT:GetPluginFactory")
endif()

set_target_properties(VSTVibe2 PROPERTIES
  OUTPUT_NAME "MONODUCK"
  SUFFIX ".vst3"
  PREFIX ""
)
