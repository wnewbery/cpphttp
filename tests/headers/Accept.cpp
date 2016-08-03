#include <boost/test/unit_test.hpp>
#include "headers/Accept.hpp"
#include <stdexcept>
using namespace http;

BOOST_AUTO_TEST_SUITE(TestHeadersAccept)
BOOST_AUTO_TEST_CASE(parse_single)
{
    Accept accept;
    Accept::Type type;

    //media-range
    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*"));
    BOOST_REQUIRE_EQUAL(1, accept.accepts().size());
    type = accept.accepts()[0];
    BOOST_CHECK_EQUAL("*", type.type);
    BOOST_CHECK_EQUAL("*", type.subtype);
    BOOST_CHECK_EQUAL(1, type.quality);


    BOOST_REQUIRE_NO_THROW(accept = Accept("text/*"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL("text", type.type);
    BOOST_CHECK_EQUAL("*", type.subtype);
    BOOST_CHECK_EQUAL(1, type.quality);


    BOOST_REQUIRE_NO_THROW(accept = Accept("text/plain"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL("text", type.type);
    BOOST_CHECK_EQUAL("plain", type.subtype);
    BOOST_CHECK_EQUAL(1, type.quality);

    //quality
    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;q=0"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL("*", type.type);
    BOOST_CHECK_EQUAL("*", type.subtype);
    BOOST_CHECK_EQUAL(0, type.quality);

    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;q=0."));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL(0, type.quality);

    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;q=0.000"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL(0, type.quality);

    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;q=0.123"));
    type = accept.accepts().at(0);
    BOOST_CHECK_CLOSE(0.123, type.quality, 0.00001);

    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;q=1"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL(1, type.quality);

    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;q=1.000"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL(1, type.quality);

    //media type parameters (currently ignored)
    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;charset=utf-8;q=1"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL(1, type.quality);

    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;charset=\"quoted\\\"string\";q=1"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL(1, type.quality);

    //accept-ext parameters (currently ignored)
    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;q=1;ext=5"));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL(1, type.quality);

    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*;q=1;ext=\"quoted\\\"string\""));
    type = accept.accepts().at(0);
    BOOST_CHECK_EQUAL(1, type.quality);
}

BOOST_AUTO_TEST_CASE(parse_list)
{
    Accept accept;

    BOOST_REQUIRE_NO_THROW(accept = Accept("*/*,,text/html , application/json,"));
    BOOST_REQUIRE_EQUAL(3, accept.accepts().size());
    BOOST_CHECK_EQUAL("*", accept.accepts()[0].type);
    BOOST_CHECK_EQUAL("*", accept.accepts()[0].subtype);

    BOOST_CHECK_EQUAL("text", accept.accepts()[1].type);
    BOOST_CHECK_EQUAL("html", accept.accepts()[1].subtype);

    BOOST_CHECK_EQUAL("application", accept.accepts()[2].type);
    BOOST_CHECK_EQUAL("json", accept.accepts()[2].subtype);
}

BOOST_AUTO_TEST_CASE(invalid_media_range)
{
    BOOST_CHECK_THROW(Accept("*/plain"), ParserError);
    BOOST_CHECK_THROW(Accept("/plain"), ParserError);
    BOOST_CHECK_THROW(Accept("text/"), ParserError);
    BOOST_CHECK_THROW(Accept("text/*plain"), ParserError);
    BOOST_CHECK_THROW(Accept("text-plain"), ParserError);
    BOOST_CHECK_THROW(Accept("text-plain"), ParserError);
    BOOST_CHECK_THROW(Accept("invalid / space"), ParserError);
    BOOST_CHECK_THROW(Accept("invalid@token/plain"), ParserError);
}

BOOST_AUTO_TEST_CASE(invalid_quality)
{
    BOOST_CHECK_THROW(Accept("*/*;q=1.0000"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;q=1.500"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;q=0.0000"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;q=05"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;q=x.0"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;q=1.x"), ParserError);
}

BOOST_AUTO_TEST_CASE(invalid_media_param)
{
    BOOST_CHECK_THROW(Accept("*/*;;charset=utf-8"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;charset="), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;charset=\"unmatched"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;charset=unmatched\""), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;charset=invalid\ntoken"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;charset= utf-8"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;charset =utf-8"), ParserError);
}

BOOST_AUTO_TEST_CASE(invalid_accept_ext)
{
    BOOST_CHECK_THROW(Accept("*/*;q=1;;ext=1"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;q=1;invalid@token=1"), ParserError);
    BOOST_CHECK_THROW(Accept("*/*;q=1;ext=invalid@token"), ParserError);
}

BOOST_AUTO_TEST_CASE(accepts)
{
    BOOST_CHECK(Accept("*/*").accepts("text/plain"));
    BOOST_CHECK(Accept("text/*").accepts("text/plain"));
    BOOST_CHECK(!Accept("text/*").accepts("application/json"));
    BOOST_CHECK(Accept("text/plain").accepts("text/plain"));
    BOOST_CHECK(!Accept("text/plain").accepts("text/html"));

    BOOST_CHECK(Accept("text/plain, text/html").accepts("text/plain"));
    BOOST_CHECK(Accept("text/plain, text/html").accepts("text/html"));
    BOOST_CHECK(!Accept("text/plain, text/html").accepts("text/xml"));

    BOOST_CHECK_NO_THROW(Accept("text/plain").check_accepts("text/plain"));
    BOOST_CHECK_THROW(Accept("text/plain").check_accepts("text/html"), NotAcceptable);
}

BOOST_AUTO_TEST_CASE(preferred)
{
    BOOST_CHECK_EQUAL("text/html", Accept("text/html").preferred({ "text/html" }));
    BOOST_CHECK_EQUAL("text/html", Accept("text/*").preferred({ "text/html" }));

    BOOST_CHECK_EQUAL("text/html", Accept("text/html, */*").preferred({ "text/plain", "text/html" }));
    BOOST_CHECK_EQUAL("text/html", Accept("*/*, text/html").preferred({ "text/plain", "text/html" }));

    BOOST_CHECK_EQUAL("image/png", Accept("*/*, image/*").preferred({ "text/plain", "image/png" }));

    BOOST_CHECK_EQUAL("text/plain", Accept("text/html, */*;q=0.5").preferred({ "text/plain", "image/png" }));

    BOOST_CHECK_EQUAL("text/plain", Accept("text/*, image/*").preferred({ "text/plain", "text/html", "image/png" }));
    BOOST_CHECK_EQUAL("image/png", Accept("text/*;q=0.5, image/*").preferred({ "text/plain", "text/html", "image/png" }));


    BOOST_CHECK_THROW(Accept("text/xml+svg, image/*").preferred({ "text/plain", "text/html" }), NotAcceptable);
}
BOOST_AUTO_TEST_SUITE_END()
