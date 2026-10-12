#include "Jobs.h"

namespace Cartograph {

JobQueue::JobQueue()
    : m_running(false)
{
}

JobQueue::~JobQueue() {
    Stop();
}

void JobQueue::Start() {
    if (m_running) return;
    
    m_running = true;
#ifndef __EMSCRIPTEN__
    m_worker = std::thread(&JobQueue::WorkerThread, this);
#endif
}

void JobQueue::Stop() {
    if (!m_running) return;
    
    m_running = false;
    m_condition.notify_all();
    
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

void JobQueue::Enqueue(
    JobType type,
    std::function<void()> work,
    JobCallback callback
) {
    Job job;
    job.type = type;
    job.work = std::move(work);
    job.callback = std::move(callback);
    job.success = false;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingJobs.push(std::move(job));
    }
    
    m_condition.notify_one();
}

void JobQueue::RunJob(Job& job) {
    try {
        job.work();
        job.success = true;
    } catch (const std::exception& e) {
        job.success = false;
        job.error = e.what();
    } catch (...) {
        job.success = false;
        job.error = "Unknown error";
    }
}

void JobQueue::ProcessCallbacks() {
#ifdef __EMSCRIPTEN__
    // No worker thread in the browser: run queued work here, one frame
    // after it was enqueued, then deliver the callbacks below.
    std::queue<Job> pending;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        pending.swap(m_pendingJobs);
    }
    while (!pending.empty()) {
        RunJob(pending.front());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_completedJobs.push(std::move(pending.front()));
        pending.pop();
    }
#endif

    std::queue<Job> completed;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        completed.swap(m_completedJobs);
    }
    
    while (!completed.empty()) {
        Job& job = completed.front();
        if (job.callback) {
            job.callback(job.success, job.error);
        }
        completed.pop();
    }
}

bool JobQueue::HasPendingJobs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_pendingJobs.empty();
}

void JobQueue::WorkerThread() {
    while (m_running) {
        Job job;
        
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this] {
                return !m_running || !m_pendingJobs.empty();
            });
            
            if (!m_running) break;
            
            if (!m_pendingJobs.empty()) {
                job = std::move(m_pendingJobs.front());
                m_pendingJobs.pop();
            } else {
                continue;
            }
        }
        
        // Execute the work
        RunJob(job);
        
        // Move to completed queue
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_completedJobs.push(std::move(job));
        }
    }
}

} // namespace Cartograph

