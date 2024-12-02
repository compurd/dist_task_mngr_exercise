/*
    A mock task to help distributed task manager implementation
*/

#include <string>

class MockTask {
public:
    enum Status {
        Waiting,
        Running,
        Cancelled,
        Aborted
    };

    MockTask(const std::string& description, int duration);
    std::string getId();
private:
    std::string m_id;
    std::string m_description;
    int m_duration;
    Status m_status = Status::Waiting;
};
