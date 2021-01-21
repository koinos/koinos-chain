#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace koinos::mq {

namespace exchange {
   constexpr const char* event = "koinos_event";
   constexpr const char* rpc = "koinos_rpc";
} // exchange

namespace routing_key {
   constexpr const char* block_accept = "koinos.block.accept";
   constexpr const char* transaction_accept = "koinos.transaction.accept";
} // routing_key

enum class error_code : int64_t
{
   success,
   failure
};

namespace detail { struct message_broker_impl; }

class message_broker final
{
private:
   std::unique_ptr< detail::message_broker_impl > _message_broker_impl;

public:
   message_broker();
   ~message_broker();

   error_code connect(
      const std::string& host,
      uint16_t port,
      const std::string& vhost = "/",
      const std::string& user = "guest",
      const std::string& pass = "guest"
   ) noexcept;

   void disconnect() noexcept;

   error_code publish(
      const std::string& routing_key,
      const std::string& data,
      const std::string& content_type = "application/json",
      const std::string& exchange = exchange::event
   ) noexcept;
};

} // koinos::mq
