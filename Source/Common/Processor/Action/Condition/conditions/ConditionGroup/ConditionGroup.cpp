/*
  ==============================================================================

    ConditionGroup.cpp
    Created: 21 Feb 2017 11:27:53am
    Author:  Ben

  ==============================================================================
*/

ConditionGroup::ConditionGroup(var params, Multiplex* multiplex) :
	Condition(ConditionGroup::getTypeStringStatic(), params, multiplex),
	manager(multiplex)
{
	addChildControllableContainer(&manager);
	manager.addConditionManagerListener(this);
}

ConditionGroup::~ConditionGroup()
{
}

void ConditionGroup::setForceDisabled(bool value,bool force) {
    Condition::setForceDisabled(value,force);
    manager.setForceDisabled(value||!enabled->boolValue(),force);
}
void ConditionGroup::onContainerParameterChangedInternal(Parameter* p) {
    Condition::onContainerParameterChangedInternal(p);
    if(p==enabled)manager.setForceDisabled(forceDisabled||!enabled->boolValue());
}
void ConditionGroup::conditionManagerValidationChanged(ConditionManager *, int multiplexIndex, bool dispatchOnChangeOnly)
{
	setValid(multiplexIndex, manager.getIsValid(multiplexIndex, false), dispatchOnChangeOnly);
}

var ConditionGroup::getJSONData(bool includeNonOverriden)
{
	var data = Condition::getJSONData(includeNonOverriden);
	data.getDynamicObject()->setProperty("manager", manager.getJSONData());
	return data;
}

void ConditionGroup::loadJSONDataInternal(var data)
{
	Condition::loadJSONDataInternal(data);
	manager.loadJSONData(data.getProperty("manager", var()));
}
