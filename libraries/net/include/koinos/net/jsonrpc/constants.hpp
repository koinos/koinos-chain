#pragma once

namespace koinos::net::jsonrpc {

namespace constants {
   constexpr const char* version_string{ "koinos/1.0" };

   namespace content_type {
      constexpr const char* application_json{ "application/json" };
   }

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
}

} // koinos::net::jsonrpc
