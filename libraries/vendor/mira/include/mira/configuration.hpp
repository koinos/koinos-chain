#pragma once

#include <nlohmann/json.hpp>

#include <any>
#include <stdexcept>
#include <string>

// Fwd declaration
namespace rocksdb { struct Options; }

namespace mira
{

struct mira_config_error final : std::exception
{
   private:
      std::string _msg;

   public:
      mira_config_error( std::string&& msg );
      ~mira_config_error() override;

      const char* what() const noexcept override;
};

class configuration
{
   configuration() = delete;
public:
   static ::rocksdb::Options get_options( const std::any& cfg, std::string type_name );
   static bool gather_statistics( const std::any& cfg );
   static size_t get_object_count( const std::any& cfg );
};

} // mira
