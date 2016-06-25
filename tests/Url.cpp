#include <boost/test/unit_test.hpp>
#include "Url.hpp"


BOOST_AUTO_TEST_SUITE(TestUrl)
BOOST_AUTO_TEST_CASE(url_decode)
{
    BOOST_CHECK_EQUAL("test", http::url_decode("test"));
    BOOST_CHECK_EQUAL("test test", http::url_decode("test%20test"));
    BOOST_CHECK_EQUAL("test test test", http::url_decode("test%20test%20test"));
    BOOST_CHECK_EQUAL("test+test", http::url_decode("test+test"));
    BOOST_CHECK_EQUAL("test ", http::url_decode("test%20"));
    BOOST_CHECK_EQUAL(" test", http::url_decode("%20test"));
    BOOST_CHECK_EQUAL("test%2", http::url_decode("test%2"));

    BOOST_CHECK_EQUAL("test test", http::url_decode_query("test+test"));

    BOOST_CHECK_THROW(http::url_decode("%XX"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(url_parse_request)
{
    http::Url x;

    x = http::Url::parse_request("/index.html");
    BOOST_CHECK_EQUAL("/index.html", x.path);
    BOOST_CHECK(x.query_params.empty());


    x = http::Url::parse_request("/shopping%20list.html");
    BOOST_CHECK_EQUAL("/shopping list.html", x.path);

    x = http::Url::parse_request("/index.html?key=value");
    BOOST_CHECK_EQUAL("/index.html", x.path);
    BOOST_CHECK_EQUAL("value", x.query_param("key"));
    BOOST_CHECK_EQUAL("", x.query_param("not"));
    BOOST_CHECK_EQUAL(1, x.query_param_list("key").size());
    BOOST_CHECK_EQUAL(0, x.query_param_list("not").size());
    BOOST_CHECK_EQUAL("value", *x.query_param_list("key").begin());


    x = http::Url::parse_request("/index.html?key=value%3D5");
    BOOST_CHECK_EQUAL("value=5", x.query_param("key"));

    x = http::Url::parse_request("/index.html?key=value&key2=val2");
    BOOST_CHECK_EQUAL("value", x.query_param("key"));
    BOOST_CHECK_EQUAL("val2", x.query_param("key2"));

    x = http::Url::parse_request("/index.html?key&key2=val2");
    BOOST_CHECK(x.has_query_param("key"));
    BOOST_CHECK_EQUAL("", x.query_param("key"));
    BOOST_CHECK_EQUAL("val2", x.query_param("key2"));

    x = http::Url::parse_request("/index.html?key%5B%5D=a&key%5B%5D=b&key%5B%5D=c");
    BOOST_CHECK_EQUAL("a", x.query_param("key[]"));
    auto y = x.query_param_list("key[]");
    BOOST_CHECK_EQUAL(3, y.size());
    std::string values[] = {"a", "b", "c"};
    BOOST_CHECK_EQUAL_COLLECTIONS(y.begin(), y.end(), values, values + 3);
}

BOOST_AUTO_TEST_SUITE_END()
