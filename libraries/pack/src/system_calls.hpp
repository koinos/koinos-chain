
namespace koinos { namespace protocol {

typedef uint32 thunk_id_type;

struct sys_call_target_reserved {};

typedef std::variant<
   sys_call_target_reserved,
   thunk_id_type,
   contract_id_type
   > sys_call_target;

} }
