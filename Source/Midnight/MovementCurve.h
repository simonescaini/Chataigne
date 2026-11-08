#pragma once
#include "JuceHeader.h"

// Application adapter over the shared Automation model/evaluators. Fitting is view-only.
class MidnightMovementCurve : public Automation {
public:
    explicit MidnightMovementCurve(std::function<Range<float>()> range)
        : Automation("Absolute position over time"), motorRange(std::move(range)) {
        viewValueRange->isSavable=false;
    }
    std::function<Range<float>()> motorRange;
    InspectableEditor* getEditorInternal(bool isRoot, Array<Inspectable*> = {}) override;
};

class MidnightCurveView : public Component, private Timer {
public:
    explicit MidnightCurveView(MidnightMovementCurve&);
    MidnightMovementCurve& model;
    AutomationUI canvas;
    Rectangle<float> envelope;
    Range<float> reference;
    void fit();
    void resized() override;
    void paint(Graphics&) override;
    void timerCallback() override { if(!isMouseButtonDown(true))fit(); repaint(); }
    Point<float> screenPoint(Point<float> p) const;
};
