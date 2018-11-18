#include <iostream>
#include <cnet/cnet.h>
#include <vector>
#include <string>

using namespace cnet;
class Test : public HttpController<Test>
{
//  public:
//    METHOD_LIST_BEGIN
//    METHOD_ADD(Test::get, "get/{2}/{1}", Get); //path will be /api/v1/test/get/{arg2}/{arg1}
//    METHOD_LIST_END
//    void get(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback, int p1, int p2) const
//    {
//        auto resp = HttpResponse::newHttpResponse();
//        resp->setBody("<p>Hello, world!</p>");
//        resp->setExpiredTime(0);
//        callback(resp);
//    }
};

int main()
{
    std::cout << banner << std::endl;
//
//    HttpApp::instance().addListener("0.0.0.0", 8080);
//    HttpApp::instance().setThreadNum(4);
//    HttpApp::instance().run();
}
