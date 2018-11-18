#include <string>

namespace cnet {
    class NotFound {
    public:
        NotFound() {};

        virtual ~NotFound() {};

        virtual std::string genText();
    };
}