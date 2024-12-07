/*

Implements a multithreaded mock task manager with a RESTful API

*/

#include <condition_variable>
#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <queue>
#include <thread>
#include <vector>

#include <crow.h>
#include "nlohmann/json.hpp"

#include "MockTask.hpp"
#include "Utils.hpp"

using json = nlohmann::json;

class TaskManager {
public:
    TaskManager(size_t nTasks){
        for(size_t i = 0; i < nTasks; ++i) {
            m_workers.emplace_back([this](){
                for(;;) {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_condition.wait(lock, [this](){ return !m_waitingTasks.empty();});
                    auto task = m_waitingTasks.front();
                    m_waitingTasks.pop();
                    lock.unlock();

                    if(task->isCancelled()){
                        continue;
                    }
                    task->compute();
                }
            });
        }
    };

    std::string executeCreateTask(const std::string& description, int duration){
        auto newTask = std::make_shared<MockTask>(description, duration);
        auto id = newTask->getId();
        m_tasks[id] = newTask;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_waitingTasks.push(newTask);
        }
        m_condition.notify_one();
        return id;
    }

    MockTaskView viewTask(const std::string& id) const {
        auto it = m_tasks.find(id);
        if(it == m_tasks.end()){
            return MockTaskView();

        }
        return it->second->getView();
    }

    std::vector<MockTaskView> viewAllTasks() const {
        std::vector<MockTaskView> views;
        std::transform(m_tasks.begin(), m_tasks.end(), std::back_inserter(views), 
            [](const std::pair<std::string, std::shared_ptr<MockTask>>& pair) { return pair.second->getView();});
        return views;
    }

    bool cancelTask(const std::string& id){
        auto it = m_tasks.find(id);
        if(it == m_tasks.end()){
            return false;
        }

        it->second->abort();
        return true;
    }
private:
    std::unordered_map<std::string, std::shared_ptr<MockTask>> m_tasks;
    std::queue<std::shared_ptr<MockTask>> m_waitingTasks;
    std::vector<std::thread> m_workers;
    std::condition_variable m_condition;
    std::mutex m_mutex;
};

class Command {
public:
    Command(TaskManager& manager) : m_manager(manager), m_executed(false) {};
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

class CommandFactory {
public:
    CommandFactory(TaskManager& manager) : m_manager(manager) {};
    
    template<typename T, typename... Args>
    std::shared_ptr<Command> create(Args... args) {
        return std::make_shared<T>(m_manager, std::forward<Args>(args)...);
    }
protected:
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

class GetTaskCommand : public Command {
public:    
    GetTaskCommand(TaskManager& manager, std::string id) : Command(manager), m_id(id) {}
    void execute() override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_view = m_manager.viewTask(m_id);
            m_executed = true;
        }
        m_condition.notify_one();
    }

    bool noTaskFound() const {
        return m_view.id.empty();
    }

    MockTaskView getTaskView() const {
        return m_view;
    }
private:
    std::string m_id;
    MockTaskView m_view;
};

class GetAllTasksCommand : public Command {
public:
    GetAllTasksCommand(TaskManager& manager) : Command(manager) {}

    void execute() override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_taskViews = m_manager.viewAllTasks();
            m_executed = true;
        }
        m_condition.notify_one();
    }

    std::vector<MockTaskView> getTaskViews() const {
        return m_taskViews;
    }
private:
    std::vector<MockTaskView> m_taskViews;
};

class CancelTaskCommand : public Command {
public:
    CancelTaskCommand(TaskManager& manager, std::string id) : Command(manager), m_id(id) {}
    enum Status {
        Canceled,
        NotFound
    };

    void execute() override {
         {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_status = m_manager.cancelTask(m_id) ? Canceled : NotFound;
            m_executed = true;
        }
        m_condition.notify_one();
    }

    Status getStatus() const {
        return m_status;
    }
private:
   std::string m_id;
   Status m_status;
};

class Controller {
public:
    void addCommand(std::shared_ptr<Command> command) {
        {
            std::lock_guard<std::mutex> lg(m_mutex);
            m_commands.push(command);
        } 
        m_condition.notify_one();
    }

    void run() {
        while(true) {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock, [this] {return !m_commands.empty();});
            auto cmd = m_commands.front();
            cmd->execute();
            m_commands.pop();
            lock.unlock();
        }
    }
private:
    std::queue<std::shared_ptr<Command>> m_commands;
    std::condition_variable m_condition;
    std::mutex m_mutex;
};

crow::json::wvalue toCrowJson(MockTaskView taskView) {
    crow::json::wvalue response;
    response["id"] = taskView.id;
    response["status"] = taskView.status;
    response["description"] = taskView.description;
    response["duration"] = taskView.duration;
    return response;
}

crow::json::wvalue toCrowJson(const std::vector<MockTaskView>& allTasksViews) {
    crow::json::wvalue j;
    for(size_t i = 0; i < allTasksViews.size(); ++i) {
        j[i] = toCrowJson(allTasksViews[i]);
    }
    return j;
}

int main() {

    crow::SimpleApp app;
    TaskManager taskManager(2);
    CommandFactory commandFactory(taskManager);
    Controller controller;


    CROW_ROUTE(app, "/taches")
    .methods("POST"_method, "GET"_method)
    ([&commandFactory, &controller](const crow::request& req ){
        crow::json::wvalue response;
        if(req.method == "POST"_method) {
            std::string description;
            int duration;
            json reqData =  json::parse(req.body);
            reqData["description"].get_to(description);
            reqData["duration"].get_to(duration);

            auto command = commandFactory.create<CreateTaskCommand>(description, duration);
            controller.addCommand(command);
            command->waitToBeExecuted();

            auto createTaskCommand = std::dynamic_pointer_cast<CreateTaskCommand>(command);
            auto taskView = createTaskCommand->getTaskView();

            
            if(taskView.id.empty()) {
                response["error"] = "Couldn't create a new task";
            } else {
                response = toCrowJson(taskView);
            }
        }
        
        if(req.method == "GET"_method) {
            auto command = commandFactory.create<GetAllTasksCommand>();
            controller.addCommand(command);
            command->waitToBeExecuted();

            auto getAllTasksCommand = std::dynamic_pointer_cast<GetAllTasksCommand>(command);
            response = toCrowJson(getAllTasksCommand->getTaskViews());
        }
        
        return response;
    });


    CROW_ROUTE(app,"/taches/<string>")
    .methods("GET"_method)
    ([&commandFactory, &controller](std::string id){
        auto command = commandFactory.create<GetTaskCommand>(id);
        controller.addCommand(command);
        command->waitToBeExecuted();

        auto getTaskCommand = std::dynamic_pointer_cast<GetTaskCommand>(command);
        if(getTaskCommand->noTaskFound()) {
            crow::json::wvalue response;
            response["error"] = "Task not found";
            return response;
        }
        return toCrowJson(getTaskCommand->getTaskView());
     });

    CROW_ROUTE(app, "/taches/<string>")
    .methods("DELETE"_method)
    ([&commandFactory, &controller](std::string id){
        auto command = commandFactory.create<CancelTaskCommand>(id);
        controller.addCommand(command);
        command->waitToBeExecuted();
        
        auto cancelTaskCommand = std::dynamic_pointer_cast<CancelTaskCommand>(command);
        crow::json::wvalue response;
        if(cancelTaskCommand->getStatus() == CancelTaskCommand::Status::Canceled) {
            response["message"] = "Task canceled";
        } else {
            response["error"] = "Task not found";
        }
        return response;
    });

    auto var = app.port(3000).multithreaded().run_async();
    controller.run();
    return 0;
}