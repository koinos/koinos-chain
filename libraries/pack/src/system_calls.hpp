
namespace koinos { namespace types { namespace system {

typedef uint32 thunk_id_type;

struct system_call_target_reserved {};

struct contract_call_bundle
{
   contract_id_type contract_id;
   uint32           entry_point;
};

} } } // koinos::types::system

namespace koinos { namespace types { namespace protocol {

typedef std::variant<
   system::system_call_target_reserved,
   system::thunk_id_type,
   system::contract_call_bundle
   > system_call_target;

} } } // koinos::types::protocol
