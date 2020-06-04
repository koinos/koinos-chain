#pragma once
#include <boost/filesystem/path.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

#define LOG(LEVEL)                                                                         \
BOOST_LOG_SEV(::boost::log::trivial::logger::get(), boost::log::trivial::LEVEL)            \
  << boost::log::add_value("Line", __LINE__)                                               \
  << boost::log::add_value("File", boost::filesystem::path(__FILE__).filename().string())  \

namespace koinos { void initialize_logging( const boost::filesystem::path& p, const std::string& file_pattern, bool color = true ); }
