#pragma once
#include <koinos/protocol/protocol.pb.h>
#include <string>
#include <memory>
#include <optional>
#include <utility>

namespace koinos::chain {

using event_bundle = std::pair< std::optional< std::string >, protocol::event_data >;

struct abstract_chronicler_session
{
   virtual void push_event( const protocol::event_data& ev )   = 0;
   virtual const std::vector< protocol::event_data >& events() = 0;

   virtual void push_log( const std::string& log )             = 0;
   virtual const std::vector< std::string >& logs()            = 0;
};

class chronicler final {
public:
   void set_session( std::shared_ptr< abstract_chronicler_session > s );
   void push_event( std::optional< std::string > transaction_id, protocol::event_data&& ev );
   void push_log( const std::string& message );
   const std::vector< event_bundle >& events();
   const std::vector< std::string >& logs();
private:
   std::weak_ptr< abstract_chronicler_session > _session;
   std::vector< event_bundle >             _events;
   std::vector< std::string >              _logs;
   uint32_t                                _seq_no = 0;
};

} // koinos::chain
