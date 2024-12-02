#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Modules/ModuleInterface.h"
#include "MultithreadedLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE(FThreadWorkDelegate);

// Forward declarations
class FRunnableThread;

// Enhanced Multithreaded Task that allows for more control
class SUPERTHREADER_API FEnhancedMultithreadedTask : public FRunnable
{
public:
    FEnhancedMultithreadedTask(const FThreadWorkDelegate& InWorkDelegate, bool bInRunOnce);
    virtual ~FEnhancedMultithreadedTask();

    // FRunnable interface
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;

    // Control methods
    void RequestStop();
    bool IsRunning() const;
    bool IsCancelled() const;
    void SetThread(FRunnableThread* InThread) { Thread = InThread; }
    void WaitForThreadCompletion() { if (Thread) Thread->WaitForCompletion(); }

private:
    FThreadWorkDelegate WorkDelegate;
    FRunnableThread* Thread;
    FThreadSafeCounter StopRequestedCounter;
    FThreadSafeCounter RunningCounter;
    volatile bool bShuttingDown;
    volatile bool bRunOnce;
    double LastExecutionTime;
};

UCLASS()
class SUPERTHREADER_API UMultithreadedLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Start a new thread and return a unique identifier
    UFUNCTION(BlueprintCallable, Category = "SuperThreader")
    static int64 StartMultithreadedTask(const FThreadWorkDelegate& WorkFunction, bool bRunOnce = false);

    // Stop a specific thread by its identifier
    UFUNCTION(BlueprintCallable, Category = "SuperThreader")
    static bool StopMultithreadedTask(int64 TaskId);

    // Stop all running tasks
    UFUNCTION(BlueprintCallable, Category = "SuperThreader")
    static void StopAllTasks();

    // Check if a specific thread is still running
    UFUNCTION(BlueprintCallable, Category = "SuperThreader")
    static bool IsThreadRunning(int64 TaskId);

private:
    // Manage active threads
    static TMap<int64, FEnhancedMultithreadedTask*> ActiveThreads;
    static FCriticalSection ThreadMapCriticalSection;
};