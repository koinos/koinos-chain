#pragma once

#include <koinos/chain/chronicler.hpp>
#include <koinos/chain/resource_meter.hpp>

#include <cstdint>
#include <string>

namespace koinos::chain {

class session final: public abstract_rc_session,
                     public abstract_chronicler_session
{
public:
  session( int64_t begin_rc );
  ~session();

  virtual void use_rc( int64_t rc ) override;
  virtual uint64_t remaining_rc() override;
  virtual uint64_t used_rc() override;
  virtual void push_event( const protocol::event_data& ev ) override;
  virtual const std::vector< protocol::event_data >& events() override;

  virtual void push_log( const std::string& log ) override;
  virtual const std::vector< std::string >& logs() override;

private:
  int64_t _begin_rc;
  int64_t _end_rc;
  std::vector< protocol::event_data > _events;
  std::vector< std::string > _logs;
};

} // namespace koinos::chain
