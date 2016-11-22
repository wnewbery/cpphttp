#include <boost/test/unit_test.hpp>
#include "net/Cert.hpp"

using namespace http;

BOOST_AUTO_TEST_SUITE(TestCert)
BOOST_AUTO_TEST_CASE(file_cert)
{
    BOOST_CHECK(load_pfx_cert("localhost.pfx", "password"));
    BOOST_CHECK_THROW(load_pfx_cert("localhost.pfx", "wrong"), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
