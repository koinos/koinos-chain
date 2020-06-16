namespace koinos { namespace chain_control {

// Every block has a cryptographic ID.
// Check the claimed ID against the block content.

struct block_topology
{
   protocol::multihash_type                 id;
   protocol::block_height_type              block_num;
   protocol::multihash_type                 previous;
};

struct submit_reserved {};

struct submit_block
{
   block_topology                                block_topo;

   protocol::variable_blob                       block_header_bytes;
   std::vector< protocol::variable_blob >        block_transactions_bytes;
   std::vector< protocol::variable_blob >        block_passives_bytes;
};

struct submit_transaction
{
   protocol::variable_blob                       transaction_active_bytes;
   protocol::variable_blob                       transaction_passive_bytes;
};

struct submit_query
{
   protocol::variable_blob                       query;
};

typedef std::variant<
   submit_reserved,
   submit_block,
   submit_transaction,
   submit_query > submit_item;

struct get_head_info_params {};

typedef std::variant<
   get_head_info_params > query_param_item;

struct submit_return_reserved {};

struct submit_return_block {};

struct submit_return_transaction {};

struct submit_return_query
{
   protocol::variable_blob result;
};

struct submit_return_error
{
   protocol::variable_blob error_text;
};

typedef std::variant<
   submit_return_reserved,
   submit_return_block,
   submit_return_transaction,
   submit_return_query,
   submit_return_error > submit_return;

typedef submit_return_query query_error;

struct get_head_info_return
{
   protocol::multihash_type    id;
   protocol::block_height_type height;
};

typedef std::variant<
   query_error,
   get_head_info_return > query_result_item;

} } // koinos::chain_control
