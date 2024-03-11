#pragma once

#include <koinos/vm_manager/fizzy/module_cache.hpp>
#include <koinos/vm_manager/vm_backend.hpp>

#include <koinos/chain/chain.pb.h>

#include <string>

namespace koinos::vm_manager::fizzy {

/**
 * Implementation of vm_backend for Fizzy.
 */
class fizzy_vm_backend: public vm_backend
{
public:
  fizzy_vm_backend();
  virtual ~fizzy_vm_backend();

  virtual std::string backend_name();
  virtual void initialize();

  virtual void run( abstract_host_api& hapi, const std::string& bytecode, const std::string& id = std::string() );

private:
  module_cache _cache;
};

} // namespace koinos::vm_manager::fizzy
