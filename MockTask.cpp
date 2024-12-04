/*
    A mock task class to help distributed task manager implementation
*/

#include <chrono>
#include <vector>
#include <iostream>
#include "MockTask.hpp"
#include "Utils.hpp"


namespace{
    const std::vector<std::string> statusStrings = {"Waiting",
                                                    "Running",
                                                    "Finished", 
                                                    "Cancelled",
                                                    "Failed"};

std::string getStatusString (MockTask::Status status){
    return statusStrings[status];
}
}



MockTask::MockTask(const std::string& description, int sleepTime) 
: m_description(description), m_sleepTimeMs(sleepTime), m_status(Status::Waiting), m_abort(false){
    m_id = utils::generateUUID();
}

void MockTask::compute() {
    std::cout << "Task " << m_id << " started, sleeping for " << m_sleepTimeMs << " miliseconds..." << std::endl;
    const auto start = std::chrono::steady_clock::now();
    const auto jobDuration = std::chrono::milliseconds(m_sleepTimeMs);

    std::unique_lock<std::mutex> lock(m_mutex);
    if(m_status != Status::Cancelled) {
        m_status = Status::Running;
        if(!m_condition.wait_for(lock, jobDuration, [this]{ return m_abort; })){
            m_status = Status::Finished;
            const auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            std::cout << "Task " << m_id << " finished after " << timeMs.count() << " miliseconds." << std::endl;
        } else {
            m_status = Status::Failed;
            const auto timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
            std::cout << "Task " << m_id << " aborted and didn't get time to finish. Stopped after " << timeMs.count() << " miliseconds." << std::endl;
        }
    }
    lock.unlock();
}

void MockTask::abort() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_abort = true;
        if(m_status == Status::Waiting) {
            m_status = Status::Cancelled;
        }
    }
    m_condition.notify_one();
}

MockTaskView MockTask::getView() const {
    return {m_id, m_description, m_sleepTimeMs, getStatusString(m_status)};
}

std::string MockTask::getId() const {
    return m_id;
}

bool MockTask::isCancelled() const {
    return m_status == Status::Cancelled;
}
