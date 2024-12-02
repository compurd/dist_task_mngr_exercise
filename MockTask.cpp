/*
    A mock task class to help distributed task manager implementation
*/

#pragma once

#include "MockTask.hpp"
#include "Utils.hpp"

MockTask::MockTask(const std::string& description, int duration) : m_description(description), m_duration(duration){
    m_id = utils::generateUUID();
}

std::string MockTask::getId() {
    return m_id;
}
