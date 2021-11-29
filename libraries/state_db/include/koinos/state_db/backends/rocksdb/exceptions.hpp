#pragma once
#include <koinos/state_db/backends/exceptions.hpp>

namespace koinos::state_db::backends::rocksdb {

KOINOS_DECLARE_DERIVED_EXCEPTION( rocksdb_backend_exception, backend_exception );

KOINOS_DECLARE_DERIVED_EXCEPTION( rocksdb_open_exception, rocksdb_backend_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( rocksdb_setup_exception, rocksdb_backend_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( rocksdb_database_not_open_exception, rocksdb_backend_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( rocksdb_read_exception, rocksdb_backend_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( rocksdb_write_exception, rocksdb_backend_exception );
KOINOS_DECLARE_DERIVED_EXCEPTION( rocksdb_session_in_progress, rocksdb_backend_exception );

} // koinos::state_db::backends::rocksdb
