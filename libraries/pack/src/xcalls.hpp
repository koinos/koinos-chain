
namespace koinos { namespace protocol {

typedef uint32 thunk_id_type;

struct xcall_target_reserved {};

typedef std::variant<
   xcall_target_reserved,
   thunk_id_type,
   contract_id_type
   > xcall_target;

} }
