/*
Tester for the HTTP web server
*/

#include <curl/curl.h>
#include <iostream>
#include <string>

size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp)
{
    (static_cast<std::string*>(userp))->append(contents, size * nmemb);
    return size * nmemb;
}

int main() {
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* easyHandle = curl_easy_init();
    std::string responseBody;
    curl_easy_setopt(easyHandle, CURLOPT_URL, "http://localhost:3000/helloworld");
    curl_easy_setopt(easyHandle, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(easyHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(easyHandle, CURLOPT_WRITEDATA, &responseBody);

    curl_easy_perform(easyHandle);
    std::cout << responseBody << std::endl;
    return 0;
}


