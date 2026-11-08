#include "MainIncludes.h"
#include "Midnight/BehaviourDefinition.h"
#include "Midnight/MovementCurve.h"
#include <iostream>
#include <windows.h>
#undef main
using namespace midnightNative;
class CurveApp:public OrganicApplication {public:CurveApp():OrganicApplication("Midnight curve software acceptance",false){} void initialiseInternal(const String&)override{}};
int main(int argc,char** argv){
 ScopedJuceInitialiser_GUI gui;CurveApp app;app.engine.reset(new ChataigneEngine());app.engine->createNewGraph();int checks=0;
 auto check=[&](bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);++checks;std::cout<<"PASS "<<msg<<std::endl;};
 try{
  Document doc;Adapter a;a.id="fake";a.simulated=true;a.reconnect=false;doc.adapters={a};Motor motor;motor.id="cupboard";motor.adapter=a.id;motor.min=-5;motor.max=5;doc.motors={motor};service()->edit(doc);
  for(double duration:{.05,3.,300.})for(double span:{.02,10.,2000.}){
   doc.motors[0].min=-span;doc.motors[0].max=span;service()->edit(doc);
   MidnightBehaviour definition(Kind::Move);definition.motor->select(motor.id);definition.duration->setValue(duration);definition.startAngle->setValue(-2*span);definition.target->setValue(2*span);
   auto* first=definition.curve->items[0];first->easingType->setValueWithData(Easing::BEZIER);auto* cubic=dynamic_cast<CubicEasing*>(first->easing.get());cubic->anchor1->setPoint(float(-duration*.2),float(8*span));cubic->anchor2->setPoint(float(duration*.2),float(-8*span));
   auto* model=dynamic_cast<MidnightMovementCurve*>(definition.curve.get());check(model!=nullptr,"native movement uses shared fitted editor adapter");
   const String before=JSON::toString(model->getJSONData());
   {
    MidnightCurveView view(*model);
    for(auto size:{Point<int>(240,150),Point<int>(360,220),Point<int>(900,450)}){
     view.setSize(size.x,size.y);view.fit();
     Array<Point<float>> points{first->getPosAndValue(),definition.curve->items.getLast()->getPosAndValue(),cubic->start+cubic->anchor1->getPoint(),cubic->end+cubic->anchor2->getPoint()};
     for(auto point:points){auto pixel=view.screenPoint(point);check(view.canvas.getBounds().reduced(10).toFloat().contains(pixel),"every endpoint and off-range handle fits inside fixed-size padded canvas");}
     check(view.reference.getStart()==float(-span)&&view.reference.getEnd()==float(span),"saved motor MIN/MAX remains the distinct reference band");
     check(JSON::toString(model->getJSONData())==before,"resize and display fitting preserve serialized curve data exactly");
    }
    if(duration==3&&span==10&&argc>1){first->selectThis();view.setSize(360,220);view.addToDesktop(0);view.setVisible(true);MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}FileOutputStream out(File(String::fromUTF8(argv[1])));out.setPosition(0);out.truncate();PNGImageFormat png;check(png.writeImageToStream(view.createComponentSnapshot(view.getLocalBounds()),out),"shared fixed-size curve editor renders");view.removeFromDesktop();}
   }
   check(JSON::toString(model->getJSONData())==before,"closing editor leaves motion intact");
   MidnightMovementCurve reopened([]{return Range<float>(-5,5);});reopened.loadJSONData(JSON::parse(before));
   check(JSON::toString(reopened.getJSONData())==before,"curve serialization survives save/reopen without motion changes");
  }
  check(service()->snapshot().events.empty(),"editing and fitting produce no motor output");
  std::cout<<"SUCCESS "<<checks<<" curve display checks"<<std::endl;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;app.engine.reset();return 1;}
 app.engine.reset();return 0;
}
