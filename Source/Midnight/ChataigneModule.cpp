#include "Module/ModuleIncludes.h"
#include "ChataigneModule.h"
#include "Module/ui/ModuleEditor.h"
namespace {
class MidnightModuleEditor final : public ModuleEditor {
public:
    MidnightModuleEditor(MidnightModule* m, bool root) : ModuleEditor({m}, root) { resetAndBuild(); }
    InspectableEditor* getEditorUIForContainer(ControllableContainer* cc) override {
        if (cc == &module->moduleParams) {
            auto ui = midnightNative::createHardwareEditor();
            ui->setSize(700, 720);
            return new GenericComponentEditor(cc, ui.release(), false);
        }
        return ModuleEditor::getEditorUIForContainer(cc);
    }
};
}
MidnightModule::MidnightModule() : Module("Midnight") {
    hideScripts();
    alwaysShowValues = true;
    includeValuesInSave = false; // Registry is saved once, in midnightNative.
    const StringArray names{"Move to position","Link","Haptic","End interaction","Activate Cupboard button","Deactivate Cupboard button","Reset Cupboard button"};
    for(int i=0;i<names.size();++i)defManager->add(CommandDefinition::createDef(this,"",names[i],&MidnightCommand::create,i<=2?CommandContext::BOTH:CommandContext::ACTION)->addParam("operation",i));
    setupIOConfiguration(true, true);
    refreshValues(); // Inputs exist before Actions and mappings load.
    startTimerHz(30);
}
void MidnightModule::afterLoadJSONDataInternal() {
    Module::afterLoadJSONDataInternal();
    refreshValues();
}
void MidnightModule::refreshValues() {
    auto* shared = midnightNative::service();
    if (!shared) return;
    const auto snapshot = shared->snapshot();
    std::set<String> present;
    for (const auto& m : snapshot.document.motors) {
        const String key = "motor" + String(m.id).removeCharacters("-");
        present.insert(key);
        auto* cc = valuesCC.getControllableContainerByName(key);
        if (!cc) {
            cc = new ControllableContainer(String(m.name));
            cc->setCustomShortName(key);
            cc->addStringParameter("Motor identity", "Stable project identity", String(m.id));
            cc->addFloatParameter("Measured angle", "Measured encoder angle in degrees", 0);
            cc->addFloatParameter("Requested angle", "Last requested absolute angle in degrees", 0);
            cc->addBoolParameter("Fresh feedback", "Measured feedback is less than 150 ms old", false);
            cc->addBoolParameter("Connected", "Shared adapter connection", false);
            cc->addIntParameter("Faults", "Available controller fault bits; see status for availability", 0);
            cc->addStringParameter("Status", "Shared service and controller status", "");
            for (auto* c : cc->controllables) { c->setControllableFeedbackOnly(true); c->isSavable = false; }
            valuesCC.addChildControllableContainer(cc, true);
        }
        if (cc->niceName != String(m.name)) cc->setNiceName(String(m.name));
        const auto found = snapshot.motors.find(m.id);
        if (found == snapshot.motors.end()) continue;
        const auto& f = found->second;
        auto set = [cc](const String& name, const var& value) {
            if (auto* p = dynamic_cast<Parameter*>(cc->getControllableByName(name, true))) p->setValue(value);
        };
        if (f.at >= 0) set("Measured angle", f.position);
        set("Requested angle", f.requested);
        set("Fresh feedback", f.at >= 0 && midnightNative::monotonic()-f.at < .15);
        set("Connected", f.connected);
        set("Faults", int(f.faults | f.reportFaults));
        set("Status", String(f.message) + (f.statusAt < 0 ? " | Fault feedback unavailable" : ""));
    }
    Array<ControllableContainer*> removed;
    for (auto& weak : valuesCC.controllableContainers)
        if (weak != nullptr && !present.count(weak->shortName)) removed.add(weak.get());
    for (auto* cc : removed) valuesCC.removeChildControllableContainer(cc);
}
InspectableEditor* MidnightModule::getEditorInternal(bool root, Array<Inspectable*>) {
    return new MidnightModuleEditor(this, root);
}
