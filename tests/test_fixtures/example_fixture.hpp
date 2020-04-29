#pragma once

struct example_fixture
{
   example_fixture()  {} // Constructor is ran prior to each test
   ~example_fixture() {} // Destructor is ran at the end of each test
};
