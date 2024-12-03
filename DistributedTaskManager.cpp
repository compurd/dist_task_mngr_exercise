/*

Implements a multithreaded mock task manager with a RESTful API

*/

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <queue>
#include <thread>

#include <crow.h>
#include "nlohmann/json.hpp"

#include "MockTask.hpp"
#include "Utils.hpp"

using json = nlohmann::json;

class TaskManager {
public:
    TaskManager(){};

    std::string executeCreateTask(const std::string& description, int duration){
        std::cout << "Entered TaskManager executeCreateTask" << std::endl;
        auto newTask = std::make_shared<MockTask>(description, duration);
        auto id = newTask->getId();
        m_tasks[id] = newTask;
        return id;
    }

    MockTaskView viewTask(const std::string& id){
        auto it = m_tasks.find(id);
        if(it == m_tasks.end()){
            return MockTaskView();

        }
        return it->second->getView();
    }
private:
    std::unordered_map<std::string, std::shared_ptr<MockTask>> m_tasks;
    std::condition_variable m_condition;
    std::mutex m_mutex;
};

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
    CreateTaskCommand(TaskManager& manager, const std::string& description, int duration)
    : Command(manager), m_description(description), m_duration(duration){};
    void execute() override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            m_id = m_manager.executeCreateTask(m_description, m_duration);
            auto m_taskView = m_manager.viewTask(m_id);
            
            if(m_taskView.id.empty()){
                std::cout << "Error: Unable to create the task" << std::endl;
            }

            std::cout << "CreateTaskCommand execution complete" << std::endl;
            m_executed = true;
        }

        m_condition.notify_one();
    }

    MockTaskView getTaskView() const {
        return m_manager.viewTask(m_id);
    }

    std::string getId() const {
        return m_id;
    }

private:
    std::string m_id;
    std::string m_description;
    int m_duration;
    MockTaskView m_taskView;
   
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

int main() {

    crow::SimpleApp app;
    TaskManager taskManager;
    Controller controller;

    CROW_ROUTE(app, "/taches")
    .methods("POST"_method)
    ([&taskManager, &controller](const crow::request& req ){
        std::cout << "Request received" << std::endl;

        std::string description;
        int duration;
        json reqData =  json::parse(req.body);
        reqData["description"].get_to(description);
        reqData["duration"].get_to(duration);

        std::cout << "description: " + description  << std::endl;
        std::cout << "duration: " + std::to_string(duration) << std::endl;

        std::shared_ptr<Command> command = 
            std::make_shared<CreateTaskCommand>(taskManager, description, duration);
        
        controller.addCommand(command);
        command->waitToBeExecuted();
        std::cout << "CreateTaskCommand completion notification received" << std::endl;

        auto createTaskCommand = std::dynamic_pointer_cast<CreateTaskCommand>(command);

        auto taskView = createTaskCommand->getTaskView();

        crow::json::wvalue response;

        if(taskView.id.empty()) {
             response["error"] = "Couldn't create a new task";
        } else {
            response["id"] = taskView.id;
            response["status"] = taskView.status;
            response["description"] = taskView.description;
            response["duration"] = taskView.duration;
        }

        return response;
    });

    auto var = app.port(3000).multithreaded().run_async();


    controller.run();

    return 0;
}