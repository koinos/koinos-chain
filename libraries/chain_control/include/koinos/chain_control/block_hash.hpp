#pragma once

// TODO:  This file should probably live in another directory



struct block_header
{
  // Block data that can read and wrie state
  active_block_data  active;

  // Block data that can only read state
  multihash_type     passive_merkle_root;
};


struct submit_block
{
   block_header_type                   block_header;
   std::vector< transaction_type >     block_transactions;
   std::vector< std::vector< segwit_type > >
                                       block_segwit;
};

struct submit_transaction
{
   transaction_type                    transaction;
   std::vector< segwit_type >          transaction_segwit;
};

