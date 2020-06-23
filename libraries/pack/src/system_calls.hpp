
namespace koinos { namespace types { namespace system {

typedef uint32 thunk_id_type;

struct sys_call_target_reserved {};

struct system_call_bundle
{
   contract_id_type contract_id;
   uint32           entry_point;
};

typedef std::variant<
   sys_call_target_reserved,
   thunk_id_type,
   system_call_bundle
   > sys_call_target;

} } } // koinos::types::system
