#include "Recording.h"
namespace midnightNative {
class RecordingPanel final : public ShapeShifterContentComponent,private Timer {
public:
    ComboBox motor,takes;TextEditor name;Label details,error;Viewport viewport;
    OwnedArray<TextButton> buttons;std::vector<Id> motorIds,takeIds;Id selectedId;
    std::shared_ptr<RecordedMovement> selected;std::unique_ptr<InspectableEditor> editor;String motorSignature,takeSignature;
    explicit RecordingPanel(const String& title):ShapeShifterContentComponent(title){
        for(Component* c:std::initializer_list<Component*>{&motor,&takes,&name,&details,&error,&viewport})addAndMakeVisible(c);
        motor.setTextWhenNothingSelected("Select cupboard");takes.setTextWhenNothingSelected("Saved takes");name.setText("My movement");
        details.setColour(Label::textColourId,Colours::lightblue);error.setColour(Label::textColourId,Colours::orange);
        add("Record",[this]{auto n=motor.getSelectedId()-1;require(n>=0&&n<int(motorIds.size()),"Select a cupboard");service()->startCapture(motorIds[n]);});
        add("Stop recording",[this]{auto item=recordings()->retain(service()->stopCapture(),name.getText());selectedId=item->id;refresh();bind();});
        add("Save name",[this]{require(bool(selected),"Stop a take first");selected->name=name.getText();Engine::mainEngine->changed();refresh();});
        add("Create / restore curve",[this]{require(bool(selected),"Select a take");selected->convertOriginal();Engine::mainEngine->changed();bind();});
        add("Play movement",[this]{recordings()->play(selectedId);});
        add("End playback",[this]{recordings()->end(selectedId);});
        add("Release selected motor",[this]{auto n=motor.getSelectedId()-1;require(n>=0&&n<int(motorIds.size()),"Select a cupboard");service()->action([&](Runtime& runtime){runtime.stopMotor(motorIds[n]);});auto s=service()->snapshot();require(s.error.empty(),s.error);});
        takes.onChange=[this]{int i=takes.getSelectedId()-1;if(i>=0&&i<int(takeIds.size())){selectedId=takeIds[i];bind();}};
        refresh();startTimerHz(4);
    }
    ~RecordingPanel()override{stopTimer();viewport.setViewedComponent(nullptr,false);editor.reset();}
    static ShapeShifterContent* create(const String& s){return new RecordingPanel(s);}
    void add(const String& label,std::function<void()> fn){auto* b=buttons.add(new TextButton(label));addAndMakeVisible(b);b->onClick=[this,fn]{try{fn();error.setText({},dontSendNotification);}catch(const std::exception& e){error.setText(e.what(),dontSendNotification);}};}
    void bind(){viewport.setViewedComponent(nullptr,false);editor.reset();selected=recordings()->find(selectedId);if(selected){name.setText(selected->name);if(selected->editable){editor.reset(selected->movement.getEditor(false));viewport.setViewedComponent(editor.get(),false);}}resized();}
    void refresh(){
        if(!service()||!recordings())return;auto s=service()->snapshot();String signature;for(const auto& m:s.document.motors)signature+=String(m.id)+String(m.name);
        if(signature!=motorSignature){int old=motor.getSelectedId();motorSignature=signature;motor.clear(dontSendNotification);motorIds.clear();for(const auto& m:s.document.motors){motorIds.push_back(m.id);motor.addItem(String(m.name),int(motorIds.size()));}motor.setSelectedId(old,dontSendNotification);}
        signature={};for(auto item:recordings()->items)signature+=String(item->id)+item->name;
        if(signature!=takeSignature){takeSignature=signature;takes.clear(dontSendNotification);takeIds.clear();for(auto item:recordings()->items){takeIds.push_back(item->id);takes.addItem(item->name,int(takeIds.size()));if(item->id==selectedId)takes.setSelectedId(int(takeIds.size()),dontSendNotification);}if(selected&&!recordings()->find(selectedId)){selectedId.clear();bind();}}
    }
    void timerCallback()override{
        refresh();auto capture=service()->captureProgress();String text;
        if(capture.active)text="RECORDING measured encoder replies: "+String(int(capture.samples))+" samples | "+String(monotonic()-capture.startedAt,2)+" s. Record does not move or release the motor.";
        else if(selected){const auto& t=*selected->original;text="Original retained: "+String(int(t.samples.size()))+" samples; gaps >150 ms: "+String(int64(t.gaps))+"; largest gap "+String(t.largestGap,3)+" s. ";
            if(!t.samples.empty())text+="Recorded start "+String(t.samples.front().angle,4)+" degrees. Curve zero = first genuine sample ("+String(t.samples.front().elapsed,4)+" s after Record). ";
            text+="Conversion keeps every sample with linear interpolation, including across gaps. Save the show to retain takes and edits.";
        }else text="Select cupboard > Record > guide by hand > Stop > name > Create curve. Record captures only; Release is a separate existing motor command. Playback uses absolute angles and normal outputs.";
        details.setText(text,dontSendNotification);
    }
    void resized()override{
        auto r=getLocalBounds().reduced(10);auto top=r.removeFromTop(28);motor.setBounds(top.removeFromLeft(jmax(120,top.getWidth()/3)));top.removeFromLeft(5);takes.setBounds(top.removeFromLeft(top.getWidth()/2));top.removeFromLeft(5);name.setBounds(top);
        r.removeFromTop(8);auto row=r.removeFromTop(30);for(int i=0;i<4;++i){buttons[i]->setBounds(row.removeFromLeft(jmax(60,row.getWidth()/(4-i))).reduced(2));}
        r.removeFromTop(4);row=r.removeFromTop(30);for(int i=4;i<buttons.size();++i)buttons[i]->setBounds(row.removeFromLeft(jmax(60,row.getWidth()/(buttons.size()-i))).reduced(2));
        details.setBounds(r.removeFromTop(78));error.setBounds(r.removeFromTop(26));viewport.setBounds(r);if(editor)editor->setSize(jmax(240,viewport.getWidth()-18),editor->getHeight());
    }
    void paint(Graphics& g)override{g.fillAll(Colour(0xff111a24));}
};
}
