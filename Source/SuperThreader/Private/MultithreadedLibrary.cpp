#include "MultithreadedLibrary.h"
#include "SuperThreaderModule.h"
#include "Engine/World.h"
#include "HAL/ThreadManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSuperThreader, Log, All);
DEFINE_LOG_CATEGORY(LogSuperThreader);

TMap<int64, FEnhancedMultithreadedTask*> UMultithreadedLibrary::ActiveThreads;
FCriticalSection UMultithreadedLibrary::ThreadMapCriticalSection;

FEnhancedMultithreadedTask::FEnhancedMultithreadedTask(const FThreadWorkDelegate& InWorkDelegate, bool bInRunOnce)
    : WorkDelegate(InWorkDelegate)
    , Thread(nullptr)
    , StopRequestedCounter(0)
    , RunningCounter(0)
    , bShuttingDown(false)
    , bRunOnce(bInRunOnce)
    , LastExecutionTime(0.0)
{
}

FEnhancedMultithreadedTask::~FEnhancedMultithreadedTask()
{
    bShuttingDown = true;
    RequestStop();

    // Quick attempt to stop gracefully
    const double ShutdownTimeout = 0.5;
    const double StartTime = FPlatformTime::Seconds();
    
    while (IsRunning() && (FPlatformTime::Seconds() - StartTime) < ShutdownTimeout)
    {
        FPlatformProcess::Sleep(0.001f);
    }

    if (Thread)
    {
        Thread->Kill(true);
        delete Thread;
        Thread = nullptr;
    }
}

bool FEnhancedMultithreadedTask::Init()
{
    RunningCounter.Increment();
    LastExecutionTime = FPlatformTime::Seconds();
    return true;
}

uint32 FEnhancedMultithreadedTask::Run()
{
    static const double MinTimeBetweenExecutions = 0.016; // ~60 FPS cap
    bool bHasExecutedOnce = false;
    
    // Get thread info
    const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
    FString ThreadName = FString::Printf(TEXT("[SuperThreader] Worker Thread %u"), ThreadId);
    
    // Register thread name with OS
    FPlatformProcess::SetThreadName(*ThreadName);
    
    // Register with thread manager if we have a valid thread
    if (Thread)
    {
        FThreadManager::Get().AddThread(ThreadId, Thread);
    }
    
    UE_LOG(LogSuperThreader, Log, TEXT("Thread started - Name: %s, ID: %u"), *ThreadName, ThreadId);
    
    while (!StopRequestedCounter.GetValue() && !bShuttingDown)
    {
        const double CurrentTime = FPlatformTime::Seconds();
        const double TimeSinceLastExecution = CurrentTime - LastExecutionTime;

        if (TimeSinceLastExecution >= MinTimeBetweenExecutions)
        {
            if (!bShuttingDown && WorkDelegate.IsBound())
            {
                WorkDelegate.Execute();
                bHasExecutedOnce = true;

                if (bRunOnce && bHasExecutedOnce)
                {
                    RequestStop();
                    break;
                }
            }
            
            LastExecutionTime = CurrentTime;
        }
        else
        {
            // Use a shorter sleep time to be more responsive
            FPlatformProcess::Sleep(0.001f);
        }
    }

    UE_LOG(LogSuperThreader, Log, TEXT("Thread ended - Name: %s, ID: %u"), *ThreadName, ThreadId);
    
    // Unregister from thread manager
    if (Thread)
    {
        FThreadManager::Get().RemoveThread(Thread);
    }
    
    RunningCounter.Decrement();
    return 0;
}

void FEnhancedMultithreadedTask::Stop()
{
    RequestStop();
}

void FEnhancedMultithreadedTask::Exit()
{
    bShuttingDown = true;
}

void FEnhancedMultithreadedTask::RequestStop()
{
    bShuttingDown = true;
    StopRequestedCounter.Increment();
}

bool FEnhancedMultithreadedTask::IsRunning() const
{
    return RunningCounter.GetValue() > 0 && !bShuttingDown;
}

bool FEnhancedMultithreadedTask::IsCancelled() const
{
    return StopRequestedCounter.GetValue() > 0 || bShuttingDown;
}

int64 UMultithreadedLibrary::StartMultithreadedTask(const FThreadWorkDelegate& WorkFunction, bool bRunOnce)
{
    if (!WorkFunction.IsBound())
    {
        UE_LOG(LogSuperThreader, Warning, TEXT("StartMultithreadedTask: Work function not bound"));
        return -1;
    }

    FEnhancedMultithreadedTask* Task = new FEnhancedMultithreadedTask(WorkFunction, bRunOnce);
    
    // Generate a unique task ID using high-resolution timer
    int64 TaskId = static_cast<int64>(FPlatformTime::Seconds() * 1000000.0);

    // Create descriptive thread name
    FString ThreadName = FString::Printf(TEXT("[SuperThreader] Task %lld"), TaskId);
    
    UE_LOG(LogSuperThreader, Log, TEXT("Creating thread: %s"), *ThreadName);

    // Create and start the thread with custom name
    uint32 StackSize = 128 * 1024; // 128KB stack
    FRunnableThread* Thread = FRunnableThread::Create(
        Task,
        *ThreadName,
        StackSize,
        TPri_Normal,
        FPlatformAffinity::GetNoAffinityMask()
    );
    
    if (Thread)
    {
        Task->SetThread(Thread);

        // Safely add to active threads
        FScopeLock Lock(&ThreadMapCriticalSection);
        ActiveThreads.Add(TaskId, Task);
        
        UE_LOG(LogSuperThreader, Log, TEXT("Thread created successfully: %s (Total threads: %d)"), *ThreadName, ActiveThreads.Num());
        return TaskId;
    }
    else
    {
        UE_LOG(LogSuperThreader, Error, TEXT("Failed to create thread: %s"), *ThreadName);
        delete Task;
        return -1;
    }
}

bool UMultithreadedLibrary::StopMultithreadedTask(int64 TaskId)
{
    FEnhancedMultithreadedTask* TaskToStop = nullptr;
    
    {
        FScopeLock Lock(&ThreadMapCriticalSection);
        if (FEnhancedMultithreadedTask** TaskPtr = ActiveThreads.Find(TaskId))
        {
            TaskToStop = *TaskPtr;
            ActiveThreads.Remove(TaskId);
        }
    }

    if (TaskToStop)
    {
        TaskToStop->RequestStop();
        delete TaskToStop;
        return true;
    }

    return false;
}

void UMultithreadedLibrary::StopAllTasks()
{
    TArray<int64> TaskIds;
    
    {
        FScopeLock Lock(&ThreadMapCriticalSection);
        ActiveThreads.GetKeys(TaskIds);
    }
    
    // Stop all tasks
    for (int64 TaskId : TaskIds)
    {
        StopMultithreadedTask(TaskId);
    }
}

bool UMultithreadedLibrary::IsThreadRunning(int64 TaskId)
{
    FScopeLock Lock(&ThreadMapCriticalSection);
    if (FEnhancedMultithreadedTask** TaskPtr = ActiveThreads.Find(TaskId))
    {
        return (*TaskPtr)->IsRunning();
    }
    return false;
}