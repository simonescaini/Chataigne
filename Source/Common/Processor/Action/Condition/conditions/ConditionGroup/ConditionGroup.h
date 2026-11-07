/*
  ==============================================================================

    ConditionGroup.h
    Created: 21 Feb 2017 11:27:53am
    Author:  Ben

  ==============================================================================
*/

#pragma once

class ConditionGroup :
	public Condition,
	public ConditionManagerListener
{
public:
	ConditionGroup(var params = var(), Multiplex* multiplex = nullptr);
	~ConditionGroup();

	ConditionManager manager;
    void setForceDisabled(bool value,bool force=false) override;
    void onContainerParameterChangedInternal(Parameter*) override;

	void conditionManagerValidationChanged(ConditionManager *, int multiplexIndex, bool dispatchOnChangeOnly) override;

	var getJSONData(bool includeNonOverriden = false) override;
	void loadJSONDataInternal(var data) override;

	String getTypeString() const override { return ConditionGroup::getTypeStringStatic(); }
	static String getTypeStringStatic() { return "Group"; }};

