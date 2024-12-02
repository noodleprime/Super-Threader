#include "MultithreadedLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuperThreader, Log, All);

TMap<int32, UMultithreadedLibrary::FTaskInfo> UMultithreadedLibrary::ActiveTasks;
FCriticalSection UMultithreadedLibrary::TaskLock;
bool UMultithreadedLibrary::bIsInitialized = false;

void FSuperThreadTask::DoWork()
{
    while (!bShouldStop)
    {
        if (Work.IsBound())
        {
            Work.Execute();
        }

        if (!bLooping)
        {
            break;
        }
    }
}

void UMultithreadedLibrary::Initialize()
{
    if (!bIsInitialized)
    {
        bIsInitialized = true;
        UE_LOG(LogSuperThreader, Log, TEXT("SuperThreader initialized"));
    }
}

void UMultithreadedLibrary::Shutdown()
{
    if (bIsInitialized)
    {
        StopAllThreads();
        bIsInitialized = false;
        UE_LOG(LogSuperThreader, Log, TEXT("SuperThreader shutdown"));
    }
}

int32 UMultithreadedLibrary::StartThread(const FThreadWorkDelegate& Work, bool bLooping)
{
    if (!bIsInitialized)
    {
        Initialize();
    }

    if (!Work.IsBound())
    {
        UE_LOG(LogSuperThreader, Warning, TEXT("Cannot start thread: Work delegate not bound"));
        return -1;
    }

    static int32 NextThreadId = 0;
    const int32 ThreadId = ++NextThreadId;

    FSuperThreadTask* Task = new FSuperThreadTask(Work, bLooping);
    auto* AsyncTask = new FAutoDeleteAsyncTask<FSuperThreadTask>(*Task);
    
    {
        FScopeLock Lock(&TaskLock);
        ActiveTasks.Add(ThreadId, FTaskInfo(Task, AsyncTask));
    }

    AsyncTask->StartBackgroundTask();
    UE_LOG(LogSuperThreader, Log, TEXT("Started thread %d"), ThreadId);

    return ThreadId;
}

void UMultithreadedLibrary::StopThread(int32 ThreadId)
{
    FScopeLock Lock(&TaskLock);
    
    if (FTaskInfo* TaskInfo = ActiveTasks.Find(ThreadId))
    {
        if (TaskInfo->IsValid())
        {
            TaskInfo->Task->Stop();
            ActiveTasks.Remove(ThreadId);
            UE_LOG(LogSuperThreader, Log, TEXT("Stopped thread %d"), ThreadId);
        }
    }
}

void UMultithreadedLibrary::StopAllThreads()
{
    FScopeLock Lock(&TaskLock);

    for (auto& Pair : ActiveTasks)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value.Task->Stop();
        }
    }

    ActiveTasks.Empty();
    UE_LOG(LogSuperThreader, Log, TEXT("Stopped all threads"));
}

bool UMultithreadedLibrary::IsThreadRunning(int32 ThreadId)
{
    FScopeLock Lock(&TaskLock);
    const FTaskInfo* TaskInfo = ActiveTasks.Find(ThreadId);
    return TaskInfo && TaskInfo->IsValid() && !TaskInfo->Task->IsStopped();
}