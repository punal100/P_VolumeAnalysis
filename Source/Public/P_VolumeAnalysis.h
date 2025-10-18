/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Main module class for Volume Analysis Plugin
 * Handles module lifecycle (startup/shutdown) and initialization
 */
class FP_VolumeAnalysis : public IModuleInterface
{
public:
	/** Called when module is loaded into memory */
	virtual void StartupModule() override;

	/** Called when module is unloaded from memory */
	virtual void ShutdownModule() override;
};
