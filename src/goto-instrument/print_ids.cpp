/*******************************************************************\

Module: Print IDs

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "print_ids.h"

#include <util/std_code.h>
#include <util/c_types.h>
#include <util/namespace.h>
#include <util/union_find.h>
#include <util/type_eq.h>
#include <util/fresh_symbol.h>
#include <util/expanding_vector.h>

#include <goto-programs/goto_model.h>

#include <iostream>

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

class aliasest
{
public:
  // IDs to addresses
  std::map<irep_idt, std::set<symbol_exprt> > address_map;

  unsigned_union_find uuf;

  // map from root to other elements of same set
  expanding_vectort<std::set<unsigned> > root_map;

  void operator()(const goto_modelt &goto_model);  

  void merge_ids(const exprt &src, optionalt<irep_idt> &id)
  {
    merge_ids_rec(src, "", id);
  }

  void merge_ids_rec(const exprt &, const std::string &suffix, optionalt<irep_idt> &id);

  std::set<irep_idt> get_ids(const exprt &src)
  {
    std::set<irep_idt> ids;
    get_ids(src, ids);
    return ids;
  }

  void get_ids(const exprt &src, std::set<irep_idt> &dest)
  {
    get_ids_rec(src, "", dest);
  }

  void get_ids_rec(const exprt &, const std::string &suffix, std::set<irep_idt> &);

  void output(std::ostream &);

  void get_addresses_rec(const exprt &, std::set<symbol_exprt> &);
};

void aliasest::get_addresses_rec(const exprt &src, std::set<symbol_exprt> &dest)
{
  if(src.id()==ID_address_of)
  {
    const exprt &obj=to_address_of_expr(src).object();
    if(obj.id()==ID_symbol)
      dest.insert(to_symbol_expr(obj));
  }
  else
  {
    for(const auto &op : src.operands())
      get_addresses_rec(op, dest);
  }
}

void aliasest::output(std::ostream &out)
{
  for(std::size_t i=0; i<uuf.size(); i++)
    if(!root_map[i].empty())
    {
      out << dstringt::make_from_table_index(i);
      for(const auto j : root_map[i])
        out << " = " << dstringt::make_from_table_index(j);
      out << '\n';
    }
}

void aliasest::merge_ids_rec(
  const exprt &src,
  const std::string &suffix,
  optionalt<irep_idt> &id)
{
  if(src.id()==ID_symbol)
  {
    const auto &identifier=to_symbol_expr(src).get_identifier();
    const irep_idt final_id=
      suffix.empty()?identifier:id2string(identifier)+suffix;

    if(id.has_value())
    {
      unsigned no1=final_id.get_no();
      unsigned no2=id->get_no();
      uuf.make_union(no1, no2);
      std::cout << "MERGING " << dstringt::make_from_table_index(no1)
                << " " << dstringt::make_from_table_index(no2) << '\n';
    }
    else
      id=final_id;
  }
  else if(src.id()==ID_member)
  {
    const auto &m=to_member_expr(src);
    merge_ids_rec(m.struct_op(), "."+id2string(m.get_component_name())+suffix, id);
  }
  else if(src.id()==ID_index)
  {
    const auto &i=to_index_expr(src);
    merge_ids_rec(i.array(), "[]"+suffix, id);
  }
  else if(src.id()==ID_typecast)
  {
    merge_ids_rec(to_typecast_expr(src).op(), suffix, id);
  }
  /*
  else if(src.id()==ID_dereference)
  {
    const auto &pointer=to_dereference_expr(src).pointer();
    merge_ids_rec(pointer, suffix, id);
  }
  */
}

void aliasest::get_ids_rec(
  const exprt &src,
  const std::string &suffix,
  std::set<irep_idt> &dest)
{
  if(src.id()==ID_symbol)
  {
    const auto &identifier=to_symbol_expr(src).get_identifier();
    const irep_idt final_id=
      suffix.empty()?identifier:id2string(identifier)+suffix;
    dest.insert(final_id);
  }
  else if(src.id()==ID_member)
  {
    const auto &m=to_member_expr(src);
    get_ids_rec(m.struct_op(), "."+id2string(m.get_component_name())+suffix, dest);
  }
  else if(src.id()==ID_index)
  {
    const auto &i=to_index_expr(src);
    get_ids_rec(i.array(), "[]"+suffix, dest);
  }
  else if(src.id()==ID_typecast)
  {
    get_ids_rec(to_typecast_expr(src).op(), suffix, dest);
  }
  /*
  else if(src.id()==ID_dereference)
  {
    const auto &pointer=to_dereference_expr(src).pointer();
    get_ids_rec(pointer, suffix, dest);
  }
  */
}

void aliasest::operator()(const goto_modelt &goto_model)
{
  namespacet ns(goto_model.symbol_table);

  // collect addresses
  for(const auto & f : goto_model.goto_functions.function_map)
  {
    for(const auto & i : f.second.body.instructions)
    {
      if(i.is_assign())
      {
        const auto &assignment=to_code_assign(i.code);
        std::set<symbol_exprt> addresses;
        get_addresses_rec(assignment.rhs(), addresses);
        if(!addresses.empty())
        {
          auto flattened_lhs=flatten_structs(assignment.lhs(), ns);
          std::set<irep_idt> ids;
          for(const auto &e : flattened_lhs)
            if(e.type().id()==ID_pointer)
              get_ids(e, ids);

          for(const auto id : ids)
          {
            for(const auto & a : addresses)
              std::cout << id << " -> " << a.get_identifier() << '\n';

            address_map[id]=addresses;
          }
        }
      }
    }
  }

  // compute aliases
  for(const auto & f : goto_model.goto_functions.function_map)
  {
    for(const auto & i : f.second.body.instructions)
    {
      if(i.is_assign())
      {
        const auto &assignment=to_code_assign(i.code);

        auto flattened_lhs=flatten_structs(assignment.lhs(), ns);
        auto flattened_rhs=flatten_structs(assignment.rhs(), ns);

        optionalt<irep_idt> id;

        for(const auto &e : flattened_lhs)
          if(e.type().id()==ID_pointer)
            merge_ids(e, id);

        for(const auto &e : flattened_rhs)
          if(e.type().id()==ID_pointer)
            merge_ids(e, id);
      }
      else if(i.is_function_call())
      {
        // do parameter assignments
        // TODO
      }
    }
  }

  // root map
  for(std::size_t i=0; i<uuf.size(); i++)
    if(!uuf.is_root(i))
      root_map[uuf.find(i)].insert(i);  
}

void fix_argument_types(
  code_function_callt &function_call,
  const namespacet &ns)
{
  const code_typet &code_type=
    to_code_type(ns.follow(function_call.function().type()));

  const code_typet::parameterst &function_parameters=
    code_type.parameters();

  code_function_callt::argumentst &call_arguments=
    function_call.arguments();

  for(std::size_t i=0; i<function_parameters.size(); i++)
  {
    if(i<call_arguments.size())
    {
      if(!type_eq(call_arguments[i].type(),
                  function_parameters[i].type(), ns))
      {
        call_arguments[i].make_typecast(function_parameters[i].type());
      }
    }
  }
}

void fix_return_type(
  code_function_callt &function_call,
  goto_programt &dest,
  goto_modelt &goto_model)
{
  // are we returning anything at all?
  if(function_call.lhs().is_nil())
    return;

  const namespacet ns(goto_model.symbol_table);

  const code_typet &code_type=
    to_code_type(ns.follow(function_call.function().type()));

  // type already ok?
  if(type_eq(
       function_call.lhs().type(),
       code_type.return_type(), ns))
    return;

  const symbolt &function_symbol =
    ns.lookup(to_symbol_expr(function_call.function()).get_identifier());

  symbolt &tmp_symbol = get_fresh_aux_symbol(
    code_type.return_type(),
    id2string(function_call.source_location().get_function()),
    "tmp_return_val_" + id2string(function_symbol.base_name),
    function_call.source_location(),
    function_symbol.mode,
    goto_model.symbol_table);

  const symbol_exprt tmp_symbol_expr = tmp_symbol.symbol_expr();

  exprt old_lhs=function_call.lhs();
  function_call.lhs()=tmp_symbol_expr;

  goto_programt::targett t_assign=dest.add_instruction();
  t_assign->make_assignment();
  t_assign->code=code_assignt(
    old_lhs, typecast_exprt(tmp_symbol_expr, old_lhs.type()));
}

void remove_function_pointer(
  goto_programt &goto_program,
  goto_programt::targett target,
  const std::set<symbol_exprt> &functions,
  goto_modelt &goto_model)
{
  const code_function_callt &code=
    to_code_function_call(target->code);

  const exprt &function=code.function();

  // this better have the right type
  code_typet call_type=to_code_type(function.type());

  // refine the type in case the forward declaration was incomplete
  if(call_type.has_ellipsis() &&
     call_type.parameters().empty())
  {
    call_type.remove_ellipsis();
    forall_expr(it, code.arguments())
      call_type.parameters().push_back(
        code_typet::parametert(it->type()));
  }

  assert(function.id()==ID_dereference);
  assert(function.operands().size()==1);

  const exprt &pointer=to_dereference_expr(function);

  // the final target is a skip
  goto_programt final_skip;

  goto_programt::targett t_final=final_skip.add_instruction();
  t_final->make_skip();

  // build the calls and gotos

  goto_programt new_code_calls;
  goto_programt new_code_gotos;

  for(const auto &fun : functions)
  {
    // call function
    goto_programt::targett t1=new_code_calls.add_instruction();
    t1->make_function_call(code);
    to_code_function_call(t1->code).function()=fun;

    // the signature of the function might not match precisely
    const namespacet ns(goto_model.symbol_table);
    fix_argument_types(to_code_function_call(t1->code), ns);
    fix_return_type(to_code_function_call(t1->code), new_code_calls, goto_model);

    // goto final
    goto_programt::targett t3=new_code_calls.add_instruction();
    t3->make_goto(t_final, true_exprt());

    // goto to call
    address_of_exprt address_of(fun, pointer_type(fun.type()));

    if(address_of.type()!=pointer.type())
      address_of.make_typecast(pointer.type());

    goto_programt::targett t4=new_code_gotos.add_instruction();
    t4->make_goto(t1, equal_exprt(pointer, address_of));
  }

  // fall-through
  //if(add_safety_assertion)
  {
    goto_programt::targett t=new_code_gotos.add_instruction();
    t->make_assertion(false_exprt());
    t->source_location.set_property_class("pointer dereference");
    t->source_location.set_comment("invalid function pointer");
  }

  goto_programt new_code;

  // patch them all together
  new_code.destructive_append(new_code_gotos);
  new_code.destructive_append(new_code_calls);
  new_code.destructive_append(final_skip);

  // set locations
  Forall_goto_program_instructions(it, new_code)
  {
    irep_idt property_class=it->source_location.get_property_class();
    irep_idt comment=it->source_location.get_comment();
    it->source_location=target->source_location;
    it->function=target->function;
    if(!property_class.empty())
      it->source_location.set_property_class(property_class);
    if(!comment.empty())
      it->source_location.set_comment(comment);
  }

  goto_programt::targett next_target=target;
  next_target++;

  goto_program.destructive_insert(next_target, new_code);

  // We preserve the original dereferencing to possibly catch
  // further pointer-related errors.
  code_expressiont code_expression(function);
  code_expression.add_source_location()=function.source_location();
  target->code.swap(code_expression);
  target->type=OTHER;
}

void print_ids(goto_modelt &goto_model)
{
  aliasest aliases;

  aliases(goto_model);

  std::cout << "---------\n";
  aliases.output(std::cout);
  std::cout << "---------\n";
  
  // now replace aliases by addresses
  for(auto & f : goto_model.goto_functions.function_map)
  {
    for(auto target=f.second.body.instructions.begin();
        target!=f.second.body.instructions.end();
        target++)
    {
      if(target->is_function_call())
      {
        const auto &call=to_code_function_call(target->code);
        if(call.function().id()==ID_dereference)
        {
          const auto &pointer=to_dereference_expr(call.function()).pointer();
          auto ids=aliases.get_ids(pointer);

          std::set<symbol_exprt> functions;

          for(const auto &id : ids)
          {
            unsigned root=aliases.uuf.find(id.get_no());

            irep_idt root_id=dstringt::make_from_table_index(root);
            for(const auto &address : aliases.address_map[root_id])
              if(address.type().id()==ID_code)
                functions.insert(address);

            for(const auto &alias : aliases.root_map[root])
            {
              irep_idt alias_id=dstringt::make_from_table_index(alias);
              for(const auto &address : aliases.address_map[alias_id])
                if(address.type().id()==ID_code)
                  functions.insert(address);
            }
          }

          std::cout << "CALL at " << target->source_location << ":\n";

          for(const auto &id : ids)
            std::cout << "  id: " << id << '\n';

          for(const auto &f : functions)
            std::cout << "  function: " << f.get_identifier() << '\n';

          remove_function_pointer(
            f.second.body, target, functions, goto_model);
        }
      }
    }
  }
}
