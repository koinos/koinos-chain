#include <koinos/state_db/backends/backend.hpp>

namespace koinos::state_db::backends {

bool abstract_backend::empty() const
{
   return size() == 0;
}

} // koinos::state_db::backends
