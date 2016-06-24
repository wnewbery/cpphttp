#include <boost/test/unit_test.hpp>
#include "Example.hpp"

BOOST_AUTO_TEST_SUITE(TestExample)
BOOST_AUTO_TEST_CASE(test)
{
    BOOST_CHECK_EQUAL("baz", foo());
}
BOOST_AUTO_TEST_SUITE_END()
