// Real Chataigne + Midnight panel integration. Linked to the application's objects.
#include "MainIncludes.h"
#include "Module/ModuleIncludes.h"
#include "StateMachine/StateMachineIncludes.h"
#include "TimeMachine/TimeMachineIncludes.h"
#include "Midnight/MidnightPanel.h"
#include <iostream>
#include <stdexcept>
#include <windows.h>
#undef main

class MidnightTestApplication : public OrganicApplication
{
public:
    MidnightTestApplication() : OrganicApplication("Midnight Native acceptance", false) {}
    void initialiseInternal(const String&) override {}
};

static int checks = 0;
static void check(bool ok, const char* label)
{
    if (!ok) throw std::runtime_error(label);
    ++checks;
    std::cout << "PASS: " << label << std::endl;
}
static void pump(int milliseconds)
{
    const auto until = Time::getMillisecondCounterHiRes() + milliseconds;
    while (Time::getMillisecondCounterHiRes() < until)
    {
        MSG message;
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        { TranslateMessage(&message); DispatchMessageW(&message); }
        else Thread::sleep(1);
    }
}
static const Array<Component*>& controls(midnight::Panel& panel)
{
    for (auto* child : panel.getChildren())
        if (auto* viewport = dynamic_cast<Viewport*>(child))
            return viewport->getViewedComponent()->getChildren();
    return panel.getChildren();
}
static void click(midnight::Panel& panel, const String& text)
{
    for (auto* child : controls(panel))
        if (auto* button = dynamic_cast<TextButton*>(child))
            if (button->getButtonText() == text) { button->onClick(); pump(150); return; }
    throw std::runtime_error("Panel button missing: " + text.toStdString());
}
static void field(midnight::Panel& panel, int index, const String& value)
{
    int found = 0;
    for (auto* child : controls(panel))
        if (auto* editor = dynamic_cast<TextEditor*>(child))
            if (found++ == index) { editor->setText(value); return; }
    throw std::runtime_error("Panel field missing");
}
static void input(midnight::Panel& panel, int id)
{
    for (auto* child : controls(panel))
        if (auto* combo = dynamic_cast<ComboBox*>(child)) combo->setSelectedId(id);
    pump(20);
    click(panel, "Trigger selected input");
}
static bool labelContains(midnight::Panel& panel, const String& text)
{
    for (auto* child : controls(panel))
        if (auto* label = dynamic_cast<Label*>(child))
            if (label->getText().contains(text)) return true;
    return false;
}
static String fieldValue(midnight::Panel& panel, int index)
{
    int found = 0;
    for (auto* child : controls(panel))
        if (auto* editor = dynamic_cast<TextEditor*>(child))
            if (found++ == index) return editor->getText();
    throw std::runtime_error("Panel field missing");
}
static CustomOSCModule* module()
{
    for (auto* item : ModuleManager::getInstance()->items)
        if (item->niceName == "Midnight Simulation") return dynamic_cast<CustomOSCModule*>(item);
    return nullptr;
}
static Sequence* sequence()
{
    for (auto* item : ChataigneSequenceManager::getInstance()->items)
        if (item->niceName == "Midnight_Cupboard_Demo") return item;
    return nullptr;
}
static Mapping1DLayer* curve()
{
    for (auto* layer : sequence()->layerManager->items)
        if (auto* mapping = dynamic_cast<Mapping1DLayer*>(layer)) return mapping;
    return nullptr;
}
static bool active(const String& name)
{
    int count = 0;
    bool selected = false;
    for (auto* state : StateManager::getInstance()->items)
        if (state->active->boolValue()) { ++count; selected |= state->niceName == name; }
    return count == 1 && selected;
}
static void inspect(bool reloaded)
{
    check(ModuleManager::getInstance()->items.size() == 1 && module(), "only the simulation OSC module exists");
    auto* motor = dynamic_cast<Parameter*>(module()->valuesCC.getControllableByName("Motor name", true));
    auto* speed = dynamic_cast<Parameter*>(module()->valuesCC.getControllableByName("Speed", true));
    check(motor && motor->stringValue() == "cupboard-1", "default custom motor identifier is preserved");
    check(speed && speed->intValue() == (reloaded ? 60 : 90), "custom motor speed is preserved");
    check(!module()->receiveCC->enabled->boolValue(), "OSC module receiving is disabled");
    check(module()->outputManager->items.size() == 1, "exactly one OSC output");
    auto* output = module()->outputManager->items[0];
    check(output->useLocal->boolValue() && output->remoteHost->stringValue() == "127.0.0.1"
        && output->remotePort->intValue() == SystemStats::getEnvironmentVariable("MIDNIGHT_POC_COMMAND_PORT", "19001").getIntValue(), "OSC output targets its isolated loopback command port");
    auto* sm = StateManager::getInstance();
    check(sm->items.size() == 3 && sm->stm.items.size() == 3, "three native states and three native transitions");
    for (auto* transition : sm->stm.items)
        check(transition->cdm->items.size() == 1
            && dynamic_cast<StandardCondition*>(transition->cdm->items[0]) != nullptr,
            "native transition contains a standard input condition");
    check(sequence() && sequence()->layerManager->items.size() == 2, "native sequence has mapping and trigger layers");
    check(std::abs(sequence()->totalTime->doubleValue() - (reloaded ? 7 : 6)) < .001, "sequence duration survives authoring/reload");
    check(curve() && curve()->automation1D.items.size() == 4, "four real automation keys");
    check(std::abs(curve()->automation1D.items[1]->value->doubleValue() - (reloaded ? 30 : 45)) < .001,
        "automation key value survives authoring/reload");
    auto* layer = dynamic_cast<ChataigneTriggerLayer*>(sequence()->layerManager->items[1]);
    check(layer && layer->ttm->items.size() == 4, "four native timed OSC actions");
    check(active("IDLE"), "IDLE is the only active state");
}
static void playback(midnight::Panel& panel, bool reloaded)
{
    input(panel, 1);
    check(active("ARMED"), "Arm input drives IDLE to ARMED");
    check(!sequence()->isPlaying->boolValue(), "ARMED does not start playback");
    input(panel, 2);
    check(active("RUNNING") && sequence()->isPlaying->boolValue(), "Run input drives RUNNING and native playback");
    pump(1300);
    check(sequence()->currentTime->doubleValue() > 1, "native playback clock advances");
    auto* mapped = dynamic_cast<Parameter*>(module()->valuesCC.getControllableByName("Automation position", true));
    check(mapped && mapped->doubleValue() > (reloaded ? 30 : 45), "native automation changes its mapped parameter");
    check(labelContains(panel, "midnight/sim/move") && !labelContains(panel, "no feedback"),
        "native panel receives simulator acknowledgement and position");
    click(panel, "Pause");
    const auto paused = sequence()->currentTime->doubleValue();
    pump(350);
    check(!sequence()->isPlaying->boolValue() && std::abs(sequence()->currentTime->doubleValue() - paused) < .001,
        "Pause freezes native sequence time");
    click(panel, "Play");
    check(sequence()->isPlaying->boolValue() && sequence()->currentTime->doubleValue() >= paused,
        "Play resumes from paused time");
    pump(6500);
    check(!sequence()->isPlaying->boolValue(), "non-looping native sequence stops at its end");
    input(panel, 3);
    check(active("IDLE") && !sequence()->isPlaying->boolValue(), "Return Idle stops the native sequence");
}

int main(int argc, char** argv)
{
    if (argc != 3) { std::cerr << "Usage: native_integration_test create|reload project.noisette\n"; return 2; }
    ScopedJuceInitialiser_GUI gui;
    MidnightTestApplication app;
    app.engine.reset(new ChataigneEngine());
    app.mainComponent.reset(new MainContentComponent());
    app.appProperties->getUserSettings()->setValue("lastVersion", ProjectInfo::versionString);
    app.mainComponent->init();
    app.engine->createNewGraph();
    int code = 0;
    try
    {
        const bool reload = String(argv[1]) == "reload";
        File file(String::fromUTF8(argv[2]));
        midnight::Panel panel("MIDNIGHT NATIVE POC");
        panel.setBounds(0, 0, 400, 220);
        auto* viewport = dynamic_cast<Viewport*>(panel.getChildComponent(0));
        check(viewport && viewport->getViewedComponent()->getHeight() > viewport->getHeight(),
            "small dock exposes controls through a vertical viewport");
        panel.setBounds(0, 0, 1000, 650);
        if (reload)
        {
            check(file.existsAsFile(), "saved project exists in a new process");
            check(app.engine->loadFrom(file, false).wasOk(), "native FileBasedDocument load accepted");
            for (int i = 0; i < 200 && app.engine->isLoadingFile; ++i) pump(50);
            check(!app.engine->isLoadingFile, "native asynchronous project load completed");
            pump(400);
        }
        else click(panel, "New experimental project");
        inspect(reload);
        if (reload)
        {
            click(panel, "Refresh saved motor fields");
            check(fieldValue(panel, 0) == "cupboard-1" && fieldValue(panel, 2).getDoubleValue() == 60
                && fieldValue(panel, 1).getDoubleValue() == 22, "saved motor fields refresh correctly after restart");
        }
        playback(panel, reload);
        if (!reload)
        {
            field(panel, 3, "1"); field(panel, 4, "30");
            click(panel, "Add/update key");
            check(curve()->automation1D.items[1]->value->intValue() == 30, "panel edits native automation key");
            field(panel, 5, "4"); click(panel, "Set duration");
            check(sequence()->totalTime->intValue() == 6 && labelContains(panel, "beyond"),
                "shortening rejects keys/events beyond the proposed duration");
            field(panel, 5, "7"); click(panel, "Set duration");
            check(sequence()->totalTime->intValue() == 7, "panel changes native sequence duration");
            field(panel, 1, "22"); field(panel, 2, "60"); click(panel, "Send simulated move");
            pump(600);
            check(labelContains(panel, "22") && labelContains(panel, "midnight/sim/move"), "direct native command receives OSC acknowledgement");
            click(panel, "Stop sequence + motor");
            check(labelContains(panel, "midnight/sim/stop"), "explicit native stop receives OSC acknowledgement");
            bool saved = false, saveFinished = false;
            app.engine->saveAsAsync(file, false, false, false, [&](FileBasedDocument::SaveResult result)
            { saved = result == FileBasedDocument::savedOk; saveFinished = true; });
            for (int i = 0; i < 200 && !saveFinished; ++i) pump(50);
            check(saveFinished && saved,
                "native FileBasedDocument saves the project");
            check(file.getSize() > 1000 && JSON::parse(file).isObject(), "saved noisette contains native project JSON");
        }
        std::cout << "SUCCESS: " << checks << " native checks in " << argv[1] << " process\n";
        // Release editor references while the engine still exists.
        app.mainComponent->clear();
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << std::endl; code = 1; }
    app.mainComponent.reset();
    app.engine.reset();
    return code;
}
