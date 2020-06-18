
namespace koinos { namespace chain {

struct void_type {};

struct hello_thunk_args
{
   protocol::uint64 a;
   protocol::uint64 b;
};

struct hello_thunk_ret
{
   protocol::uint64 c;
   protocol::uint64 d;
};

struct prints_args
{
   std::string message;
};

typedef void_type prints_ret;

struct verify_block_header_args
{
   protocol::fixed_blob< 65 > sig;
   protocol::multihash_type   digest;
};

typedef protocol::boolean verify_block_header_ret;

struct apply_block_args
{
   protocol::active_block_data block;
};

typedef void_type apply_block_ret;

struct apply_transaction_args
{
   protocol::transaction_type trx;
};

typedef void_type apply_transaction_ret;

struct apply_upload_contract_operation_args
{
   protocol::create_system_contract_operation op;
};

typedef void_type apply_upload_contract_operation_ret;

struct apply_execute_contract_operation_args
{
   protocol::contract_call_operation op;
};

typedef void_type apply_execute_contract_operation_ret;

struct db_put_object_args
{
   protocol::uint256       space;
   protocol::uint256       key;
   protocol::variable_blob obj;
};

typedef protocol::boolean db_put_object_ret;

struct db_get_object_args
{
   protocol::uint256 space;
   protocol::uint256 key;
   protocol::int32   object_size_hint;
};

typedef protocol::variable_blob db_get_object_ret;

typedef db_get_object_args db_get_next_object_args;

typedef db_get_object_ret db_get_next_object_ret;

typedef db_get_object_args db_get_prev_object_args;

typedef db_get_object_ret db_get_prev_object_ret;

typedef void_type get_contract_args_size_args;

typedef protocol::int32 get_contract_args_size_ret;

typedef void_type get_contract_args_args;

typedef protocol::variable_blob get_contract_args_ret;

} }
