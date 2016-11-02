#include <boost/test/unit_test.hpp>
#include "server/Router.hpp"
#include "Error.hpp"
#include "Response.hpp"
using namespace http;

BOOST_AUTO_TEST_SUITE(TestRouter)

class TestHandler : public RequestHandler
{
public:
    virtual Response handle(Request&, PathParams&)override
    {
        return{};
    }
};

BOOST_AUTO_TEST_CASE(simple)
{
    Router router;

    auto handler = std::make_shared<TestHandler>();
    auto handler2 = std::make_shared<TestHandler>();
    auto handler3 = std::make_shared<TestHandler>();
    auto handler4 = std::make_shared<TestHandler>();
    BOOST_CHECK_NO_THROW(router.add("GET", "/", handler));
    BOOST_CHECK_NO_THROW(router.add("GET", "/index.html", handler));
    BOOST_CHECK_NO_THROW(router.add("GET", "/login", handler2));
    BOOST_CHECK_NO_THROW(router.add("GET", "/login/", handler3));
    BOOST_CHECK_NO_THROW(router.add("POST", "/login", handler4));

    BOOST_CHECK(handler.get() == router.get("GET", "/index.html").handler);
    BOOST_CHECK(handler.get() == router.get("GET", "/").handler);
    BOOST_CHECK(handler2.get() == router.get("GET", "/login").handler);
    BOOST_CHECK(handler3.get() == router.get("GET", "/login/").handler);
    BOOST_CHECK(handler4.get() == router.get("POST", "/login").handler);
    // Extra slash not significant
    BOOST_CHECK(handler.get() == router.get("GET", "//index.html").handler);

    // Found path, but not method
    BOOST_CHECK_THROW(router.get("POST", "/index.html"), MethodNotAllowed);
    // Not found
    BOOST_CHECK(!router.get("GET", "/not_found"));
    BOOST_CHECK(!router.get("POST", "/not_found"));
    // Is case sensitive
    BOOST_CHECK(!router.get("GET", "/Index.html"));

    // Duplicate not allowed
    BOOST_CHECK_THROW(router.add("GET", "/index.html", handler), InvalidRouteError);
    // Invalid
    BOOST_CHECK_THROW(router.add("GET", "", handler), UrlError);
}

BOOST_AUTO_TEST_CASE(prefix)
{
    Router router;
    auto handler = std::make_shared<TestHandler>();
    BOOST_CHECK_NO_THROW(router.add("GET", "/assets/*", handler));

    BOOST_CHECK(handler.get() == router.get("GET", "/assets/").handler);
    BOOST_CHECK(handler.get() == router.get("GET", "/assets/application.js").handler);
    BOOST_CHECK(handler.get() == router.get("GET", "/assets/dark/application.css").handler);

    BOOST_CHECK_THROW(router.get("POST", "/assets/hack.js"), MethodNotAllowed);

    BOOST_CHECK(!router.get("GET", "/index"));

    BOOST_CHECK_THROW(router.add("GET", "/assets/other", handler), InvalidRouteError);
    BOOST_CHECK_NO_THROW(router.add("GET", "/other/child", handler));
    BOOST_CHECK_THROW(router.add("GET", "/other/*", handler), InvalidRouteError);
}

BOOST_AUTO_TEST_CASE(path_params)
{
    Router router;
    auto handler = std::make_shared<TestHandler>();
    BOOST_CHECK_NO_THROW(router.add("GET", "/forums/:forum_name", handler));
    BOOST_CHECK_NO_THROW(router.add("GET", "/forums/:forum_name/post", handler));
    BOOST_CHECK_NO_THROW(router.add("GET", "/forums/:forum_name/topics/:topic_id", handler));
    BOOST_CHECK_NO_THROW(router.add("GET", "/forums/:forum_name/topics/:topic_id/post", handler));

    auto matched = router.get("GET", "/forums/General");
    BOOST_CHECK(matched);
    BOOST_CHECK_EQUAL("General", matched.path_params["forum_name"]);

    matched = router.get("GET", "/forums/General/topics/567/post");
    BOOST_CHECK(matched);
    BOOST_CHECK_EQUAL("General", matched.path_params["forum_name"]);
    BOOST_CHECK_EQUAL("567", matched.path_params["topic_id"]);


    BOOST_CHECK_THROW(router.add("GET", "/forums/:forum_name2/unread", handler), InvalidRouteError);
}

BOOST_AUTO_TEST_SUITE_END()
