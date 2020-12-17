#pragma once

namespace koinos::net::protocol::jsonrpc {

namespace field {
   constexpr const char* method  { "method" };
   constexpr const char* id      { "id" };
   constexpr const char* params  { "params" };
   constexpr const char* jsonrpc { "jsonrpc" };
   constexpr const char* code    { "code" };
   constexpr const char* message { "message" };
   constexpr const char* result  { "result" };
   constexpr const char* error   { "error" };
}

} // koinos::net::protocol::jsonrpc
