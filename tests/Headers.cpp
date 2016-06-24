#include <boost/test/unit_test.hpp>
#include "Headers.hpp"

using namespace http;

BOOST_AUTO_TEST_SUITE(TestHeaders)
BOOST_AUTO_TEST_CASE(content_type)
{
    Headers headers;
    BOOST_CHECK(!headers.has("Content-Type"));
    BOOST_CHECK_EQUAL("", headers.get("Content-Type"));
    
    headers.content_type("text/html");
    BOOST_CHECK(headers.has("Content-Type"));
    BOOST_CHECK_EQUAL("text/html", headers.get("Content-Type"));
    BOOST_CHECK_EQUAL("text/html", headers.content_type().mime);
    BOOST_CHECK_EQUAL("", headers.content_type().charset);

    headers.content_type("text/html", "utf8");
    BOOST_CHECK_EQUAL("text/html; charset=utf8", headers.get("Content-Type"));
    BOOST_CHECK_EQUAL("text/html", headers.content_type().mime);
    BOOST_CHECK_EQUAL("utf8", headers.content_type().charset);
}
BOOST_AUTO_TEST_SUITE_END()
