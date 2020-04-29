
# strpolate

A lightweight library for C++ string interpolation.

### Basic usage

```
#include <strpolate/strpolate.hpp>

STRPOLATE("Hello world, my name is ${name}, and I am ${years} years old.", ("name", my_name)("years", my_age) )
```

### Custom types

To define `strpolate` for a custom type, override `strpolate::to_string()` as appropriate, for example:

```
namespace strpolate {
inline void to_string( std::string& result, const MyType& val )
{   /* convert T to string, store in result */         }
}
```
