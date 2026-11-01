// Midnight Native POC. GPL-3.0, matching the host application.
#pragma once
#include "JuceHeader.h"

namespace midnight
{
class Panel final : public ShapeShifterContentComponent,
                    private Timer,
                    private OSCReceiver::Listener<OSCReceiver::MessageLoopCallback>
{
public:
    explicit Panel(const String& name);
    ~Panel() override;
    static ShapeShifterContent* create(const String& name) { return new Panel(name); }
    void resized() override;
private:
    void timerCallback() override;
    void oscMessageReceived(const OSCMessage& message) override;
    void perform(const std::function<void()>& operation);
    void addButton(const String& text, std::function<void()> operation);
    void newProject();
    void editKey();
    void setDuration();
    void triggerInput();
    void sendMotor(bool stop);
    void showSequence();
    TextEditor motorName, target, speed, keyTime, keyValue, duration;
    ComboBox input;
    ToggleButton inputValue { "Simulated input HIGH" };
    Label title, projectStatus, stateStatus, sequenceStatus, positionStatus, oscStatus, errorStatus;
    OwnedArray<TextButton> buttons;
    OwnedArray<Label> fieldLabels;
    Array<Component*> fields;
    DatagramSocket feedbackSocket { false };
    OSCReceiver feedbackReceiver;
    String error, lastOSC { "No receiver acknowledgement yet" };
    float simulatedPosition = 0;
    double feedbackAt = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Panel)
};
}
