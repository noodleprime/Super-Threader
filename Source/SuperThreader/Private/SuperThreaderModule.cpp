#include "SuperThreaderModule.h"
#include "MultithreadedLibrary.h"

#define LOCTEXT_NAMESPACE "FSuperThreaderModule"

void FSuperThreaderModule::StartupModule()
{
    UMultithreadedLibrary::Initialize();
}

void FSuperThreaderModule::ShutdownModule()
{
    UMultithreadedLibrary::Shutdown();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSuperThreaderModule, SuperThreader)