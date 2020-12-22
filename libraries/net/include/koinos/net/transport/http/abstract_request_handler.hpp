#pragma once

#include <string>

namespace koinos::net::transport::http {

struct abstract_request_handler
{
   virtual std::string handle( const std::string& payload ) = 0;
   virtual ~abstract_request_handler() {}
};

} // koinos::net::transport::http
