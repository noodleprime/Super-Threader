#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FSuperThreaderModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FSuperThreaderModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FSuperThreaderModule>("SuperThreader");
    }

    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("SuperThreader");
    }
};