#include <boost/test/unit_test.hpp>
#include "server/Router.hpp"
#include "Error.hpp"
#include "Response.hpp"
using namespace http;

BOOST_AUTO_TEST_SUITE(TestRouter)

class TestHandler : public RequestHandler
{
public:
    virtual Response handle(Request &)override
    {
        return{};
    }
};

BOOST_AUTO_TEST_CASE(test)
{
    Router router;

    auto handler = std::make_shared<TestHandler>();
    BOOST_CHECK_NO_THROW(router.add("GET", "/index.html", handler));
    BOOST_CHECK(handler.get() == router.get_handler("GET", "/index.html"));
    BOOST_CHECK_THROW(router.get_handler("POST", "/index.html"), MethodNotAllowed);
    BOOST_CHECK_THROW(router.get_handler("GET", "/index.php"), NotFound);

    BOOST_CHECK_THROW(router.add("GET", "/index.html", handler), std::runtime_error);

    BOOST_CHECK_NO_THROW(router.add("POST", "/index.html", handler));
    BOOST_CHECK(handler.get() == router.get_handler("POST", "/index.html"));
}

BOOST_AUTO_TEST_SUITE_END()
