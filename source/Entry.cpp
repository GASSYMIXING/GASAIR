#include "Gasair.h"
#include "version.h"
#include "public.sdk/source/main/pluginfactory.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace Gasair;
BEGIN_FACTORY_DEF("Gassymixing","","")
DEF_CLASS2(INLINE_UID_FROM_FUID(ProcessorID),PClassInfo::kManyInstances,kVstAudioEffectClass,"GASAIR",Vst::kDistributable,"Fx|EQ",FULL_VERSION_STR,kVstVersionString,Processor::createInstance)
DEF_CLASS2(INLINE_UID_FROM_FUID(ControllerID),PClassInfo::kManyInstances,kVstComponentControllerClass,"GASAIR Controller",0,"",FULL_VERSION_STR,kVstVersionString,Controller::createInstance)
END_FACTORY
