/*

Implements a multithreaded mock task manager with a RESTful API

*/

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <thread>

#include <crow.h>
#include "nlohmann/json.hpp"

#include "MockTask.hpp"
#include "Utils.hpp"

using json = nlohmann::json;

class TaskManager;

class Command {
public:
    Command(TaskManager& manager) : m_manager(manager) {};
    virtual void execute() = 0;

    void waitToBeExecuted() {
        std::unique_lock lock(m_mutex);
        m_condition.wait(lock, [this] {return m_executed; });
        lock.unlock();
    };

protected:
    std::condition_variable m_condition;
    std::mutex m_mutex;
    bool m_executed;
    TaskManager& m_manager;
};

class CreateTaskCommand : public Command {
public:
    CreateTaskCommand(TaskManager& manager): Command(manager){};
    void execute() override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::cout << "CreateTaskCommand executed" << std::endl;
            m_executed = true;
        }

        m_condition.notify_one();
    }

    void setId (const std::string& id) {

    }
private:
    std::string m_id;
};

class Controller {
public:
    void addCommand(std::shared_ptr<Command> command) {
        {
            std::lock_guard<std::mutex> lg(m_mutex);
            m_commands.push(command);
            std::cout << "Command added" << std::endl;
        } 
        m_condition.notify_one();
    }

    void run() {
        while(true) {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this] {return !m_commands.empty();});
            
            std::cout << "New Command notification received" << std::endl;
            
            while(!m_commands.empty()){
                auto cmd = m_commands.front();
                cmd->execute();
                m_commands.pop();
            }

            lock.unlock();
        }
    }
private:
    std::queue<std::shared_ptr<Command>> m_commands;
    std::condition_variable m_condition;
    std::mutex m_mutex;
};

class TaskManager {
public:
    TaskManager(){};
    std::string executeCreateTask();
private:
    std::unordered_map<std::string, std::shared_ptr<MockTask>> m_tasks;
    std::condition_variable m_condition;
    std::mutex m_mutex;
};

std::string TaskManager::executeCreateTask() {
    auto newTask = std::make_shared<MockTask>("Mock description", 10);
    auto id = newTask->getId();
    m_tasks[id] = newTask;
    return id;
}

int main() {

    crow::SimpleApp app;
    TaskManager taskManager;
    Controller controller;

    CROW_ROUTE(app, "/taches")
    .methods("POST"_method)
    ([&taskManager, &controller](const crow::request& req ){
        std::cout << "Request received" << std::endl;

        json reqData =  json::parse(req.body);
        
        std::string description;
        int duration;

        reqData["description"].get_to(description);
        reqData["duration"].get_to(duration);

        std::cout << "description: " + description  << std::endl;
        std::cout << "duration: " + std::to_string(duration) << std::endl;

        std::shared_ptr<Command> createTaskCommand = std::make_shared<CreateTaskCommand>(taskManager);
        controller.addCommand(createTaskCommand);
        createTaskCommand->waitToBeExecuted();

        std::cout << "CreateTaskCommand completion notification received" << std::endl;

        crow::json::wvalue response;
        response["message"] = "Hello World!";
        return response;
    });

    auto var = app.port(3000).multithreaded().run_async();


    controller.run();

    return 0;
}