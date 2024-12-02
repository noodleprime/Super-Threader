#include "SuperThreaderModule.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"

IMPLEMENT_MODULE(FSuperThreaderModule, SuperThreader);

void FSuperThreaderModule::StartupModule()
{
    // Stats will be initialized automatically when needed
}

void FSuperThreaderModule::ShutdownModule()
{
    // Stats will be cleaned up automatically
}