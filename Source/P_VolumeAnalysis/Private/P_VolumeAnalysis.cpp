/*
 * @Author: Punal Manalan
 * @Description: Volume Analysis Plugin.
 * @Date: 18/10/2025
 */

#include "P_VolumeAnalysis.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPVolumeAnalysis, Log, All);

// Text localization namespace for this module
#define LOCTEXT_NAMESPACE "FP_VolumeAnalysisModule"

/**
 * Module startup - called after module is loaded into memory
 * Initialize any global resources or register systems here
 */
void FP_VolumeAnalysis::StartupModule()
{
    UE_LOG(LogPVolumeAnalysis, Display, TEXT("P_VolumeAnalysis: StartupModule"));
}

/**
 * Module shutdown - called before module is unloaded
 * Clean up any global resources or unregister systems here
 */
void FP_VolumeAnalysis::ShutdownModule()
{
    UE_LOG(LogPVolumeAnalysis, Display, TEXT("P_VolumeAnalysis: ShutdownModule"));
}

// Undefine the localization namespace to avoid conflicts
#undef LOCTEXT_NAMESPACE

// Register this module with Unreal's module system
IMPLEMENT_MODULE(FP_VolumeAnalysis, P_VolumeAnalysis)
