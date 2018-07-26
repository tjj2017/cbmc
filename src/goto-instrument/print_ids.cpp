/*******************************************************************\

Module: Print IDs

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "print_ids.h"

#include <util/std_code.h>
#include <util/namespace.h>

#include <goto-programs/goto_model.h>

#include <iostream>

void print_ids_rec(const exprt &src, const std::string &suffix)
{
  if(src.id()==ID_symbol)
  {
    std::cout << "  ID: "
              << to_symbol_expr(src).get_identifier()
              << suffix << '\n';
  }
  else if(src.id()==ID_member)
  {
    const auto &m=to_member_expr(src);
    print_ids_rec(m.struct_op(), "."+id2string(m.get_component_name())+suffix);
  }
  else if(src.id()==ID_index)
  {
    const auto &i=to_index_expr(src);
    print_ids_rec(i.array(), "[]"+suffix);
  }
}

std::vector<exprt> flatten_structs(
  const exprt &src,
  const namespacet &ns)
{
  const typet &t=ns.follow(src.type());
  if(t.id()==ID_struct)
  {
    std::vector<exprt> result;

    for(const auto &c : to_struct_type(t).components())
    {
      member_exprt m(src, c.get_name(), c.type());
      auto f=flatten_structs(m, ns);
      result.insert(result.end(), f.begin(), f.end());
    }

    return result;
  }
  else
    return { src };
}

void print_ids(const goto_modelt &goto_model)
{
  for(const auto & f : goto_model.goto_functions.function_map)
  {
    std::cout << f.first << '\n';
    for(const auto & i : f.second.body.instructions)
    {
      if(i.is_assign())
      {
        exprt lhs=to_code_assign(i.code).lhs();
        namespacet ns(goto_model.symbol_table);
        auto flattened=flatten_structs(lhs, ns);
        for(const auto &e : flattened)
        {
          // check type e.type() here if you want to only do
          // function pointers
          print_ids_rec(e, "");
        }
      }
    }
  }
}
