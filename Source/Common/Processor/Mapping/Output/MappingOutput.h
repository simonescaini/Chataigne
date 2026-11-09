/*
  ==============================================================================

    MappingOutput.h
    Created: 28 Oct 2016 8:12:01pm
    Author:  bkupe

  ==============================================================================
*/

#pragma once
#include "Midnight/OutputAdmission.h"

#include "Common/Command/BaseCommandHandler.h"

class MappingOutput :
	public BaseCommandHandler
{
public:
	MappingOutput(Multiplex * multiplex = nullptr);
	virtual ~MappingOutput();

	bool forceDisabled;
    std::shared_ptr<midnightShow::OutputAdmission> admission;
    uint64_t admissionGeneration=0;

	Array<Array<WeakReference<Parameter>>> outParams;

	void setForceDisabled(bool value);

	virtual void setOutParams(Array<WeakReference<Parameter>> outParams, int multiplexIndex);
	void updateCommandOutParams();

	virtual void setCommand(CommandDefinition * cd) override;

	void setValue(var value, int multiplexIndex);
    void triggerCommand(int multiplexIndex=0) override;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingOutput)
};