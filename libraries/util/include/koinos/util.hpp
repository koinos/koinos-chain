#pragma once

#define DO_PRAGMA(x) _Pragma (#x)
#define KOINOS_TODO(msg) DO_PRAGMA(message("TODO: " msg))

namespace koinos {

// Helper struct for using std::visit with std::variants
template< class... Ts > struct overloaded : Ts... { using Ts::operator()...; };
template< class... Ts > overloaded( Ts... ) -> overloaded< Ts... >;

} // koinos
