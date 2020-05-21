namespace koinos { namespace protocol {

// Every block has a cryptographic ID.
// Check the claimed ID against the block content.

struct submit_reserved {};

struct submit_block
{
   block_topology                      block_topo;

   vl_blob                             block_header_bytes;
   std::vector< vl_blob >              block_transactions_bytes;
   std::vector< vl_blob >              block_passives_bytes;
};

struct submit_transaction
{
   vl_blob                             transaction_active_bytes;
   vl_blob                             transaction_passive_bytes;
};

struct submit_query
{
   vl_blob                             query;
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
   vl_blob                             result;
};

struct submit_return_error
{
   vl_blob                             error_text;
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
   multihash_type    id;
   block_height_type height;
};

typedef std::variant<
   query_error,
   get_head_info_return > query_result_item;

} } // koinos::protocol
