/*
Tester for the HTTP web server
*/

#include <curl/curl.h>

#include <chrono>
#include <thread>
#include <iostream>
#include <string>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
    (static_cast<std::string*>(userp))->append(contents, size * nmemb);
    return size * nmemb;
}

class Client  {
public:
    Client(){ 
        curl_global_init(CURL_GLOBAL_ALL);
    }
    json makeCreateTaskRequest(const std::string& description, int duration) {
        m_response.clear();  
        CURL* handle = curl_easy_init();
        
        struct curl_slist* slist;
        slist = NULL;
        slist = curl_slist_append(slist, "Content-Type: application/json");
        
        curl_easy_setopt(handle, CURLOPT_URL, "http://localhost:3000/taches");
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, slist);

        // json j;
        // j["description"] = description;
        // j["duration"] = duration;
        //curl_easy_setopt(handle, CURLOPT_POSTFIELDS, j.dump().c_str());

        //Manual string used because the output of j.dump() above doesn't work
        std::string manualString = "{\"description\":\"" + description + "\",\"duration\":" + std::to_string(duration) + "}";
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, manualString.c_str());
        
        //curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &m_response);
        curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        return json::parse(m_response);
    }

    json makeGetTaskRequest(const std::string& id) {
        m_response.clear();
        CURL* handle = curl_easy_init();
        std::string url = "http://localhost:3000/taches";
        url.append("/" + id);
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &m_response);
        curl_easy_perform(handle);
        std::cout << "Response is " << m_response << std::endl;
        curl_easy_cleanup(handle);
        return json::parse(m_response);
    }

    json makeGetAllTasksRequest() {
        m_response.clear();
        CURL* handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_URL, "http://localhost:3000/taches");
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &m_response);
        curl_easy_perform(handle);
        std::cout << "Response is " << m_response << std::endl;
        curl_easy_cleanup(handle);
        return json::parse(m_response);
    }

    json makeCancelRequest(const std::string& id) {
        m_response.clear();
        CURL* handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
        std::string url = "http://localhost:3000/taches";
        url.append("/" + id);
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &m_response);
        curl_easy_perform(handle);
        std::cout << "Response is " << m_response << std::endl;
        curl_easy_cleanup(handle);
        return json::parse(m_response);
    }

    ~Client(){
        curl_global_cleanup();
    }
private:
    CURL* m_curl;
    std::string m_response;
};

void log(const std::string& msg) {
    std::cout << msg << std::endl;
}

void wait(int msCount) {
    std::this_thread::sleep_for(std::chrono::milliseconds(msCount));
}

std::string evaluate(const json& response, std::function<bool(const json&)> predicate) {
    return predicate(response) ? "PASS" : "FAIL"; 
}

int main() {
Client cl;

log("Creating the first task...");
std::string description = "simple post";
int duration = 1000;
auto response = cl.makeCreateTaskRequest(description, duration);

log("[TEST] Response should contain id of created task:");
log(evaluate(response, [](const json& resp) {
        return !resp["id"].empty() && (resp["status"] == "Waiting" || resp["status"] == "Running");
    }));
log("");

log("Making a get task request...");
std::string id = response["id"];
response = cl.makeGetTaskRequest(id);
log("[TEST] Should be able to get existing task:");
log(response["id"] == id ? "PASS" : "FAIL");
log("");

log("Waiting for task to start running...");
wait(100);
log("[TEST] Task should have started running:");
log(evaluate(cl.makeGetTaskRequest(id), 
            [](const json& resp){ return resp["status"] == "Running"; }));
log("");

log("Waiting for task to finish...");
wait(1000);
log("[TEST] Task should have finished:");
log(evaluate(cl.makeGetTaskRequest(id), 
            [](const json& resp){ return resp["status"] == "Finished"; }));
log("");

log("Creating a new task to test DELETE endpoint...");
response = cl.makeCreateTaskRequest(description, duration);
log("Waiting task to start running...");
wait(100);
log("Sending cancel request...");
log("[TEST] Should receive task cancelled response:");
log(evaluate(cl.makeCancelRequest(response["id"]), 
            [](const json& resp){ return resp["message"] == "Task canceled"; }));
log("");

log("Trying to cancel non-existent task...");
log("[TEST] Should receive task not found error:");
log(evaluate(cl.makeCancelRequest("non-existent-id"), 
            [](const json& resp){ return resp["error"] == "Task not found"; }));
log("");

log("Sending get all tasks request...");
log("[TEST] Should receive the information for both tasks:"); 
log(evaluate(cl.makeGetAllTasksRequest(), [](const json& resp){ return resp.size()== 2; } ));
log("");

// // Test task multithreading

// log("Creating 3 tasks to occupy both worker threads and have a waiting third task...");

// std::vector<std::string> taskIds;
// for(int i = 0; i < 3; ++i) {
//     description = "Task " + std::to_string(i);
//     response = cl.makeCreateTaskRequest(description, duration);
//     getTaskResponseJson = json::parse(cl.makeCancelRequest(id));
// }

return 0;
}


