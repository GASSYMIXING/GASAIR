#include "Gasair.h"
#include "base/source/fstreamer.h"
#include <cstring>
namespace Gasair {
class ResizableEditor final : public VSTGUI::VST3Editor {
public:
    ResizableEditor(Controller* controller):VST3Editor(controller,"GASAIR","gasair.uidesc") {
        setInitialSize();
    }
    tresult PLUGIN_API getSize(ViewRect* size) override {
        if(!size)return kInvalidArgument;
        if(!opened)setInitialSize();
        return VST3Editor::getSize(size);
    }
    bool PLUGIN_API open(void* parent,const VSTGUI::PlatformType& type) override {
        setInitialSize();
        const bool result=VST3Editor::open(parent,type);
        opened=result;return result;
    }
    void PLUGIN_API close() override {opened=false;VST3Editor::close();}
    tresult PLUGIN_API checkSizeConstraint(ViewRect* rect) override {
        if(!rect)return kInvalidArgument;
        const auto width=std::clamp<double>(rect->getWidth(),768.*getContentScaleFactor(),2304.*getContentScaleFactor());
        const int32 units=static_cast<int32>(std::round(width/3.));
        rect->right=rect->left+units*3;
        rect->bottom=rect->top+units*2;
        return kResultTrue;
    }
    tresult PLUGIN_API onSize(ViewRect* rect) override {
        if(!rect)return kInvalidArgument;
        if(applyingSize)return VST3Editor::onSize(rect);
        // Use the size the host actually accepted, never a cached menu factor.
        // Suppress a recursive host resize while updating the content transform.
        applyingSize=true;
        const double scale=std::min(rect->getWidth()/1536.,rect->getHeight()/1024.);
        if(scale>0.)setZoomFactor(scale/getContentScaleFactor());
        const auto result=VST3Editor::onSize(rect);
        applyingSize=false;
        return result;
    }
    bool beforeSizeChange(const VSTGUI::CRect& next,const VSTGUI::CRect& previous) override {
        return applyingSize?true:VST3Editor::beforeSizeChange(next,previous);
    }
private:
    void setInitialSize(){
        // VST3Editor reads the unscaled template size into its host rectangle.
        // Setting zoom alone before a frame exists does not update that rectangle.
        zoomFactor=.5;
        rect.left=rect.top=0;
        rect.right=static_cast<int32>(std::round(768.*getContentScaleFactor()));
        rect.bottom=static_cast<int32>(std::round(512.*getContentScaleFactor()));
        nonEditRect=VSTGUI::CRect();
    }
    bool opened=false;
    bool applyingSize=false;
};
tresult PLUGIN_API Controller::initialize(FUnknown* context){
    const auto result=EditControllerEx1::initialize(context);if(result!=kResultOk)return result;
    auto add=[&](const TChar* label,ParamID id,const TChar* units,double min,double max,double def,int precision){
        auto* p=new RangeParameter(label,id,units,min,max,def);p->setPrecision(precision);parameters.addParameter(p);
    };
    add(STR16("Mid Air"),Mid,STR16("%"),0.,100.,Defaults[Mid]*100.,0);
    add(STR16("High Air"),High,STR16("%"),0.,100.,Defaults[High]*100.,0);
    add(STR16("Trim"),Trim,STR16("dB"),-12.,12.,0.,1);
    parameters.addParameter(STR16("Bypass"),nullptr,1,0.,ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass,Bypass);
    parameters.addParameter(STR16("Link"),nullptr,1,0.,ParameterInfo::kCanAutomate,Link);
    parameters.addParameter(STR16("Output L"),nullptr,0,0.,ParameterInfo::kIsReadOnly,MeterL);
    parameters.addParameter(STR16("Output R"),nullptr,0,0.,ParameterInfo::kIsReadOnly,MeterR);
    return kResultOk;
}
tresult PLUGIN_API Controller::setComponentState(IBStream* state){
    std::array<double,ParamCount> values{};if(!readState(state,values))return kResultFalse;
    for(ParamID p=0;p<ParamCount;++p)setParamNormalized(p,values[p]);return kResultOk;
}
tresult PLUGIN_API Controller::setState(IBStream* state){
    if(!state)return kInvalidArgument;IBStreamer s(state,kLittleEndian);double v;
    if(!s.readDouble(v)||!std::isfinite(v))return kResultFalse;zoom=.5;return kResultOk;
}
tresult PLUGIN_API Controller::getState(IBStream* state){if(!state)return kInvalidArgument;IBStreamer s(state,kLittleEndian);return s.writeDouble(.5)?kResultOk:kResultFalse;}
IPlugView* PLUGIN_API Controller::createView(FIDString name){
    if(!FIDStringsEqual(name,ViewType::kEditor))return nullptr;
    auto* editor=new ResizableEditor(this);
    zoom=.5;
    editor->setAllowedZoomFactors({});editor->setZoomFactor(.5);editor->setIdleRate(33);return editor;
}
VSTGUI::CView* Controller::createCustomView(VSTGUI::UTF8StringPtr name,const VSTGUI::UIAttributes&,const VSTGUI::IUIDescription*,VSTGUI::VST3Editor* editor){
    return std::strcmp(name,"GASAIRPanel")==0?createPanel(this,editor):nullptr;
}
}
