#pragma once
#include "NativeService.h"
// Names are display labels. Serialization and execution always retain the stable motor ID.
class MidnightMotorChoice : public EnumParameter, private Timer {
public:
    MidnightMotorChoice(const String& label) : EnumParameter(label, "Select a shared Midnight motor") {
        refresh(); startTimerHz(2);
    }
    midnightNative::Id motorId() { return getValueData().toString().toStdString(); }
    void select(const midnightNative::Id& id) { refresh(String(id)); }
    var getJSONDataInternal() override {
        auto data=EnumParameter::getJSONDataInternal();
        data.getDynamicObject()->setProperty("motorId",String(motorId()));
        return data;
    }
    void loadJSONDataInternal(var data) override {
        const auto id=data.getProperty("motorId","").toString();
        refresh(id); setValueWithData(id);
    }
    void refresh(String requested=String()) {
        auto* shared=midnightNative::service();if(!shared)return;
        String selected=requested.isNotEmpty()?requested:getValueData().toString();
        Array<EnumValue> choices;choices.add({"None",""});
        bool found=selected.isEmpty();
        for(const auto& m:shared->snapshot().document.motors) {
            choices.add({String(m.name)+" ["+String(m.id).substring(0,8)+"]",String(m.id)});
            found|=selected==String(m.id);
        }
        if(!found)choices.add({"Missing motor ["+selected+"]",selected});
        String signature;for(const auto& c:choices)signature+=c.key+c.value.toString();
        if(signature!=previous){previous=signature;setOptions(choices);}
        setValueWithData(selected);
    }
private:
    String previous;
    void timerCallback() override { refresh(); }
};
