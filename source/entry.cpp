#include "public.sdk/source/main/pluginfactory.h"
#include "pluginprocessor.h"
#include "plugincontroller.h"

#include "version.h"
#define stringPluginVersion MONODUCK_VERSION
#define stringPluginName    MONODUCK_NAME

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace VSTVibe2 {
extern FUnknown* createProcessorInstance(void*);
extern FUnknown* createControllerInstance(void*);
}

BEGIN_FACTORY_DEF("MONODUCK", "https://drsuds.com", "mailto:drsuds@gmail.com")

    DEF_CLASS2(INLINE_UID_FROM_FUID(VSTVibe2::kVSTVibe2ProcessorUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               stringPluginName,
               Vst::kDistributable,
               "Instrument",
               stringPluginVersion,
               kVstVersionString,
               VSTVibe2::createProcessorInstance)

    DEF_CLASS2(INLINE_UID_FROM_FUID(VSTVibe2::kVSTVibe2ControllerUID),
               PClassInfo::kManyInstances,
               kVstComponentControllerClass,
               stringPluginName " Controller",
               0,
               "",
               stringPluginVersion,
               kVstVersionString,
               VSTVibe2::createControllerInstance)

END_FACTORY
