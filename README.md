# cnet

#include <iostream>
#include <cnet/cnet.h>
#include <vector>
#include <string>

using namespace cnet;

class Test : public HttpController<Test> {
public:
    METHOD_LIST_BEGIN
        METHOD_ADD(Test::get, "/get", Get); //http://127.0.0.1:8080/Test/get
    METHOD_LIST_END

    void get(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, int p1, int p2) const {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody("<p>Hello, world!</p>");
        resp->setExpiredTime(0);
        callback(resp);
    }
};

int main() {
    std::cout << banner << std::endl;
    Logger::setLogLevel(Logger::LogLevel::TRACE);
    app().addListener("127.0.0.1", 8080);
    app().setThreadNum(4);
    app().setDocumentRoot("./");
    app().enableSession(60);
    app().run();
}
