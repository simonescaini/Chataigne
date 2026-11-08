#include "MovementCurve.h"
MidnightCurveView::MidnightCurveView(MidnightMovementCurve& curve):model(curve),canvas(&curve) {
    canvas.showNumberLines=false;canvas.disableOverlayFill=true;
    canvas.keysUI.autoAdaptViewRange=false;canvas.keysUI.transparentBG=true;
    addAndMakeVisible(canvas);setSize(360,220);startTimerHz(20);
}
void MidnightCurveView::resized(){
    canvas.setBounds(getLocalBounds().withTrimmedLeft(jmin(62,getWidth()/4)).withTrimmedRight(14).withTrimmedTop(25).withTrimmedBottom(40));fit();
}
void MidnightCurveView::fit(){
    reference=model.motorRange?model.motorRange():Range<float>(0,180);
    double x0=0,x1=model.length->doubleValue(),y0=reference.getStart(),y1=reference.getEnd();
    auto include=[&](Point<float> p){x0=jmin(x0,double(p.x));x1=jmax(x1,double(p.x));y0=jmin(y0,double(p.y));y1=jmax(y1,double(p.y));};
    for(auto* key:model.items){include(key->getPosAndValue());if(key->easing){auto b=key->easing->getBounds(true);include(b.getTopLeft());include(b.getBottomRight());}}
    const double dx=jmax(.01,x1-x0),dy=jmax(.01,y1-y0);
    // Leave at least 12 pixels for endpoint circles and tangent handles, even at fixed small sizes.
    const double px=dx*12/jmax(8,canvas.getWidth()-24),py=dy*12/jmax(8,canvas.getHeight()-24);
    envelope={float(x0-px),float(y0-py),float(dx+2*px),float(dy+2*py)};
    Point<float> v(envelope.getY(),envelope.getBottom());
    if(model.viewValueRange->getPoint()!=v)model.viewValueRange->setPoint(v);
    canvas.setViewRange(envelope.getX(),envelope.getRight());canvas.keysUI.resized();
}
Point<float> MidnightCurveView::screenPoint(Point<float> p) const {
    return {canvas.getX()+(p.x-envelope.getX())/envelope.getWidth()*canvas.getWidth(),
            canvas.getBottom()-(p.y-envelope.getY())/envelope.getHeight()*canvas.getHeight()};
}
void MidnightCurveView::paint(Graphics& g){
    g.fillAll(Colour(0xff171d26));if(envelope.isEmpty())return;
    const auto top=screenPoint({0,reference.getEnd()}).y,bottom=screenPoint({0,reference.getStart()}).y;
    g.setColour(Colour(0xff243c49));g.fillRect(float(canvas.getX()),top,float(canvas.getWidth()),bottom-top);
    g.setFont(12);g.setColour(Colours::lightblue);
    g.drawText("Mechanical degrees | saved MIN/MAX "+String(reference.getStart(),3)+" / "+String(reference.getEnd(),3),getLocalBounds().removeFromTop(22),Justification::centredLeft);
    int nx=jlimit(2,8,canvas.getWidth()/70),ny=jlimit(2,8,canvas.getHeight()/38);
    for(int i=0;i<=nx;++i){double t=model.length->doubleValue()*i/nx;float x=screenPoint({float(t),0}).x;
        g.setColour(Colour(0xff465161));g.drawVerticalLine(roundToInt(x),float(canvas.getY()),float(canvas.getBottom()));
        g.setColour(Colours::whitesmoke);g.drawText(String(t,3),roundToInt(x)-32,canvas.getBottom()+2,64,16,Justification::centred);}
    double step=std::pow(10.,std::floor(std::log10(envelope.getHeight()/ny)));
    while(envelope.getHeight()/step>ny+1)step*=2;
    for(double v=std::ceil(envelope.getY()/step)*step;v<=envelope.getBottom();v+=step){float y=screenPoint({0,float(v)}).y;
        g.setColour(Colour(0xff465161));g.drawHorizontalLine(roundToInt(y),float(canvas.getX()),float(canvas.getRight()));
        g.setColour(Colours::whitesmoke);g.drawText(String(v,3),0,roundToInt(y)-8,canvas.getX()-4,16,Justification::centredRight);}
    g.setColour(Colours::lightblue);g.drawHorizontalLine(roundToInt(top),float(canvas.getX()),float(canvas.getRight()));g.drawHorizontalLine(roundToInt(bottom),float(canvas.getX()),float(canvas.getRight()));
    String selected="Seconds | double-click a point for exact numeric editing";
    for(auto* key:model.items)if(key->isThisOrChildSelected()){selected="Selected: "+String(key->position->doubleValue(),6)+" s  |  "+String(key->value->doubleValue(),6)+" degrees";break;}
    g.setColour(Colours::white);g.drawText(selected,getLocalBounds().removeFromBottom(20),Justification::centredLeft);
}
namespace {
class MidnightCurveEditor final : public EnablingControllableContainerEditor {
public:
    MidnightCurveView view;
    MidnightCurveEditor(MidnightMovementCurve& c,bool root):EnablingControllableContainerEditor(&c,root),view(c){
        addAndMakeVisible(view);view.setSize(360,220);resetAndBuild();setSize(360,250);
    }
    void resizedInternalContent(Rectangle<int>& r) override {
        view.setVisible(!container->editorIsCollapsed);
        if(!container->editorIsCollapsed){if(r.getHeight()==0)r.setHeight(220);view.setBounds(r);r.translate(0,view.getHeight());}r.setHeight(0);
    }
};
}
InspectableEditor* MidnightMovementCurve::getEditorInternal(bool root,Array<Inspectable*>) {return new MidnightCurveEditor(*this,root);}
