// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Async/AsyncWork.h"
#include "MultithreadedLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE(FThreadWorkDelegate);

class FSuperThreadTask : public FNonAbandonableTask
{
public:
    FSuperThreadTask(const FThreadWorkDelegate& InWork, bool bInLooping)
        : Work(InWork)
        , bLooping(bInLooping)
        , bShouldStop(false)
    {}

    void DoWork();
    void Stop() { bShouldStop = true; }
    bool IsStopped() const { return bShouldStop; }

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FSuperThreadTask, STATGROUP_ThreadPoolAsyncTasks);
    }

private:
    FThreadWorkDelegate Work;
    bool bLooping;
    FThreadSafeBool bShouldStop;
};

UCLASS()
class SUPERTHREADER_API UMultithreadedLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    static void Initialize();
    static void Shutdown();

    UFUNCTION(BlueprintCallable, Category = "Threading")
    static int32 StartThread(const FThreadWorkDelegate& Work, bool bLooping = false);

    UFUNCTION(BlueprintCallable, Category = "Threading")
    static void StopThread(int32 ThreadId);

    UFUNCTION(BlueprintCallable, Category = "Threading")
    static void StopAllThreads();

    UFUNCTION(BlueprintCallable, Category = "Threading")
    static bool IsThreadRunning(int32 ThreadId);

private:
    struct FTaskInfo
    {
        FSuperThreadTask* Task;
        FAutoDeleteAsyncTask<FSuperThreadTask>* AsyncTask;

        FTaskInfo() : Task(nullptr), AsyncTask(nullptr) {}
        FTaskInfo(FSuperThreadTask* InTask, FAutoDeleteAsyncTask<FSuperThreadTask>* InAsyncTask) 
            : Task(InTask), AsyncTask(InAsyncTask) {}

        bool IsValid() const { return Task != nullptr && AsyncTask != nullptr; }
    };

    static TMap<int32, FTaskInfo> ActiveTasks;
    static FCriticalSection TaskLock;
    static bool bIsInitialized;
};