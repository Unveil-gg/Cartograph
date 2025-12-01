#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <string>

namespace Cartograph {

/**
 * Job types for background processing.
 */
enum class JobType {
    SaveProject,
    ExportPng,
    ProcessIcon,
    LoadProject
};

/**
 * Job completion callback.
 */
using JobCallback = std::function<void(bool success, const std::string& error)>;

/**
 * Job queue for background tasks.
 * Desktop: Uses a worker thread.
 * Web: Falls back to synchronous execution (TODO: Web Workers later).
 */
class JobQueue {
public:
    JobQueue();
    ~JobQueue();
    
    /**
     * Start the worker thread (desktop only).
     */
    void Start();
    
    /**
     * Stop the worker thread and wait for completion.
     */
    void Stop();
    
    /**
     * Enqueue a job for execution.
     * @param type Job type
     * @param work Work function (executed on worker thread)
     * @param callback Completion callback (executed on main thread)
     */
    void Enqueue(
        JobType type,
        std::function<void()> work,
        JobCallback callback
    );
    
    /**
     * Process completed jobs and invoke callbacks.
     * Call this from the main thread each frame.
     */
    void ProcessCallbacks();
    
    /**
     * Check if there are pending jobs.
     */
    bool HasPendingJobs() const;
    
private:
    struct Job {
        JobType type;
        std::function<void()> work;
        JobCallback callback;
        bool success;
        std::string error;
    };
    
    void WorkerThread();
    
    std::thread m_worker;
    std::atomic<bool> m_running;
    mutable std::mutex m_mutex;  // mutable allows locking in const methods
    std::condition_variable m_condition;
    std::queue<Job> m_pendingJobs;
    std::queue<Job> m_completedJobs;
};

} // namespace Cartograph

