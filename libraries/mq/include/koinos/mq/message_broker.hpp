#pragma once

#include <cstdint>
#include <string>

#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>

namespace koinos::mq {

namespace exchange {
   constexpr const char* broadcast = "koinos_event";
   constexpr const char* rpc = "koinos_rpc";
} // exchange

namespace queue {
   constexpr const char* block_accept = "koinos.block.accept";
   constexpr const char* transaction_accept = "koinos.transaction.accept";
} // queue

enum class error_code : int64_t
{
   success,
   failure
};

class message_broker final
{
private:
   amqp_socket_t *socket = nullptr;
   amqp_connection_state_t connection;
   amqp_channel_t channel = 1;
   amqp_bytes_t queue_name;

public:
   message_broker() = default;
   ~message_broker();

   error_code connect( const std::string& hostname, uint16_t port ) noexcept;
   error_code publish(
      const std::string& routing_key,
      const std::string& data,
      const std::string& content_type = "application/json",
      const std::string& exchange = exchange::broadcast
   ) noexcept;
   void reset() noexcept;
   void run() noexcept;
};

} // koinos::mq
