#define BOOST_TEST_MODULE "C++ HTTP Unit Tests"
#include <boost/test/included/unit_test.hpp>
#include "net/Net.hpp"

struct Startup
{
    Startup()
    {
        http::init_net();
    }
}startup;
