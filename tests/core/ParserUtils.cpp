#include <boost/test/unit_test.hpp>
#include "core/ParserUtils.hpp"

using namespace http;
using namespace http::parser;

BOOST_AUTO_TEST_SUITE(TestCoreParserUtils)

template<typename T> std::string read_str(std::string str, T f)
{
    auto end = str.c_str() + str.size();
    auto p = f(str.c_str(), end);
    if (!p) return "null";
    else return std::string(p, end);
}

BOOST_AUTO_TEST_CASE(test_find_newline)
{
    auto f = [](std::string str) { return read_str(str, find_newline); };
    BOOST_CHECK_EQUAL("null", f(""));
    BOOST_CHECK_EQUAL("null", f("testing"));
    BOOST_CHECK_EQUAL("null", f("\r"));
    BOOST_CHECK_EQUAL("null", f("test\r"));

    BOOST_CHECK_EQUAL("\r\n", f("\r\n"));
    BOOST_CHECK_EQUAL("\r\n", f(" testing\r\n"));
    BOOST_CHECK_EQUAL("\r\n", f(" test\ring\r\n"));
    BOOST_CHECK_EQUAL("\r\n", f(" test\ning\r\n"));
    BOOST_CHECK_EQUAL("\r\ntest\r\n", f(" testing\r\ntest\r\n"));
    BOOST_CHECK_EQUAL("\r\n\r\ntest\r\n", f(" testing\r\n\r\ntest\r\n"));
    BOOST_CHECK_EQUAL("\r\n\r\ntest\r\n", f(" testing\r\n\r\ntest\r\n"));
}

BOOST_AUTO_TEST_CASE(test_read_method)
{
    auto f = [](std::string str) { return read_str(str, read_method); };
    BOOST_CHECK_EQUAL(" ", f("GET "));
    BOOST_CHECK_EQUAL(" /icon.png", f("GET /icon.png"));
    BOOST_CHECK_EQUAL(" ", f("CUSTOM_METHOD "));

    BOOST_CHECK_THROW(f(""), ParserError);
    BOOST_CHECK_THROW(f("GET"), ParserError);
    BOOST_CHECK_THROW(f("GET\r\n"), ParserError);
    BOOST_CHECK_THROW(f("INVALID{} "), ParserError);
}

BOOST_AUTO_TEST_CASE(test_read_uri)
{
    auto f = [](std::string str) { return read_str(str, read_uri); };
    BOOST_CHECK_EQUAL(" ", f("/ "));
    BOOST_CHECK_EQUAL(" HTTP/1.1", f("/icon.png HTTP/1.1"));
    BOOST_CHECK_EQUAL(" ", f("/icons?types=svg%20png "));

    BOOST_CHECK_THROW(f(""), ParserError);
    BOOST_CHECK_THROW(f(" "), ParserError);
    BOOST_CHECK_THROW(f("\r\n"), ParserError);
}

template<typename T> std::string read_version_str(std::string str, Version *out, T f)
{
    auto end = str.c_str() + str.size();
    auto p = f(str.c_str(), end, out);
    if (!p) return "null";
    else return std::string(p, end);
}
BOOST_AUTO_TEST_CASE(test_read_version)
{
    Version v;
    auto f = [&v](std::string str) { return read_version_str(str, &v, read_version); };

    BOOST_CHECK_EQUAL("", f("HTTP/1.1"));
    BOOST_CHECK_EQUAL(1, v.major);
    BOOST_CHECK_EQUAL(1, v.minor);

    BOOST_CHECK_EQUAL("\r\n", f("HTTP/1.0\r\n"));
    BOOST_CHECK_EQUAL(1, v.major);
    BOOST_CHECK_EQUAL(0, v.minor);

    BOOST_CHECK_EQUAL(" ", f("HTTP/10.11 "));
    BOOST_CHECK_EQUAL(10, v.major);
    BOOST_CHECK_EQUAL(11, v.minor);

    BOOST_CHECK_THROW(f(""), ParserError);
    BOOST_CHECK_THROW(f("HTTP/"), ParserError);
    BOOST_CHECK_THROW(f("HTTP/1"), ParserError);
    BOOST_CHECK_THROW(f("HTTP/1."), ParserError);
    BOOST_CHECK_THROW(f("HxTP/1.1"), ParserError);
    BOOST_CHECK_THROW(f("HTTP/x.1"), ParserError);
    BOOST_CHECK_THROW(f("HTTP/1x.1"), ParserError);
    BOOST_CHECK_THROW(f("HTTP/1x1"), ParserError);
    BOOST_CHECK_THROW(f("HTTP/1.x"), ParserError);

    BOOST_CHECK_THROW(read_version_str("HTTP/1.1 ", &v, read_request_version), ParserError);
    BOOST_CHECK_NO_THROW(read_version_str("HTTP/1.1", &v, read_request_version));
    BOOST_CHECK_NO_THROW(read_version_str("HTTP/1.1 ", &v, read_response_version));
    BOOST_CHECK_THROW(read_version_str("HTTP/1.1", &v, read_response_version), ParserError);
}

BOOST_AUTO_TEST_CASE(test_read_status_code)
{
    StatusCode code;
    auto f = [&code](std::string str) -> std::string
    {
        auto end = str.c_str() + str.size();
        auto p = read_status_code(str.c_str(), end, &code);
        if (!p) return "null";
        else return std::string(p, end);
    };

    BOOST_CHECK_EQUAL(" ", f("200 "));
    BOOST_CHECK_EQUAL(200, code.code);
    BOOST_CHECK_EQUAL(-1, code.extended);

    BOOST_CHECK_EQUAL(" ", f("500 "));
    BOOST_CHECK_EQUAL(500, code.code);
    BOOST_CHECK_EQUAL(-1, code.extended);

    BOOST_CHECK_EQUAL(" ", f("500.12 "));
    BOOST_CHECK_EQUAL(500, code.code);
    BOOST_CHECK_EQUAL(12, code.extended);

    BOOST_CHECK_THROW(f(""), ParserError);
    BOOST_CHECK_THROW(f(" "), ParserError);
    BOOST_CHECK_THROW(f("200"), ParserError);
    BOOST_CHECK_THROW(f("200."), ParserError);
    BOOST_CHECK_THROW(f("200.12"), ParserError);
}

BOOST_AUTO_TEST_CASE(test_read_status_phrase)
{
    auto f = [](std::string str) { read_status_phrase(str.c_str(), str.c_str() + str.size()); };
    BOOST_CHECK_NO_THROW(f(""));
    BOOST_CHECK_NO_THROW(f("Not Found"));

    BOOST_CHECK_THROW(f("Not\nFound"), ParserError);
}
BOOST_AUTO_TEST_SUITE_END()
