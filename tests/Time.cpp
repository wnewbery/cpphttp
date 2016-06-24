#include <boost/test/unit_test.hpp>
#include "Time.hpp"
#include <stdexcept>

BOOST_AUTO_TEST_SUITE(TestTime)
BOOST_AUTO_TEST_CASE(test)
{
    BOOST_CHECK_EQUAL("Fri, 24 Jun 2016 09:47:55 GMT", http::format_time(1466761675));

    BOOST_CHECK_EQUAL(1466761675, http::parse_time("Fri, 24 Jun 2016 09:47:55 GMT"));
    BOOST_CHECK_EQUAL(1466761675, http::parse_time("Friday, 24-Jun-16 09:47:55 GMT"));
    BOOST_CHECK_EQUAL(1466761675, http::parse_time("Fri Jun 24 09:47:55 2016"));

    BOOST_CHECK_EQUAL(1466761675, http::parse_time("XXX, 24 Jun 2016 09:47:55 GMT"));

    BOOST_CHECK_THROW(http::parse_time(""), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 Jun 2016 09:47:55 PST"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 Jun 2016 09:47:55"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 Jun 2016 GMT"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 XXX 2016 09:47:55 GMT"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 40 Mar 2016 09:47:55 GMT"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 Jun 1899 09:47:55 GMT"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 Jun 4000 09:47:55 GMT"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 Jun 2016 30:47:55 GMT"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 Jun 2016 09:65:55 GMT"), std::runtime_error);
    BOOST_CHECK_THROW(http::parse_time("Fri, 24 Jun 2016 09:47:62 GMT"), std::runtime_error);
}
BOOST_AUTO_TEST_SUITE_END()
