/*
    A mock task to help distributed task manager implementation
*/

#include <condition_variable>
#include <string>
#include <mutex>

struct MockTaskView {
    std::string id;
    std::string description;
    int duration;
    std::string status;
};

class MockTask {
public:
    enum Status {
        Waiting,
        Running,
        Finished,
        Cancelled,
        Aborted
    };

    MockTask(const std::string& description, int sleepTime);
    void compute();
    void abort();
    std::string getId() const;
    MockTaskView getView() const;
    bool isCancelled() const;
private:
    std::string m_id;
    std::string m_description;
    int m_sleepTimeMs;
    Status m_status;
    bool m_abort;
    std::mutex m_mutex;
    std::condition_variable m_condition;
};


