#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <koinos/log.hpp>

namespace koinos::capsule {

/**
 * dummy_tree is a test helper that explicitly tracks interior tree nodes.
 */
class dummy_tree
{
    public:
        dummy_tree() {}
        virtual ~dummy_tree() {}

        /**
         * Node ID of the root node.
         */
        int64_t root_id = -1;

        /**
         * Map from a node's ID to a vector of the node ID's of its children.
         */
        std::map< int64_t, std::vector< int64_t > > children_map;

        /**
         * Map from a node's ID to a vector of the node ID's of all leaf nodes in the subtree.
         */
        std::map< int64_t, std::vector< int64_t > > leaves_map;
};

typedef std::shared_ptr< dummy_tree > dummy_tree_ptr;

dummy_tree_ptr create_tree(int64_t level)
{
   dummy_tree_ptr result = std::make_shared< dummy_tree >();
   if( level == 0 )
   {
      result->root_id = 0;
      result->children_map[0] = std::vector<int64_t>();
      return result;
   }

   dummy_tree_ptr l_node = create_tree(level-1);
   int64_t root_id = (int64_t) l_node->children_map.size();
   int64_t offset = root_id + 1;

   for( std::pair< int64_t, std::vector< int64_t > > kv : l_node->children_map )
   {
      int64_t k = kv.first;
      std::vector< int64_t > temp = kv.second;
      result->children_map[k] = temp;
      for( size_t i=0; i<temp.size(); i++ )
         temp[i] += offset;
      result->children_map[k+offset] = temp;
   }
   result->children_map[root_id] = std::vector< int64_t > { l_node->root_id, l_node->root_id + offset };
   result->root_id = root_id;
   return result;
}

void annotate_leaves(dummy_tree_ptr tree, int64_t start_node_id)
{
   if( tree->children_map[start_node_id].size() == 0 )
   {
      tree->leaves_map[start_node_id] = std::vector< int64_t > { start_node_id };
      return;
   }

   std::vector< int64_t > result;
   for( int64_t c : tree->children_map[start_node_id] )
   {
      annotate_leaves(tree, c);
      for( int64_t leaf : tree->leaves_map[c] )
         result.push_back(leaf);
   }
   tree->leaves_map[start_node_id] = result;
}

std::set< int64_t > prune(dummy_tree_ptr tree, std::set< int64_t > leaf_ids, bool prune_parent)
{
   std::set< int64_t > pruned;
   for( const std::pair< int64_t, std::vector< int64_t > >& kv : tree->children_map )
   {
      int64_t k = kv.first;
      bool prune_current = true;
      for( int64_t leaf : tree->leaves_map[k] )
      {
         if( leaf_ids.find(leaf) != leaf_ids.end() )
         {
            prune_current = false;
            break;
         }
      }
      if( prune_current )
      {
         if( prune_parent )
            pruned.insert(k);
         else
         {
            for( int64_t c : kv.second )
            {
               pruned.insert(c);
            }
         }
      }
   }
   return pruned;
}

std::string pruned_tree_to_string(dummy_tree_ptr tree, int64_t start_node_id, const std::set< int64_t >& pruned )
{
   if( tree->children_map[start_node_id].size() == 0 )
      return "h"+std::to_string(start_node_id);

   bool recurse = true;
   for( int64_t c : tree->children_map[start_node_id] )
   {
      if( pruned.find(c) != pruned.end() )
      {
         recurse = false;
         break;
      }
   }
   if( !recurse )
      return "h"+std::to_string(start_node_id);

   bool first = true;
   std::string result = "(";
   for( int64_t c : tree->children_map[start_node_id] )
   {
      if( first )
         first = false;
      else
         result += ", ";
      if( pruned.find(c) != pruned.end() )
         result += "nil";
      else
         result += pruned_tree_to_string(tree, c, pruned);
   }
   result += ")";
   return result;
}

std::string int_array_to_string( const std::vector< int64_t >& a )
{
   std::stringstream ss;
   ss << "[";
   for( size_t i=0; i<a.size(); i++ )
   {
      if( i > 0 )
         ss << ", ";
      ss << a[i];
   }

   ss << "]";
   return ss.str();
}

void print_dummy_tree(dummy_tree_ptr tree, const std::set< int64_t >& pruned)
{
   for( const auto& kv : tree->children_map )
   {
      int64_t k = kv.first;
      if( pruned.find(k) != pruned.end() )
         continue;
      std::vector< int64_t > children;
      for( int64_t c : kv.second )
      {
         if( pruned.find(c) == pruned.end() )
            children.push_back(c);
         else
            children.push_back(-1);
      }
      LOG(debug) << k << ": children=" << int_array_to_string(children)
                      << "   leaves=" << int_array_to_string(tree->leaves_map[k]);
   }
}

}
