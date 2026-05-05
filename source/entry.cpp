#include "public.sdk/source/main/pluginfactory.h"
#include "pluginprocessor.h"
#include "plugincontroller.h"

#define stringPluginName "MONODUCK 1.01"

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
               "1.0.1",
               kVstVersionString,
               VSTVibe2::createProcessorInstance)

    DEF_CLASS2(INLINE_UID_FROM_FUID(VSTVibe2::kVSTVibe2ControllerUID),
               PClassInfo::kManyInstances,
               kVstComponentControllerClass,
               stringPluginName " Controller",
               0,
               "",
               "1.0.1",
               kVstVersionString,
               VSTVibe2::createControllerInstance)

END_FACTORY
