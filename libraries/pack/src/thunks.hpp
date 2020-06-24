
namespace koinos { namespace types { namespace thunks {

struct void_type {};

struct prints_args
{
   std::string message;
};

typedef void_type prints_ret;

struct verify_block_header_args
{
   types::fixed_blob< 65 > sig;
   types::multihash_type   digest;
};

typedef types::boolean verify_block_header_ret;

struct apply_block_args
{
   types::protocol::active_block_data block;
};

typedef void_type apply_block_ret;

struct apply_transaction_args
{
   types::protocol::transaction_type trx;
};

typedef void_type apply_transaction_ret;

struct apply_upload_contract_operation_args
{
   types::protocol::create_system_contract_operation op;
};

typedef void_type apply_upload_contract_operation_ret;

struct apply_reserved_operation_args
{
   types::protocol::reserved_operation op;
};

typedef void_type apply_reserved_operation_ret;

struct apply_execute_contract_operation_args
{
   types::protocol::contract_call_operation op;
};

typedef void_type apply_execute_contract_operation_ret;

struct apply_set_system_call_operation_args
{
   protocol::set_system_call_operation op;
};

typedef void_type apply_set_system_call_operation_ret;

struct db_put_object_args
{
   types::uint256       space;
   types::uint256       key;
   types::variable_blob obj;
};

typedef types::boolean db_put_object_ret;

struct db_get_object_args
{
   types::uint256 space;
   types::uint256 key;
   types::int32   object_size_hint;
};

typedef types::variable_blob db_get_object_ret;

typedef db_get_object_args db_get_next_object_args;

typedef db_get_object_ret db_get_next_object_ret;

typedef db_get_object_args db_get_prev_object_args;

typedef db_get_object_ret db_get_prev_object_ret;

typedef void_type get_contract_args_size_args;

typedef types::int32 get_contract_args_size_ret;

typedef void_type get_contract_args_args;

typedef types::variable_blob get_contract_args_ret;

} } } // koinos::types::thunks
