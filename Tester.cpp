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

class Test {
public:
    Test(const std::string& response) {
        m_response = json::parse(response);
    }
    
    void printResult() const {
        std::cout << "Test " + m_name + ": " + (evaluate() ? "PASS" : "FAIL") << std::endl;
    }

protected:
    virtual bool evaluate() const = 0;
    std::string m_name;
    json m_response;
};

class SimplePostTest : public Test {
public:
    SimplePostTest(const std::string& response, const std::string& description, int duration) 
    : Test(response), m_description(description), m_duration(duration) {
        m_name = "SimplePost";
    }
 
private:
    bool evaluate() const override {
        return !m_response["id"].empty()
                && m_response["description"] == m_description
                && m_response["duration"] == m_duration
                && m_response["status"] == "Waiting" ;
    }
    std::string m_description;
    int m_duration;
};

class Client  {
public:
    Client(){ 
        curl_global_init(CURL_GLOBAL_ALL);
    }
    std::string makeCreateTaskRequest(const std::string& description, int duration) {
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
        std::cout << manualString << std::endl;
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, manualString.c_str());
        
        //curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &m_response);
        curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        return m_response;
    }
    ~Client(){
        curl_global_cleanup();
    }
private:
    CURL* m_curl;
    std::string m_response;
};

int main() {
    Client cl;
    
    std::string description = "simple post";
    int duration = 1000;
    SimplePostTest t1(cl.makeCreateTaskRequest(description, duration), description, duration);
    t1.printResult();

    for(int i = 0; i < 10; ++i) {
        description = "Task " + std::to_string(i);
        cl.makeCreateTaskRequest(description, duration);
    }

    //std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;
}


