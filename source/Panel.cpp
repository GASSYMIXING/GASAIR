// The approved artwork supplies the panel, machining, labels and foreground arm.
// Interactive parts are drawn at fixed panel coordinates; never rotate a context.
#include "Gasair.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/clinestyle.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cviewcontainer.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/controls/ctextedit.h"

#include <vector>
#include <cstdio>
#include <cstdlib>

namespace Gasair {
using namespace VSTGUI;
namespace {
using Bitmap=SharedPointer<CBitmap>;
constexpr double pi=3.14159265358979323846;
const CColor Pink(255,39,133);
struct Knob {double x,y,face,arc;ParamID tag;CRect readout;};
const std::array<Knob,3> Knobs{{
    {203,487,67,92,Mid,CRect(154,571,250,596)},
    {494,487,67,92,High,CRect(446,571,541,596)},
    {192,750,56,80,Trim,CRect(145,823,238,848)}
}};

class Readout final : public CTextEdit {
public:
    Readout(CRect r,IControlListener* listener,int32_t tag,CFontRef font):CTextEdit(r,listener,tag,"",nullptr,kNoFrame){
        setFont(font);setFontColor(Pink);setBackColor(CColor(26,26,28));setFrameColor(CColor(26,26,28));setTextInset(CPoint(0,0));
    }
    void draw(CDrawContext* context) override {
        context->setFillColor(CColor(26,26,28));context->drawRect(getViewSize(),kDrawFilled);
        context->setFont(getFont());context->setFontColor(Pink);
        context->drawString(getText().getString().c_str(),getViewSize(),kCenterText);setDirty(false);
    }
};

class Panel final : public CViewContainer, public IControlListener {
public:
    Panel(Controller* c,VST3Editor* e):CViewContainer(CRect(0,0,1536,1024)),ctl(c),editor(e),
        art(VSTGUI::owned(new CBitmap(CResourceDescription("gasair.png")))),
        numberFont(VSTGUI::owned(new CFontDesc("Bahnschrift",23,kBoldFace))),
        labelFont(VSTGUI::owned(new CFontDesc("Bahnschrift",27,kBoldFace))) {
        ctl->addRef();setTransparency(false);
        auto access=VSTGUI::owned(CBitmapPixelAccess::create(art,false));
        if(access&&access->getBitmapWidth()==1536&&access->getBitmapHeight()==1024){
            pixels.resize(1536*1024);
            for(int y=0;y<1024;++y)for(int x=0;x<1536;++x){access->setPosition(x,y);access->getColor(pixels[y*1536+x]);}
        }
        for(size_t i=0;i<Knobs.size();++i){
            const auto k=Knobs[i];const int extent=static_cast<int>(k.arc+24);
            surround[i]=bitmap(extent*2,extent+63,[&](int x,int y){
                const double dx=x-extent+.5,dy=y-extent+.5,r=std::hypot(dx,dy);
                CColor original=at(int(k.x+dx),int(k.y+dy));
                const double inner=k.face+15.,outer=i==2?100.:112.;
                if(r>inner&&r<outer&&dy<k.arc*.55&&k.y+dy>=(i==2?670:396)){
                    // Erase the baked arc and ticks with adjacent plain panel texture.
                    // The original metal rim and shadows stay untouched.
                    const auto paper=at((i==2?290:327)+x%7,int(k.y+dy));
                    return blend(original,paper,std::min({1.,r-inner,outer-r}));
                }
                return original;
            });
            const int size=static_cast<int>(k.face*2);
            faces[i]=bitmap(size,size,[&](int x,int y){
                const double dx=x+.5-k.face,dy=y+.5-k.face;
                int shade=int(30.+9.*(1.-y/double(size))+3.*std::exp(-((dx+20)*(dx+20)+(dy+24)*(dy+24))/1700.));
                return CColor(shade,shade+1,shade+2,uint8_t(255.*std::clamp(k.face-std::hypot(dx,dy),0.,1.)));
            });
            auto* text=new Readout(k.readout,this,k.tag,numberFont);
            text->setStringToValueFunction([this,tag=k.tag](UTF8StringPtr s,float& result,CTextEdit*){
                char* end=nullptr;double value=std::strtod(s,&end);
                if(end==s||!std::isfinite(value))return false;
                result=static_cast<float>(normalized(ctl->getParameterObject(tag)->toNormalized(value)));return true;
            });
            readouts[i]=text;addView(text);
        }
        for(int i=0;i<2;++i)for(int on=0;on<2;++on)switches[i][on]=makeSwitch(button(i),on!=0);
        previous.fill(-1.);refreshReadouts();setWantsIdle(true);
    }
    ~Panel() noexcept override {finish();ctl->release();}
    void drawBackgroundRect(CDrawContext* context,const CRect& update) override {
        context->saveGlobalState();context->setDrawMode(kAntiAliasing|kNonIntegralMode);
        art->draw(context,update,CPoint(update.left,update.top));
        for(size_t i=0;i<Knobs.size();++i)if(update.rectOverlap(knobBounds(i)))drawKnob(context,i,value(Knobs[i].tag));
        for(int i=0;i<2;++i){
            auto bounds=button(i);bounds.extend(8,8);if(!update.rectOverlap(bounds))continue;
            bool on=i==0?value(Bypass)<.5:value(Link)>.5;
            switches[i][on]->draw(context,bounds);
            const auto r=button(i);context->setFont(labelFont);context->setFontColor(CColor(255,250,252));
            context->drawString(i==0?(on?"ON":"OFF"):"LINK",r,kCenterText);
        }
        for(int c=0;c<2;++c){
            const int x=c==0?418:477;if(!update.rectOverlap(CRect(x-2,668,x+25,847)))continue;
            context->setFillColor(CColor(15,15,17));context->drawRect(CRect(x-1,669,x+24,845),kDrawFilled);
            const double level=value(c==0?MeterL:MeterR);const double db=level*48.-48.;
            for(int row=0;row<14;++row){
                const double threshold=-48.+row*(48./13.);const bool lit=level>0.&&db>=threshold;
                const int y=830-row*12;
                auto cell=CRect(x,y,x+22,y+10);
                if(lit){
                    // Reuse the approved luminous LED pixels for exact material matching.
                    art->draw(context,cell,CPoint(418,row>=8?699:774));
                    if(row==13&&level>=1.){context->setFillColor(CColor(255,54,77));context->drawRect(cell,kDrawFilled);}
                }else{
                    context->setFillColor(CColor(36,35,38));context->drawRect(cell,kDrawFilled);
                    context->setFillColor(CColor(45,43,46));context->drawRect(CRect(x+3,y+2,x+19,y+8),kDrawFilled);
                }
            }
        }
        if(update.rectOverlap(gripRect())){
            context->setFrameColor(CColor(196,190,188));context->setLineWidth(2.);
            for(int i=0;i<3;++i)context->drawLine(CPoint(1470+i*9,1005),CPoint(1493,982+i*9));
        }
        context->restoreGlobalState();
    }
    void onIdle() override {
        for(int p=0;p<ParamCount;++p){double v=value(p);if(v!=previous[p]){invalidate(p);previous[p]=v;}}
        for(int c=0;c<2;++c){double v=value(c==0?MeterL:MeterR);if(v!=previous[ParamCount+c]){int x=c==0?418:477;invalidRect(CRect(x-2,668,x+25,847));previous[ParamCount+c]=v;}}
        refreshReadouts();
    }
    CMouseEventResult onMouseDown(CPoint& point,const CButtonState& buttons) override {
        if(buttons&kRButton)return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
        if(!(buttons&kLButton))return kMouseEventNotHandled;
        if(point.x>=1520||point.y>=1008||gripRect().pointInside(point)){
            resizeAxis=point.x>=1520&&point.y<1008?1:(point.y>=1008&&point.x<1520?2:0);finish();resizing=true;resizeStart=editor->getZoomFactor();resizeOrigin=CPoint(point.x*resizeStart,point.y*resizeStart);return kMouseEventHandled;
        }
        for(int i=0;i<2;++i)if(button(i).pointInside(point)){change(i==0?Bypass:Link,value(i==0?Bypass:Link)>.5?0.:1.);return kMouseDownEventHandledButDontNeedMovedOrUpEvents;}
        const int tag=hit(point);if(tag<0)return CViewContainer::onMouseDown(point,buttons);
        if(buttons&kDoubleClick){change(tag,Defaults[tag]);return kMouseDownEventHandledButDontNeedMovedOrUpEvents;}
        start(tag);lastY=point.y;return kMouseEventHandled;
    }
    CMouseEventResult onMouseMoved(CPoint& point,const CButtonState& buttons) override {
        if(resizing){
            const double current=editor->getZoomFactor();
            const double dx=point.x*current-resizeOrigin.x,dy=point.y*current-resizeOrigin.y;
            const double next=std::clamp(resizeStart+(resizeAxis==1?dx/1536.:(resizeAxis==2?dy/1024.:(dx*1536.+dy*1024.)/(1536.*1536.+1024.*1024.))),.5,1.5);
            const double scale=getFrame()->getZoom()/current; const double width=3.*std::round(1536.*next*scale/3.); editor->requestResize(CPoint(width,width*2./3.));return kMouseEventHandled;
        }
        if(active<0)return kMouseEventNotHandled;
        setGesture(value(active)+(lastY-point.y)*((buttons&kShift)?.1:1.)/220.);lastY=point.y;return kMouseEventHandled;
    }
    CMouseEventResult onMouseUp(CPoint&,const CButtonState&) override {if(resizing){resizing=false;return kMouseEventHandled;}if(active<0)return kMouseEventNotHandled;finish();return kMouseEventHandled;}
    CMouseEventResult onMouseCancel() override {if(resizing){resizing=false;return kMouseEventHandled;}if(active<0)return kMouseEventNotHandled;finish();return kMouseEventHandled;}
    void onMouseWheelEvent(MouseWheelEvent& event) override {
        int tag=hit(event.mousePosition);if(tag<0)return;
        change(tag,value(tag)+event.deltaY*(event.modifiers.has(ModifierKey::Shift)?.001:.01));event.consumed=true;
    }
    void valueChanged(CControl* control) override {change(control->getTag(),control->getValue());}
private:
    static CColor blend(CColor a,CColor b,double t){t=std::clamp(t,0.,1.);return CColor(uint8_t(a.red+(b.red-a.red)*t),uint8_t(a.green+(b.green-a.green)*t),uint8_t(a.blue+(b.blue-a.blue)*t));}
    CColor at(int x,int y)const {return pixels.empty()?CColor(235,229,219):pixels[std::clamp(y,0,1023)*1536+std::clamp(x,0,1535)];}
    template<class F> Bitmap bitmap(int w,int h,F fn){
        auto b=VSTGUI::owned(new CBitmap(w,h));auto access=VSTGUI::owned(CBitmapPixelAccess::create(b,false));
        if(access)for(int y=0;y<h;++y)for(int x=0;x<w;++x){access->setPosition(x,y);access->setColor(fn(x,y));}return b;
    }
    Bitmap makeSwitch(CRect r,bool on){
        int w=int(r.getWidth()),h=int(r.getHeight());
        return bitmap(w+16,h+16,[&](int px,int py){
            double x=px-8.+.5,y=py-8.+.5;
            auto distance=[&](double xx,double yy){double qx=std::abs(xx-w*.5)-(w*.5-15.),qy=std::abs(yy-h*.5)-(h*.5-15.);return std::hypot(std::max(qx,0.),std::max(qy,0.))+std::min(std::max(qx,qy),0.)-15.;};
            double d=distance(x,y);
            // Rebuild both controls from the same material; no baked-on text or halos.
            CColor bg=at(int(r.left)+px-8,268+py%6);
            bg=blend(bg,CColor(20,16,20),.28*std::exp(-std::max(0.,distance(x-1,y-3))/2.));
            if(on)bg=blend(bg,Pink,.22*std::exp(-std::max(0.,d)/3.));
            if(d>0)return bg;
            CColor color(25,23,26);
            if(d< -2.)color=on?CColor(255,132,181):CColor(112,110,112);
            if(d< -4.){
                double t=std::clamp((y-4.)/(h-8.),0.,1.);
                color=on?blend(CColor(255,44,137),CColor(199,0,80),t):blend(CColor(52,51,54),CColor(24,24,26),t);
                const double led=std::hypot(x-43.,y-h*.5);
                if(on){color=blend(color,CColor(255,182,215),.65*std::exp(-led*led/170.));if(led<14)color=blend(color,CColor(255,145,192),std::clamp(14-led,0.,1.));if(led<10)color=blend(color,CColor(255,251,253),std::clamp(10-led,0.,1.));}
                else{if(led<14)color=blend(color,CColor(19,18,21),std::clamp(14-led,0.,1.));if(led<10)color=blend(color,CColor(83,60,77),std::clamp(10-led,0.,1.));}
            }
            return blend(bg,color,std::clamp(-d,0.,1.));
        });
    }
    void drawKnob(CDrawContext* c,size_t i,double v){
        const auto k=Knobs[i];double extent=int(k.arc+24);
        surround[i]->draw(c,CRect(k.x-extent,k.y-extent,k.x+extent,k.y+63));
        faces[i]->draw(c,CRect(k.x-k.face,k.y-k.face,k.x+k.face,k.y+k.face));
        c->setLineStyle(CLineStyle(CLineStyle::kLineCapRound,CLineStyle::kLineJoinRound));
        auto point=[&](double angle,double radius){angle*=pi/180.;return CPoint(k.x+radius*std::cos(angle),k.y+radius*std::sin(angle));};
        auto arc=[&](double end,CColor color,double width){
            if(end<=155.)return;c->setFrameColor(color);c->setLineWidth(width);
            int steps=std::max(1,int(std::ceil((end-155.)/1.5)));
            for(int j=0;j<steps;++j)c->drawLine(point(155.+(end-155.)*j/steps,k.arc),point(155.+(end-155.)*(j+1)/steps,k.arc));
        };
        arc(385.,CColor(46,45,47),3.5);arc(155.+230.*v,Pink,5.);
        c->setFrameColor(CColor(40,39,40));c->setLineWidth(3.5);
        for(int tick=0;tick<5;++tick)c->drawLine(point(155.+tick*57.5,k.arc-15.),point(155.+tick*57.5,k.arc-9.));
        double angle=155.+230.*v;CPoint a=point(angle,k.face*.35),b=point(angle,k.face*.82);
        c->setFrameColor(CColor(8,8,10,230));c->setLineWidth(12.);c->drawLine(a,b);
        c->setFrameColor(Pink);c->setLineWidth(7.5);c->drawLine(a,b);
        c->setFrameColor(CColor(255,155,198));c->setLineWidth(2.);c->drawLine(CPoint(a.x-1.,a.y-1.),CPoint(b.x-1.,b.y-1.));
        // Restore complete original label pixels above the animated ring.
        // These protected rectangles must never be cleaned as dial background.
        const CRect labels[]={CRect(125,361,283,391),CRect(407,361,582,391),CRect(137,630,248,665)};
        const auto label=labels[i];art->draw(c,label,CPoint(label.left,label.top));
    }
    CRect button(int index)const{return index==0?CRect(78,283,298,343):CRect(319,283,539,343);}
    CRect knobBounds(size_t i)const{const auto k=Knobs[i];double e=k.arc+26;return CRect(k.x-e,i==2?630:361,k.x+e,k.y+65);}
    CRect gripRect()const{return CRect(1458,977,1500,1009);}
    int hit(const CPoint& p)const{for(auto k:Knobs)if(std::hypot(p.x-k.x,p.y-k.y)<k.arc+5)return int(k.tag);return -1;}
    double value(int tag)const{return normalized(ctl->getParamNormalized(tag));}
    void invalidate(int tag){
        if(tag<=Trim){invalidRect(knobBounds(tag));if(readouts[tag])readouts[tag]->invalid();}
        else if(tag==Bypass||tag==Link){auto r=button(tag==Bypass?0:1);r.extend(8,8);invalidRect(r);}
    }
    void setOne(int tag,double v){v=normalized(v);if(v==value(tag))return;ctl->setParamNormalized(tag,v);ctl->performEdit(tag,v);invalidate(tag);}
    void start(int tag){finish();active=tag;ctl->beginEdit(tag);linked=(tag==Mid||tag==High)&&value(Link)>.5;if(linked)ctl->beginEdit(tag==Mid?High:Mid);}
    void setGesture(double next){
        if(active<0)return;next=normalized(next);double delta=next-value(active);
        if(linked){int other=active==Mid?High:Mid;delta=std::clamp(delta,-value(other),1.-value(other));setOne(other,value(other)+delta);}
        setOne(active,value(active)+delta);refreshReadouts();
    }
    void finish(){if(active>=0){if(linked)ctl->endEdit(active==Mid?High:Mid);ctl->endEdit(active);}active=-1;linked=false;}
    void change(int tag,double v){start(tag);setGesture(v);finish();}
    void refreshReadouts(){
        for(auto* r:readouts)if(r&&!r->getPlatformTextEdit()){
            auto tag=r->getTag();double plain=ctl->getParameterObject(tag)->toPlain(value(tag));char text[32];
            if(tag==Trim)std::snprintf(text,sizeof(text),"%.1f dB",std::abs(plain)<.05?0.:plain);
            else std::snprintf(text,sizeof(text),"%.0f%%",plain);
            if(r->getText().getString()!=text)r->setText(text);
        }
    }
    Controller* ctl;VST3Editor* editor;
    Bitmap art,surround[3],faces[3],switches[2][2];std::vector<CColor> pixels;
    SharedPointer<CFontDesc> numberFont,labelFont;
    std::array<Readout*,3> readouts{};std::array<double,ParamCount+2> previous{};
    int active=-1;double lastY=0.;bool linked=false;
    int resizeAxis=0;double resizeStart=.5;CPoint resizeOrigin;bool resizing=false;
};
}
CView* createPanel(Controller* c,VST3Editor* e){return new Panel(c,e);}
}
