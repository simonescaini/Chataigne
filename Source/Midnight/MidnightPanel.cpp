// Compiled once through MainComponent.cpp, following Chataigne's unity include style.
#include "MidnightPanel.h"
#include "MainIncludes.h"
#include "StateMachine/StateMachineIncludes.h"
#include "TimeMachine/TimeMachineIncludes.h"
#include "Module/ModuleIncludes.h"
#include <cmath>
#include <stdexcept>

namespace midnight
{
namespace
{
constexpr auto moduleName = "Midnight Simulation";
constexpr auto sequenceName = "Midnight_Cupboard_Demo";
constexpr auto layerName = "Simulated motor position";
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

template<class T, class Manager>
T* named(Manager* manager, const String& name)
{
    for (auto* item : manager->items)
        if (item->niceName == name) return dynamic_cast<T*>(item);
    return nullptr;
}
CustomOSCModule* module()
{
    return named<CustomOSCModule>(ModuleManager::getInstance(), moduleName);
}
Sequence* sequence()
{
    return named<Sequence>(ChataigneSequenceManager::getInstance(), sequenceName);
}
Parameter* value(const String& name)
{
    auto* m = module();
    return m ? dynamic_cast<Parameter*>(m->valuesCC.getControllableByName(name, true)) : nullptr;
}
Mapping1DLayer* automation()
{
    auto* s = sequence();
    return s ? named<Mapping1DLayer>(s->layerManager.get(), layerName) : nullptr;
}
double number(const TextEditor& field)
{
    auto text = field.getText().trim().toStdString();
    size_t used = 0;
    double n = std::stod(text, &used);
    require(used == text.size() && std::isfinite(n), "Enter a finite number using a decimal point.");
    return n;
}
CustomOSCModule* createModule()
{
    if (auto* existing = module()) return existing;
    auto m = std::make_unique<CustomOSCModule>();
    m->setNiceName(moduleName);
    m->receiveCC->enabled->setValue(false);
    m->autoAdd->setValue(false);
    m->autoFeedback->setValue(false);
    m->logOutgoingData->setValue(true);
    auto* output = new OSCOutput();
    output->useLocal->setValue(true);
    output->remoteHost->setValue("127.0.0.1");
    output->remotePort->setValue(19001);
    output->listenToOutputFeedback->setValue(false);
    m->outputManager->addItem(output, var(), false);
    Array<Controllable*> controls;
    controls.add(m->valuesCC.addStringParameter("Motor name", "Simulation identifier, not a CAN ID", "cupboard-1"));
    controls.add(m->valuesCC.addFloatParameter("Target", "Simulated degrees", 0));
    controls.add(m->valuesCC.addFloatParameter("Speed", "Simulated degrees per second", 90, 0));
    controls.add(m->valuesCC.addFloatParameter("Automation position", "Time Machine output, simulated degrees", 0));
    for (const auto& name : { "Arm", "Run", "Return Idle" })
        controls.add(m->valuesCC.addBoolParameter(name, "Simulated transition input", false));
    for (auto* c : controls)
    {
        c->saveValueOnly = false;
        c->isCustomizableByUser = true;
        // These parameters are recreated from their serialized type on load;
        // their constructor defaults (e.g. cupboard-1 and 90) are not retained.
        if (auto* p = dynamic_cast<Parameter*>(c)) p->forceSaveValue = true;
    }
    auto* result = m.release();
    ModuleManager::getInstance()->addItem(result, var(), false);
    return result;
}
void configureOSC(Consequence& consequence, const String& address, const Array<var>& args)
{
    auto* m = module();
    require(m != nullptr, "Create the demo first.");
    auto* definition = m->defManager->getCommandDefinitionFor("", "Custom Message");
    require(definition != nullptr, "Chataigne OSC Custom Message definition is unavailable.");
    consequence.setCommand(definition);
    auto* command = dynamic_cast<CustomOSCCommand*>(consequence.command.get());
    require(command != nullptr, "Unexpected Chataigne OSC command type.");
    command->address->setValue(address);
    command->customValuesManager->clear();
    for (const auto& arg : args)
    {
        auto* a = command->customValuesManager->createItemFromType(
            arg.isString() ? Controllable::STRING : Controllable::FLOAT);
        require(a && a->param, "Could not construct an OSC argument.");
        a->param->setValue(arg);
        command->customValuesManager->addItem(a, var(), false);
    }
}
void oscAction(ConsequenceManager* manager, const String& address, const Array<var>& args)
{
    auto c = std::make_unique<Consequence>();
    configureOSC(*c, address, args);
    manager->addItem(c.release(), var(), false);
}
void sequenceAction(ConsequenceManager* manager, bool play)
{
    auto* sm = ChataigneSequenceManager::getInstance();
    auto* definition = sm->module->defManager->getCommandDefinitionFor(
        "Playback", play ? "Play Sequence" : "Stop Sequence");
    require(definition != nullptr, "Chataigne sequence command definition is unavailable.");
    auto c = std::make_unique<Consequence>();
    c->setCommand(definition);
    auto* command = dynamic_cast<SequenceCommand*>(c->command.get());
    require(command && sequence(), "Sequence command target is unavailable.");
    command->target->setValueFromTarget(sequence());
    if (play) command->playFromStart->setValue(true);
    manager->addItem(c.release(), var(), false);
}
void createSequence()
{
    createModule();
    if (sequence()) return;
    auto* s = new ChataigneSequence();
    s->setNiceName(sequenceName);
    ChataigneSequenceManager::getInstance()->addItem(s, var(), false);
    s->totalTime->setValue(6.0);
    s->loopParam->setValue(false);
    auto* layer = new Mapping1DLayer(s, var());
    layer->setNiceName(layerName);
    s->layerManager->addItem(layer, var(), false);
    auto& curve = layer->automation1D;
    curve.clear();
    curve.valueRange->setPoint(0, 90);
    curve.viewValueRange->setPoint(0, 90);
    curve.setLength(6);
    const Point<float> points[] { {0,0}, {1,45}, {3,90}, {5,0} };
    for (const auto& point : points)
        curve.addKey(point.x, point.y, false)->setEasing(Easing::LINEAR);
    auto* e = dynamic_cast<ChataigneEngine*>(Engine::mainEngine);
    require(e != nullptr, "Chataigne engine is unavailable.");
    auto* definition = e->module->defManager->getCommandDefinitionFor("", "Set Parameter Value");
    require(definition != nullptr, "Generic parameter mapping command is unavailable.");
    auto out = std::make_unique<MappingOutput>();
    out->setCommand(definition);
    auto* cmd = dynamic_cast<GenericControllableCommand*>(out->command.get());
    require(cmd != nullptr, "Unexpected generic mapping command.");
    cmd->target->setValueFromTarget(value("Automation position"));
    layer->mapping->om.addItem(out.release(), var(), false);
    auto* triggers = new ChataigneTriggerLayer(s);
    triggers->setNiceName("Simulated OSC motor commands");
    s->layerManager->addItem(triggers, var(), false);
    const String motor = value("Motor name")->stringValue();
    const float motorSpeed = value("Speed")->floatValue();
    for (const auto& point : points)
    {
        auto* trigger = new ChataigneTimeTrigger("Move " + String(point.y) + " deg");
        trigger->time->setValue(point.x);
        trigger->forwardSeek->setValueWithData(TimeTrigger::neverSeek);
        trigger->backwardSeek->setValueWithData(TimeTrigger::neverSeek);
        oscAction(trigger->csm.get(), "/midnight/sim/move", { motor, point.y, motorSpeed });
        triggers->ttm->addItem(trigger, var(), false);
    }
    Engine::mainEngine->changed();
}
State* makeState(const String& name, int x)
{
    auto* state = new State();
    state->setNiceName(name);
    StateManager::getInstance()->addItem(state, Point<float>((float)x, 100), false);
    state->active->setValue(false);
    state->loadActivationBehavior->setValueWithData(name == "IDLE" ? State::ACTIVE : State::NONACTIVE);
    return state;
}
void transition(State* from, State* to, const String& inputName)
{
    auto* t = new StateTransition(from, to);
    t->setNiceName(from->niceName + " to " + to->niceName);
    StateManager::getInstance()->stm.addItem(t, var(), false);
    auto* condition = new StandardCondition();
    t->cdm->addItem(condition, var(), false);
    condition->sourceTarget->setValueFromTarget(value(inputName));
    require(condition->comparator != nullptr, "Input comparator could not be constructed.");
    condition->comparator->compareFunction->setValueWithData("=");
    condition->comparator->reference->setValue(true);
    condition->forceCheck();
}
Action* onEnter(State* state)
{
    auto* action = new Action();
    action->setNiceName("Midnight on enter " + state->niceName);
    state->pm->addItem(action, var(), false);
    action->cdm->addItem(new ActivationCondition(), var(), false);
    return action;
}
void createStates()
{
    createSequence();
    auto* sm = StateManager::getInstance();
    require(!named<State>(sm, "IDLE") && !named<State>(sm, "ARMED") && !named<State>(sm, "RUNNING"),
            "Demo state names already exist. Use New experimental project for a fresh demo.");
    auto* idle = makeState("IDLE", 0);
    auto* armed = makeState("ARMED", 350);
    auto* running = makeState("RUNNING", 700);
    transition(idle, armed, "Arm");
    transition(armed, running, "Run");
    transition(running, idle, "Return Idle");
    auto* start = onEnter(running);
    sequenceAction(start->csmOn.get(), true);
    oscAction(start->csmOn.get(), "/midnight/sim/state", { "RUNNING" });
    auto* stop = onEnter(idle);
    sequenceAction(stop->csmOn.get(), false);
    oscAction(stop->csmOn.get(), "/midnight/sim/stop", { value("Motor name")->stringValue() });
    idle->active->setValue(true);
    Engine::mainEngine->changed();
}
}

Panel::Panel(const String& name) : ShapeShifterContentComponent(name)
{
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&controls, false);
    viewport.setScrollBarsShown(true, false);
    title.setText("MIDNIGHT NATIVE POC - SIMULATION ONLY", dontSendNotification);
    for (auto* label : { &title, &projectStatus, &stateStatus, &sequenceStatus,
                         &positionStatus, &oscStatus, &errorStatus }) controls.addAndMakeVisible(label);
    errorStatus.setColour(Label::textColourId, Colours::orange);
    auto field = [this](const String& name, TextEditor& editor, const String& initial)
    {
        auto* label = fieldLabels.add(new Label());
        label->setText(name, dontSendNotification);
        controls.addAndMakeVisible(label); controls.addAndMakeVisible(editor);
        editor.setText(initial); fields.add(&editor);
    };
    field("Motor identifier", motorName, "cupboard-1");
    motorName.setTooltip("Direct controls. Existing timed OSC actions retain their authored identifiers and speeds; edit those in the native sequence inspector.");
    field("Target (degrees)", target, "45");
    field("Speed (degrees/s)", speed, "90");
    field("Key time (seconds)", keyTime, "1");
    field("Key value (degrees)", keyValue, "45");
    field("Sequence duration (s)", duration, "6");
    input.addItemList({ "Arm", "Run", "Return Idle" }, 1);
    input.setSelectedId(1);
    controls.addAndMakeVisible(input); controls.addAndMakeVisible(inputValue);
    input.onChange = [this] { if (auto* p = value(input.getText())) inputValue.setToggleState(p->boolValue(), dontSendNotification); };
    inputValue.onClick = [this] { perform([this] { auto* p=value(input.getText()); require(p, "Create states first."); p->setValue(inputValue.getToggleState()); }); };
    addButton("New experimental project", [this] { newProject(); });
    addButton("Save project", [this]
    {
        Component::SafePointer<Panel> self(this);
        Engine::mainEngine->saveAsync(true, true, [self](FileBasedDocument::SaveResult r)
        { if (self && r == FileBasedDocument::failedToWriteToFile) self->error="Could not save project."; });
    });
    addButton("Open project", [this]
    {
        Component::SafePointer<Panel> self(this);
        Engine::mainEngine->saveIfNeededAndUserAgreesAsync([self](FileBasedDocument::SaveResult r)
        {
            if (!self || r == FileBasedDocument::userCancelledSave || r == FileBasedDocument::failedToWriteToFile) return;
            MessageManager::callAsync([self]
            {
                if (!self) return;
                Engine::mainEngine->loadFromUserSpecifiedFileAsync(true, [self](juce::Result loaded)
                { if(self && loaded.failed()) self->error=loaded.getErrorMessage(); });
            });
        });
    });
    addButton("Create demo states", [] { createStates(); });
    addButton("Create/select sequence", [this] { createSequence(); showSequence(); });
    addButton("Trigger selected input", [this] { triggerInput(); });
    addButton("Activate IDLE", [] { auto* s=named<State>(StateManager::getInstance(), "IDLE"); require(s, "Create states first."); s->active->setValue(true); });
    addButton("Native State Machine", [] { ShapeShifterManager::getInstance()->showContent("State Machine"); });
    addButton("Native sequence editor", [this] { showSequence(); });
    addButton("Add/update key", [this] { editKey(); });
    addButton("Set duration", [this] { setDuration(); });
    addButton("Play", [] { require(sequence(), "Create sequence first."); sequence()->playTrigger->trigger(); });
    addButton("Pause", [] { require(sequence(), "Create sequence first."); sequence()->pauseTrigger->trigger(); });
    addButton("Stop sequence + motor", [this] { if (sequence()) sequence()->stopTrigger->trigger(); sendMotor(true); });
    addButton("Send simulated move", [this] { sendMotor(false); });
    addButton("Refresh saved motor fields", [this] { if(value("Motor name")) { motorName.setText(value("Motor name")->stringValue()); speed.setText(value("Speed")->stringValue()); target.setText(value("Target")->stringValue()); } });
    feedbackReceiver.addListener(this);
    if (!feedbackSocket.bindToPort(19002, "127.0.0.1") || !feedbackReceiver.connectToSocket(feedbackSocket))
        error = "Simulation feedback port 127.0.0.1:19002 is unavailable.";
    startTimerHz(10); // Display refresh only. No sequence execution or state transitions here.
}
Panel::~Panel()
{
    stopTimer(); feedbackReceiver.removeListener(this); feedbackReceiver.disconnect();
}
void Panel::addButton(const String& text, std::function<void()> operation)
{
    auto* b = buttons.add(new TextButton(text)); controls.addAndMakeVisible(b);
    b->onClick = [this, operation] { perform(operation); };
}
void Panel::perform(const std::function<void()>& operation)
{
    if (!Engine::mainEngine || Engine::mainEngine->isLoadingFile || Engine::mainEngine->isClearing)
    { error = "Engine is loading or clearing its project."; return; }
    try { operation(); error.clear(); }
    catch (const std::exception& e) { error = e.what(); NLOGERROR("Midnight", error); }
}
void Panel::newProject()
{
    Component::SafePointer<Panel> self(this);
    Engine::mainEngine->saveIfNeededAndUserAgreesAsync([self](FileBasedDocument::SaveResult result)
    {
        if (!self || result == FileBasedDocument::userCancelledSave || result == FileBasedDocument::failedToWriteToFile) return;
        self->perform([] { Engine::mainEngine->createNewGraph(); createStates(); });
    });
}
void Panel::triggerInput()
{
    auto* p = value(input.getText()); require(p, "Create demo states first.");
    // Clear previous demo inputs so returning to IDLE does not immediately re-enter ARMED.
    // These are ordinary Chataigne input values; native conditions still own all transitions.
    for (const auto& name : { "Arm", "Run", "Return Idle" })
        if (auto* previous = value(name)) previous->setValue(false);
    p->setValue(true);
    inputValue.setToggleState(true, dontSendNotification);
}
void Panel::showSequence()
{
    require(sequence(), "Create sequence first.");
    auto* view = dynamic_cast<TimeMachineView*>(ShapeShifterManager::getInstance()->showContent("Sequence Editor"));
    require(view, "Native sequence editor is unavailable."); view->setSequence(sequence());
}
void Panel::editKey()
{
    auto* layer = automation(); require(layer, "Create sequence first.");
    const float t=(float)number(keyTime), v=(float)number(keyValue);
    require(t >= 0 && t <= sequence()->totalTime->floatValue(), "Key time must be within the sequence.");
    require(v >= 0 && v <= 90, "Demo curve range is 0 to 90 simulated degrees; edit its range in the native editor for other values.");
    bool updated=false;
    for (auto* key : layer->automation1D.items)
        if (std::abs(key->position->floatValue()-t) < 0.0001f) { key->value->setValue(v); updated=true; break; }
    if (!updated) layer->automation1D.addKey(t,v,true)->setEasing(Easing::LINEAR);
    Engine::mainEngine->changed(); showSequence();
}
void Panel::setDuration()
{
    require(sequence(), "Create sequence first.");
    const double d=number(duration); require(d > 0, "Duration must be positive.");
    // Do not silently move or discard existing engine keys/events.
    for (auto* layer : sequence()->layerManager->items)
    {
        if (auto* m=dynamic_cast<Mapping1DLayer*>(layer))
            for(auto* key:m->automation1D.items) require(key->position->doubleValue()<=d, "A key is beyond this duration; move it in the native editor first.");
        if (auto* t=dynamic_cast<TriggerLayer*>(layer))
            for(auto* key:t->ttm->items) require(key->time->doubleValue()<d, "A timed trigger is at or beyond this duration.");
    }
    sequence()->totalTime->setValue(d); Engine::mainEngine->changed();
}
void Panel::sendMotor(bool stop)
{
    require(module(), "Create demo first.");
    const auto name=motorName.getText().trim(); require(name.isNotEmpty(), "Enter a motor identifier.");
    Consequence action;
    if (stop) configureOSC(action,"/midnight/sim/stop",{name});
    else
    {
        const double p=number(target), s=number(speed); require(s>0,"Speed must be positive.");
        value("Motor name")->setValue(name); value("Target")->setValue(p); value("Speed")->setValue(s);
        configureOSC(action,"/midnight/sim/move",{name,p,s}); Engine::mainEngine->changed();
    }
    action.triggerCommand(); // Existing Chataigne command path, no direct OSCSender here.
}
void Panel::oscMessageReceived(const OSCMessage& m)
{
    if (m.getAddressPattern().toString()=="/midnight/sim/ack" && m.size()==1 && m[0].isString())
        lastOSC=m[0].getString();
    if (m.getAddressPattern().toString()=="/midnight/sim/error" && m.size()==1 && m[0].isString())
        error=m[0].getString();
    if (m.getAddressPattern().toString()=="/midnight/sim/feedback" && m.size()==3 && m[0].isString() && m[1].isFloat32() && m[2].isFloat32()
        && m[0].getString()==motorName.getText().trim())
    { simulatedPosition=m[1].getFloat32(); feedbackAt=Time::getMillisecondCounterHiRes(); }
}
void Panel::timerCallback()
{
    auto* e=Engine::mainEngine;
    if (!e || e->isLoadingFile || e->isClearing) { projectStatus.setText("Engine: loading / clearing",dontSendNotification); return; }
    projectStatus.setText("Project: " + e->getDocumentTitle() + (e->hasChangedSinceSaved() ? " *" : "") + " | Engine ready",dontSendNotification);
    StringArray active;
    for(auto* state:StateManager::getInstance()->items) if(state->active->boolValue()) active.add(state->niceName);
    stateStatus.setText("Active states: " + active.joinIntoString(", "),dontSendNotification);
    auto* seq=sequence();
    sequenceStatus.setText(seq ? seq->niceName + " | " + String(seq->currentTime->floatValue(),2) + " / " + String(seq->totalTime->floatValue(),2) + (seq->isPlaying->boolValue()?" s PLAYING":" s PAUSED/STOPPED") : "No demo sequence",dontSendNotification);
    const String current=feedbackAt>0 ? String(simulatedPosition,2)+" deg ("+String((Time::getMillisecondCounterHiRes()-feedbackAt)/1000,1)+" s old)" : "no feedback";
    positionStatus.setText("Receiver position: " + current + " | Native automation: " + (value("Automation position") ? String(value("Automation position")->floatValue(),2):"--"),dontSendNotification);
    oscStatus.setText("Last received OSC acknowledgement: " + lastOSC,dontSendNotification);
    errorStatus.setText(error,dontSendNotification);
}
void Panel::resized()
{
    viewport.setBounds(getLocalBounds());
    const int width=jmax(1, viewport.getWidth()-viewport.getScrollBarThickness());
    const int columns=width>=800?4:2;
    controls.setSize(width, 20+30+6*25+((buttons.size()+columns-1)/columns)*34+34+((fields.size()+1)/2)*48);
    auto area=controls.getLocalBounds().reduced(10);
    title.setBounds(area.removeFromTop(30));
    for(auto* label:{&projectStatus,&stateStatus,&sequenceStatus,&positionStatus,&oscStatus,&errorStatus}) label->setBounds(area.removeFromTop(25));
    for(int row=0;row<buttons.size();row+=columns)
    {
        auto strip=area.removeFromTop(34); int width=strip.getWidth()/columns;
        for(int c=0;c<columns && row+c<buttons.size();++c) buttons[row+c]->setBounds(strip.removeFromLeft(width).reduced(2));
    }
    auto controls=area.removeFromTop(34); input.setBounds(controls.removeFromLeft(180)); inputValue.setBounds(controls);
    for(int i=0;i<fields.size();i+=2)
    {
        auto row=area.removeFromTop(48); int width=row.getWidth()/2;
        for(int c=0;c<2 && i+c<fields.size();++c)
        { auto cell=row.removeFromLeft(width).reduced(3); fieldLabels[i+c]->setBounds(cell.removeFromTop(18)); fields[i+c]->setBounds(cell); }
    }
}
}
