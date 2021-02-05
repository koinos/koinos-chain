#pragma once

#include <koinos/util.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace koinos::mq {

enum class error_code : int64_t
{
   success,
   failure,
   time_out
};

struct message
{
   uint64_t                     delivery_tag;
   std::string                  exchange;
   std::string                  routing_key;
   std::string                  content_type;
   std::optional< std::string > reply_to;
   std::optional< std::string > correlation_id;
   std::string                  data;
};

namespace detail { struct message_broker_impl; }

class message_broker final
{
private:
   std::unique_ptr< detail::message_broker_impl > _message_broker_impl;

public:
   message_broker();
   ~message_broker();

   error_code connect( const std::string& url ) noexcept;
   void disconnect() noexcept;
   bool is_connected() noexcept;

   error_code publish( const message& msg ) noexcept;

   std::pair< error_code, std::shared_ptr< message > > consume() noexcept;

   error_code declare_exchange(
      const std::string& exchange,
      const std::string& exchange_type = "direct",
      bool passive = false,
      bool durable = false,
      bool auto_delete = false,
      bool internal = false
   ) noexcept;

   std::pair< error_code, std::string > declare_queue(
      const std::string& queue,
      bool passive = false,
      bool durable = false,
      bool exclusive = false,
      bool auto_delete = false
   ) noexcept;

   error_code bind_queue(
      const std::string& queue,
      const std::string& exchange,
      const std::string& binding_key
   ) noexcept;
};

} // koinos::mq
