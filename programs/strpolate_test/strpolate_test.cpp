
#include <strpolate/strpolate.hpp>

#include <iostream>

int main(int argc, char** argv, char** envp)
{
   std::string my_name = "Pythagoras";
   int my_age = 2300;
   std::cout << STRPOLATE("Hello world, my name is ${name}, and I am ${years} years old.", ("name", my_name)("years", my_age) ) << std::endl;
   return 0;
}
