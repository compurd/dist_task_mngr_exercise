/*
    A mock task to help distributed task manager implementation
*/

#include <string>

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
        Cancelled,
        Aborted
    };

    MockTask(const std::string& description, int duration);
    std::string getId() const;
    MockTaskView getView() const;
private:
    std::string m_id;
    std::string m_description;
    int m_duration;
    Status m_status = Status::Waiting;
};


