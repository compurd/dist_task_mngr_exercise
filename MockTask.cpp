/*
    A mock task class to help distributed task manager implementation
*/

#include <vector>
#include <iostream>
#include "MockTask.hpp"
#include "Utils.hpp"


namespace{
    const std::vector<std::string> statusStrings = {"Waiting",
                                                    "Running",
                                                    "Cancelled",
                                                    "Aborted"};

std::string getStatusString (MockTask::Status status){
    return statusStrings[status];
}
}



MockTask::MockTask(const std::string& description, int duration) : m_description(description), m_duration(duration){
    m_id = utils::generateUUID();

    std::cout << "Generated id: " + m_id << std::endl;
}

MockTaskView MockTask::getView() const {
    return {m_id, m_description, m_duration, getStatusString(m_status)};
}

std::string MockTask::getId() const {
    return m_id;
}
