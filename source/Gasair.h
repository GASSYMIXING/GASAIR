#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace Gasair {
using namespace Steinberg;
using namespace Steinberg::Vst;
inline const FUID ProcessorID(0xD15F787A,0x457B4EC2,0xA64FB321,0x19AC37E8);
inline const FUID ControllerID(0x4F0239CE,0x09D24674,0xBD5C1069,0xF72A8631);
enum : ParamID { Mid=0, High, Trim, Bypass, Link, ParamCount, MeterL=100, MeterR=101 };
inline constexpr std::array<double,ParamCount> Defaults{0.,0.,.50,0.,0.};
inline double normalized(double v) { return std::isfinite(v)?std::clamp(v,0.,1.):0.; }
bool readState(IBStream*,std::array<double,ParamCount>&);

class Processor final : public AudioEffect {
public:
    Processor();
    static FUnknown* createInstance(void*) {return static_cast<IAudioProcessor*>(new Processor);}
    tresult PLUGIN_API initialize(FUnknown*) override;
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32,SpeakerArrangement*,int32) override;
    tresult PLUGIN_API canProcessSampleSize(int32) override;
    tresult PLUGIN_API setupProcessing(ProcessSetup&) override;
    tresult PLUGIN_API setActive(TBool) override;
    tresult PLUGIN_API setProcessing(TBool) override;
    tresult PLUGIN_API process(ProcessData&) override;
    tresult PLUGIN_API setState(IBStream*) override;
    tresult PLUGIN_API getState(IBStream*) override;
    uint32 PLUGIN_API getTailSamples() override {return static_cast<uint32>(processSetup.sampleRate*.5);}
private:
    void updateTargets();
    void reset();
    template<class Sample> tresult render(ProcessData&,Sample**,Sample**);
    std::array<double,ParamCount> params=Defaults;
    std::array<double,4> target{},smooth{};
    std::array<std::array<double,2>,2> lowState{};
    std::array<double,2> envelope{},peak{},clipHold{};
    double gMid=.1,gHigh=.3,attack=.99,release=.999,parameterRate=.999,bypassRate=.99,meterRelease=.999;
};

class Controller final : public EditControllerEx1, public VSTGUI::VST3EditorDelegate {
public:
    static FUnknown* createInstance(void*) {return static_cast<IEditController*>(new Controller);}
    tresult PLUGIN_API initialize(FUnknown*) override;
    tresult PLUGIN_API setComponentState(IBStream*) override;
    tresult PLUGIN_API setState(IBStream*) override;
    tresult PLUGIN_API getState(IBStream*) override;
    IPlugView* PLUGIN_API createView(FIDString) override;
    VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr,const VSTGUI::UIAttributes&,const VSTGUI::IUIDescription*,VSTGUI::VST3Editor*) override;
    void onZoomChanged(VSTGUI::VST3Editor*,double value) override {zoom=std::clamp(value,.5,1.5);}
    double zoom=.5;
};
VSTGUI::CView* createPanel(Controller*,VSTGUI::VST3Editor*);
}
