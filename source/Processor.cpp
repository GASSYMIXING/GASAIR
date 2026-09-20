// GASAIR: independently implemented parallel dynamic high-frequency enhancer.
#include "Gasair.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include <xmmintrin.h>

namespace Gasair {
constexpr int32 StateMagic=0x47415341;
bool readState(IBStream* stream,std::array<double,ParamCount>& result) {
    if(!stream)return false;
    IBStreamer reader(stream,kLittleEndian);
    int32 magic=0,version=0;
    std::array<double,ParamCount> temporary{};
    if(!reader.readInt32(magic)||!reader.readInt32(version)||magic!=StateMagic||version!=1)return false;
    for(auto& v:temporary)if(!reader.readDouble(v)||!std::isfinite(v)||v<0.||v>1.)return false;
    result=temporary;return true;
}
Processor::Processor(){setControllerClass(ControllerID);updateTargets();smooth=target;}
tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    auto result=AudioEffect::initialize(context);if(result!=kResultOk)return result;
    addAudioInput(STR16("Stereo In"),SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"),SpeakerArr::kStereo);return kResultOk;
}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* in,int32 ni,SpeakerArrangement* out,int32 no) {
    if(ni!=1||no!=1||!in||!out||in[0]!=out[0]||(in[0]!=SpeakerArr::kMono&&in[0]!=SpeakerArr::kStereo))return kResultFalse;
    return AudioEffect::setBusArrangements(in,ni,out,no);
}
tresult PLUGIN_API Processor::canProcessSampleSize(int32 size){return size==kSample32||size==kSample64?kResultTrue:kResultFalse;}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
    if(!std::isfinite(setup.sampleRate)||setup.sampleRate<8000.||setup.sampleRate>768000.)return kInvalidArgument;
    if(canProcessSampleSize(setup.symbolicSampleSize)!=kResultTrue)return kResultFalse;
    const double fs=setup.sampleRate,pi=3.14159265358979323846;
    auto coefficient=[&](double hz){double g=std::tan(pi*std::min(hz,fs*.40)/fs);return g/(1.+g);};
    gMid=coefficient(2000.);gHigh=coefficient(8000.);
    attack=std::exp(-1./(fs*.0025));release=std::exp(-1./(fs*.090));
    parameterRate=std::exp(-1./(fs*.012));bypassRate=std::exp(-1./(fs*.004));
    meterRelease=std::exp(-1./(fs*.250));
    reset();return AudioEffect::setupProcessing(setup);
}
void Processor::updateTargets() {
    target[0]=std::pow(10.,9.*std::pow(params[Mid],1.25)/20.)-1.;
    target[1]=std::pow(10.,12.*std::pow(params[High],1.25)/20.)-1.;
    target[2]=std::pow(10.,(-12.+24.*params[Trim])/20.);
    target[3]=params[Bypass]>.5?0.:1.;
}
void Processor::reset(){lowState={};envelope={};peak={};clipHold={};updateTargets();smooth=target;}
tresult PLUGIN_API Processor::setActive(TBool active){if(active)reset();return AudioEffect::setActive(active);}
tresult PLUGIN_API Processor::setProcessing(TBool state){if(state)reset();return AudioEffect::setProcessing(state);}
tresult PLUGIN_API Processor::setState(IBStream* stream){if(!readState(stream,params))return kResultFalse;updateTargets();return kResultOk;}
tresult PLUGIN_API Processor::getState(IBStream* stream) {
    if(!stream)return kInvalidArgument;
    IBStreamer writer(stream,kLittleEndian);
    if(!writer.writeInt32(StateMagic)||!writer.writeInt32(1))return kResultFalse;
    for(double v:params)if(!writer.writeDouble(v))return kResultFalse;
    return kResultOk;
}

// Queue cursors stay on the stack. No allocation, locks, UI calls or messages
// are made by our sample loop. Every automation point is applied at its offset.
struct Cursor {
    IParamValueQueue* queue=nullptr; int32 index=0,offset=0; double value=0.;
    void advance(){if(!queue||index>=queue->getPointCount()||queue->getPoint(index++,offset,value)!=kResultTrue)queue=nullptr;}
};
template<class Sample> tresult Processor::render(ProcessData& data,Sample** input,Sample** output) {
    std::array<Cursor,ParamCount> changes{};
    if(data.inputParameterChanges)for(int32 q=0;q<data.inputParameterChanges->getParameterCount();++q){
        auto* queue=data.inputParameterChanges->getParameterData(q);
        if(queue&&queue->getParameterId()<ParamCount){auto& c=changes[queue->getParameterId()];c.queue=queue;c.advance();}
    }
    auto apply=[&](int32 sample){bool changed=false;for(size_t p=0;p<changes.size();++p){auto& c=changes[p];while(c.queue&&c.offset<=sample){params[p]=normalized(c.value);c.advance();changed=true;}}if(changed)updateTargets();};
    if(data.numSamples<=0){apply(0);return kResultOk;}
    const int channels=std::clamp(data.outputs[0].numChannels,0,2);
    if(!output)return kInvalidArgument;
    const unsigned oldCsr=_mm_getcsr();_mm_setcsr(oldCsr|0x8040u);
    uint64 silence=channels==2?3:1;
    std::array<double,2> blockPeak{};
    for(int32 s=0;s<data.numSamples;++s){
        apply(s);
        for(size_t p=0;p<smooth.size();++p){const double r=p==3?bypassRate:parameterRate;smooth[p]=target[p]+r*(smooth[p]-target[p]);if(std::abs(smooth[p]-target[p])<1e-10)smooth[p]=target[p];}
        double dry[2]{},mid[2]{},high[2]{},detect[2]{};
        for(int c=0;c<channels;++c){
            if(input&&c<data.inputs[0].numChannels&&input[c]&&!(data.inputs[0].silenceFlags&(uint64(1)<<c)))dry[c]=input[c][s];
            if(!std::isfinite(dry[c]))dry[c]=0.;
            auto lowpass=[&](int band,double g){double v=(dry[c]-lowState[c][band])*g;double y=v+lowState[c][band];lowState[c][band]=y+v;return y;};
            const double low=lowpass(0,gMid),upper=lowpass(1,gHigh);
            mid[c]=upper-low;high[c]=dry[c]-upper;
            detect[0]=std::max(detect[0],std::abs(mid[c]));detect[1]=std::max(detect[1],std::abs(high[c]));
        }
        double dynamic[2];
        for(int b=0;b<2;++b){
            const double r=detect[b]>envelope[b]?attack:release;
            envelope[b]=detect[b]+r*(envelope[b]-detect[b]);
            const double hot=envelope[b]/(b==0?.16:.08);
            // Linked soft compression in the parallel enhancement paths:
            // quieter detail receives more lift, strong treble receives less.
            dynamic[b]=1./std::sqrt(1.+hot*hot);
        }
        for(int c=0;c<channels;++c){
            double y=dry[c];
            if(smooth[3]!=0.){
                const double effected=(dry[c]+smooth[0]*dynamic[0]*mid[c]+smooth[1]*dynamic[1]*high[c])*smooth[2];
                y=smooth[3]==1.?effected:dry[c]+smooth[3]*(effected-dry[c]);
            }
            if(!std::isfinite(y))y=0.;
            if(output[c])output[c][s]=static_cast<Sample>(y);
            if(y!=0.)silence&=~(uint64(1)<<c);
            peak[c]=std::max(std::abs(y),peak[c]*meterRelease);
            if(std::abs(y)>=1.)clipHold[c]=processSetup.sampleRate;
            else clipHold[c]=std::max(0.,clipHold[c]-1.);
            blockPeak[c]=std::max(blockPeak[c],peak[c]);
        }
    }
    _mm_setcsr(oldCsr);
    data.outputs[0].silenceFlags=silence;
    if(channels==1){blockPeak[1]=blockPeak[0];clipHold[1]=clipHold[0];}
    if(data.outputParameterChanges)for(int c=0;c<2;++c){
        int32 index=0;auto* queue=data.outputParameterChanges->addParameterData(c==0?MeterL:MeterR,index);
        const double db=20.*std::log10(std::max(1e-12,blockPeak[c]));
        const double level=clipHold[c]>0.?1.:std::clamp((db+48.)/48.,0.,.999);
        if(queue)queue->addPoint(data.numSamples-1,level,index);
    }
    return kResultOk;
}
tresult PLUGIN_API Processor::process(ProcessData& data) {
    if(data.numSamples<0)return kInvalidArgument;
    if(data.numSamples==0||data.numInputs==0||data.numOutputs==0||!data.inputs||!data.outputs){
        if(data.inputParameterChanges)for(int32 i=0;i<data.inputParameterChanges->getParameterCount();++i){
            auto* q=data.inputParameterChanges->getParameterData(i);int32 offset=0;double value=0.;
            if(q&&q->getParameterId()<ParamCount&&q->getPointCount()>0&&q->getPoint(q->getPointCount()-1,offset,value)==kResultTrue)params[q->getParameterId()]=normalized(value);
        }updateTargets();return kResultOk;
    }
    if(data.inputs[0].numChannels!=data.outputs[0].numChannels||data.outputs[0].numChannels<1||data.outputs[0].numChannels>2)return kResultFalse;
    if(data.symbolicSampleSize==kSample32)return render(data,data.inputs[0].channelBuffers32,data.outputs[0].channelBuffers32);
    if(data.symbolicSampleSize==kSample64)return render(data,data.inputs[0].channelBuffers64,data.outputs[0].channelBuffers64);
    return kResultFalse;
}
}
