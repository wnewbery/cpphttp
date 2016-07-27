#include <boost/test/unit_test.hpp>
#include "core/Parser.hpp"

using namespace http;
using namespace http::parser;

BOOST_AUTO_TEST_SUITE(TestCoreParser)

template<typename T> std::string read_str(std::string str, T f)
{
    auto end = str.c_str() + str.size();
    auto p = f(str.c_str(), end);
    if (!p) return "null";
    else return std::string(p, end);
}

BOOST_AUTO_TEST_CASE(test_request_parser_get)
{
    RequestParser parser;
    auto read_str = [&parser](std::string str) -> std::string
    {
        auto p = parser.read(str.c_str(), str.c_str() + str.size());
        BOOST_REQUIRE(p >= str.c_str());
        BOOST_REQUIRE(p <= str.c_str() + str.size());
        return {p, str.c_str() + str.size()};
    };

    BOOST_CHECK_EQUAL("GET /", read_str("GET /"));
    BOOST_CHECK_EQUAL(RequestParser::START, parser.state());

    BOOST_CHECK_EQUAL("Host:", read_str("GET /index.html HTTP/1.1\r\nHost:"));
    BOOST_CHECK_EQUAL(RequestParser::HEADERS, parser.state());
    BOOST_CHECK_EQUAL("GET", parser.method());
    BOOST_CHECK_EQUAL("/index.html", parser.uri());

    BOOST_CHECK_EQUAL("Host: localhost", read_str("Host: localhost"));
    BOOST_CHECK_EQUAL(0, parser.headers().size());

    BOOST_CHECK_EQUAL("", read_str("Host: localhost\r\nAccept: \t*/*  \r\n"));
    BOOST_CHECK_EQUAL(RequestParser::HEADERS, parser.state());
    BOOST_CHECK_EQUAL("localhost", parser.headers().get("Host"));
    BOOST_CHECK_EQUAL("*/*", parser.headers().get("Accept"));

    BOOST_CHECK_EQUAL("POST /next", read_str("\r\nPOST /next"));
    BOOST_CHECK_EQUAL(RequestParser::COMPLETED, parser.state());
    BOOST_CHECK(parser.is_completed());
    BOOST_CHECK(parser.has_content_length());
    BOOST_CHECK_EQUAL(0, parser.content_length());
    BOOST_CHECK_EQUAL("", parser.body());
}

BOOST_AUTO_TEST_CASE(test_request_parser_post)
{
    RequestParser parser;
    auto read_str = [&parser](std::string str) -> std::string
    {
        auto p = parser.read(str.c_str(), str.c_str() + str.size());
        BOOST_REQUIRE(p >= str.c_str());
        BOOST_REQUIRE(p <= str.c_str() + str.size());
        return{ p, str.c_str() + str.size() };
    };


    BOOST_CHECK_EQUAL("", read_str("POST /test HTTP/1.1\r\nContent-Length: 10\r\n\r\n"));
    BOOST_CHECK_EQUAL(RequestParser::BODY, parser.state());
    BOOST_CHECK_EQUAL(true, parser.has_content_length());
    BOOST_CHECK_EQUAL(10, parser.content_length());

    BOOST_CHECK_EQUAL("GET /next", read_str("0123456789GET /next"));
    BOOST_CHECK(parser.is_completed());
    BOOST_CHECK(parser.has_content_length());
    BOOST_CHECK_EQUAL(10, parser.content_length());
    BOOST_CHECK_EQUAL("0123456789", parser.body());


    parser.reset();
    BOOST_CHECK(!parser.is_completed());
    BOOST_CHECK_EQUAL("", read_str("POST /x HTTP/1.1\r\n\r\n"));
    BOOST_CHECK(parser.is_completed());
    BOOST_CHECK_EQUAL(0, parser.content_length());
    BOOST_CHECK_EQUAL("", parser.body());
}

BOOST_AUTO_TEST_CASE(test_request_parser_post_split)
{
    RequestParser parser;
    auto read_str = [&parser](std::string str) -> std::string
    {
        auto p = parser.read(str.c_str(), str.c_str() + str.size());
        BOOST_REQUIRE(p >= str.c_str());
        BOOST_REQUIRE(p <= str.c_str() + str.size());
        return{ p, str.c_str() + str.size() };
    };


    BOOST_CHECK_EQUAL("", read_str("POST /test HTTP/1.1\r\nContent-Length: 10\r\n\r\n"));
    BOOST_CHECK_EQUAL(RequestParser::BODY, parser.state());
    BOOST_CHECK_EQUAL(true, parser.has_content_length());
    BOOST_CHECK_EQUAL(10, parser.content_length());

    BOOST_CHECK_EQUAL("", read_str("012345"));
    BOOST_CHECK(!parser.is_completed());
    BOOST_CHECK_EQUAL("GET /next", read_str("6789GET /next"));
    BOOST_CHECK(parser.is_completed());
    BOOST_CHECK_EQUAL(10, parser.content_length());
    BOOST_CHECK_EQUAL("0123456789", parser.body());
}

BOOST_AUTO_TEST_CASE(test_chunked)
{
    RequestParser parser;
    auto read_str = [&parser](std::string str) -> std::string
    {
        auto p = parser.read(str.c_str(), str.c_str() + str.size());
        BOOST_REQUIRE(p >= str.c_str());
        BOOST_REQUIRE(p <= str.c_str() + str.size());
        return{ p, str.c_str() + str.size() };
    };

    BOOST_CHECK_EQUAL("", read_str(
        "POST /test HTTP/1.1\r\n"
        "Trailer: Expires\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"));
    BOOST_CHECK_EQUAL(RequestParser::BODY_CHUNK_LEN, parser.state());

    BOOST_CHECK_EQUAL("", read_str("5\r\n"));
    BOOST_CHECK_EQUAL(RequestParser::BODY_CHUNK, parser.state());

    BOOST_CHECK_EQUAL("", read_str("0123"));
    BOOST_CHECK_EQUAL(RequestParser::BODY_CHUNK, parser.state());

    BOOST_CHECK_EQUAL("", read_str("4"));
    BOOST_CHECK_EQUAL(RequestParser::BODY_CHUNK_TERMINATOR, parser.state());

    BOOST_CHECK_EQUAL("", read_str("\r\n"));
    BOOST_CHECK_EQUAL(RequestParser::BODY_CHUNK_LEN, parser.state());

    BOOST_CHECK_EQUAL("", read_str("0\r\n"));
    BOOST_CHECK_EQUAL(RequestParser::TRAILER_HEADERS, parser.state());

    BOOST_CHECK_EQUAL("", read_str("\r\n"));
    BOOST_CHECK(parser.is_completed());
    BOOST_CHECK_EQUAL(5, parser.content_length());
    BOOST_CHECK_EQUAL("01234", parser.body());

    parser.reset();
    BOOST_CHECK_EQUAL("", read_str(
        "POST /test HTTP/1.1\r\n"
        "Trailer: Expires\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n01234\r\n"
        "0\r\n"
        "\r\n"));
    BOOST_CHECK_EQUAL(5, parser.content_length());
    BOOST_CHECK_EQUAL("01234", parser.body());
}

BOOST_AUTO_TEST_CASE(test_chunked_trailers)
{
    RequestParser parser;
    auto read_str = [&parser](std::string str) -> std::string
    {
        auto p = parser.read(str.c_str(), str.c_str() + str.size());
        BOOST_REQUIRE(p >= str.c_str());
        BOOST_REQUIRE(p <= str.c_str() + str.size());
        return{ p, str.c_str() + str.size() };
    };
    BOOST_CHECK_EQUAL("", read_str(
        "POST /test HTTP/1.1\r\n"
        "Trailer: Expires\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\n01234\r\n"
        "0\r\n"
        "Expires: now\r\n"
        "\r\n"));
    BOOST_CHECK_EQUAL("now", parser.headers().get("Expires"));
    BOOST_CHECK_EQUAL(5, parser.content_length());
    BOOST_CHECK_EQUAL("01234", parser.body());
}

BOOST_AUTO_TEST_CASE(test_request_parser_errors)
{
    RequestParser parser;
    auto read_str = [&parser](std::string str) -> std::string
    {
        auto p = parser.read(str.c_str(), str.c_str() + str.size());
        BOOST_REQUIRE(p >= str.c_str());
        BOOST_REQUIRE(p <= str.c_str() + str.size());
        return{ p, str.c_str() + str.size() };
    };

    parser.reset();
    BOOST_CHECK_THROW(read_str("POST /x HTTP/1.1\r\nContent-Length: A\r\n\r\n0123456789"), ParserError);

    parser.reset();
    BOOST_CHECK_THROW(read_str("POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n----\r\n"), ParserError);

    parser.reset();
    BOOST_CHECK_THROW(read_str("POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n1000000000\r\n"), ParserError);

    parser.reset();
    BOOST_CHECK_THROW(read_str("POST /x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n5\r\n01234X\r\n"), ParserError);
    parser.reset();
    BOOST_CHECK_THROW(read_str("POST /x HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n"), ParserError);
}


BOOST_AUTO_TEST_CASE(test_response_parser) //only considers differences
{
    ResponseParser parser;
    auto read_str = [&parser](std::string str) -> std::string
    {
        auto p = parser.read(str.c_str(), str.c_str() + str.size());
        BOOST_REQUIRE(p >= str.c_str());
        BOOST_REQUIRE(p <= str.c_str() + str.size());
        return{ p, str.c_str() + str.size() };
    };

    parser.reset(GET);
    BOOST_CHECK_EQUAL("Next", read_str("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n12345Next"));
    BOOST_CHECK(parser.is_completed());
    BOOST_CHECK_EQUAL(5, parser.content_length());
    BOOST_CHECK_EQUAL("12345", parser.body());

    parser.reset(HEAD);
    BOOST_CHECK_EQUAL("Next", read_str("HTTP/1.1 200 OK\r\nContent-Length: 50\r\n\r\nNext"));
    BOOST_CHECK(parser.is_completed());

    parser.reset(CONNECT);
    BOOST_CHECK_EQUAL("Next", read_str("HTTP/1.1 100 OK\r\nContent-Length: 50\r\n\r\nNext"));
    BOOST_CHECK(parser.is_completed());

    parser.reset(GET);
    BOOST_CHECK_EQUAL("Next", read_str("HTTP/1.1 100 OK\r\n\r\nNext"));
    BOOST_CHECK(parser.is_completed());

    parser.reset(GET);
    BOOST_CHECK_EQUAL("Next", read_str("HTTP/1.1 204 OK\r\n\r\nNext"));
    BOOST_CHECK(parser.is_completed());

    parser.reset(GET);
    BOOST_CHECK_EQUAL("Next", read_str("HTTP/1.1 304 OK\r\n\r\nNext"));
    BOOST_CHECK(parser.is_completed());

    parser.reset(CONNECT);
    BOOST_CHECK_EQUAL("Next", read_str("HTTP/1.1 404 OK\r\nContent-Length: 5\r\n\r\n12345Next"));
    BOOST_CHECK(parser.is_completed());
}

BOOST_AUTO_TEST_SUITE_END()
