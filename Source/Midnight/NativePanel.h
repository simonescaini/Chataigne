#pragma once
#include "JuceHeader.h"
#include "NativeService.h"
namespace midnightNative {
std::unique_ptr<Component> createEventEditor(const Id&);
std::unique_ptr<Component> createMotorEditor(const Id&);
class NativePanel final : public ShapeShifterContentComponent, private Timer {
public:
    explicit NativePanel(const String&);~NativePanel() override;
    static ShapeShifterContent* create(const String& name){return new NativePanel(name);}
    void resized() override;
    void paint(Graphics&) override;
    void openEditor(const Id&);void navigate(const Id&);void update();
    Id selected,parent;
    Snapshot current;
    void perform(const std::function<void()>&);
private:
    class Canvas;std::unique_ptr<Canvas> canvas;Viewport viewport;
    OwnedArray<TextButton> buttons;Label title,status,breadcrumb;Slider zoom;
    Document clipboard;Id copied;
    String localError;
    void timerCallback() override;
    void add(const String&,std::function<void()>);
    void newEvent(Kind);
};
}
