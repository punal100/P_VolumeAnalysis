/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#include "P_VolumeAnalysis.h"

// Text localization namespace for this module
#define LOCTEXT_NAMESPACE "FP_VolumeAnalysisModule"

/**
 * Module startup - called after module is loaded into memory
 * Initialize any global resources or register systems here
 */
void FP_VolumeAnalysis::StartupModule()
{
	// Module initialization logic goes here
	// Currently empty as we don't have initialization steps
}

/**
 * Module shutdown - called before module is unloaded
 * Clean up any global resources or unregister systems here
 */
void FP_VolumeAnalysis::ShutdownModule()
{
	// Module cleanup logic goes here
	// Currently empty as we don't have resources to clean up
}

// Undefine the localization namespace to avoid conflicts
#undef LOCTEXT_NAMESPACE

// Register this module with Unreal's module system
IMPLEMENT_MODULE(FP_VolumeAnalysis, P_VolumeAnalysis)