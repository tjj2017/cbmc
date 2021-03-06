/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include "replace_symbol.h"

#include "std_types.h"
#include "std_expr.h"

replace_symbolt::replace_symbolt()
{
}

replace_symbolt::~replace_symbolt()
{
}

void replace_symbolt::insert(
  const symbol_exprt &old_expr,
  const exprt &new_expr)
{
  expr_map.insert(std::pair<irep_idt, exprt>(
    old_expr.get_identifier(), new_expr));
}

bool replace_symbolt::replace(
  exprt &dest,
  const bool replace_with_const) const
{
  bool result=true; // unchanged

  // first look at type

  const exprt &const_dest(dest);
  if(have_to_replace(const_dest.type()))
    if(!replace(dest.type()))
      result=false;

  // now do expression itself

  if(!have_to_replace(dest))
    return result;

  if(dest.id()==ID_member)
  {
    member_exprt &me=to_member_expr(dest);

    if(!replace(me.struct_op(), replace_with_const)) // Could give non l-value.
      result=false;
  }
  else if(dest.id()==ID_index)
  {
    index_exprt &ie=to_index_expr(dest);

    if(!replace(ie.array(), replace_with_const)) // Could give non l-value.
      result=false;

    if(!replace(ie.index()))
      result=false;
  }
  else if(dest.id()==ID_address_of)
  {
    address_of_exprt &aoe=to_address_of_expr(dest);

    if(!replace(aoe.object(), false))
      result=false;
  }
  else if(dest.id()==ID_symbol)
  {
    const symbol_exprt &s=to_symbol_expr(dest);

    expr_mapt::const_iterator it=
      expr_map.find(s.get_identifier());

    if(it!=expr_map.end())
    {
      const exprt &e=it->second;

      if(!replace_with_const && e.is_constant())  // Would give non l-value.
        return true;

      dest=e;

      return false;
    }
  }
  else
  {
    Forall_operands(it, dest)
      if(!replace(*it))
        result=false;
  }

  const typet &c_sizeof_type =
    static_cast<const typet&>(dest.find(ID_C_c_sizeof_type));
  if(c_sizeof_type.is_not_nil() && have_to_replace(c_sizeof_type))
    result &= replace(static_cast<typet&>(dest.add(ID_C_c_sizeof_type)));

  const typet &va_arg_type =
    static_cast<const typet&>(dest.find(ID_C_va_arg_type));
  if(va_arg_type.is_not_nil() && have_to_replace(va_arg_type))
    result &= replace(static_cast<typet&>(dest.add(ID_C_va_arg_type)));

  return result;
}

bool replace_symbolt::have_to_replace(const exprt &dest) const
{
  if(expr_map.empty() && type_map.empty())
    return false;

  // first look at type

  if(have_to_replace(dest.type()))
    return true;

  // now do expression itself

  if(dest.id()==ID_symbol)
  {
    const irep_idt &identifier = to_symbol_expr(dest).get_identifier();
    return expr_map.find(identifier) != expr_map.end();
  }

  forall_operands(it, dest)
    if(have_to_replace(*it))
      return true;

  const irept &c_sizeof_type=dest.find(ID_C_c_sizeof_type);

  if(c_sizeof_type.is_not_nil())
    if(have_to_replace(static_cast<const typet &>(c_sizeof_type)))
      return true;

  const irept &va_arg_type=dest.find(ID_C_va_arg_type);

  if(va_arg_type.is_not_nil())
    if(have_to_replace(static_cast<const typet &>(va_arg_type)))
      return true;

  return false;
}

bool replace_symbolt::replace(typet &dest) const
{
  if(!have_to_replace(dest))
    return true;

  bool result=true;

  if(dest.has_subtype())
    if(!replace(dest.subtype()))
      result=false;

  Forall_subtypes(it, dest)
    if(!replace(*it))
      result=false;

  if(dest.id()==ID_struct ||
     dest.id()==ID_union)
  {
    struct_union_typet &struct_union_type=to_struct_union_type(dest);
    struct_union_typet::componentst &components=
      struct_union_type.components();

    for(struct_union_typet::componentst::iterator
        it=components.begin();
        it!=components.end();
        it++)
      if(!replace(*it))
        result=false;
  }
  else if(dest.id()==ID_code)
  {
    code_typet &code_type=to_code_type(dest);
    if(!replace(code_type.return_type()))
      result = false;
    code_typet::parameterst &parameters=code_type.parameters();
    for(code_typet::parameterst::iterator it = parameters.begin();
        it!=parameters.end();
        it++)
      if(!replace(*it))
        result=false;
  }
  else if(dest.id() == ID_symbol_type)
  {
    type_mapt::const_iterator it =
      type_map.find(to_symbol_type(dest).get_identifier());

    if(it!=type_map.end())
    {
      dest=it->second;
      result=false;
    }
  }
  else if(dest.id()==ID_array)
  {
    array_typet &array_type=to_array_type(dest);
    if(!replace(array_type.size()))
      result=false;
  }

  return result;
}

bool replace_symbolt::have_to_replace(const typet &dest) const
{
  if(expr_map.empty() && type_map.empty())
    return false;

  if(dest.has_subtype())
    if(have_to_replace(dest.subtype()))
      return true;

  forall_subtypes(it, dest)
    if(have_to_replace(*it))
      return true;

  if(dest.id()==ID_struct ||
     dest.id()==ID_union)
  {
    const struct_union_typet &struct_union_type=
      to_struct_union_type(dest);

    const struct_union_typet::componentst &components=
      struct_union_type.components();

    for(struct_union_typet::componentst::const_iterator
        it=components.begin();
        it!=components.end();
        it++)
      if(have_to_replace(*it))
        return true;
  }
  else if(dest.id()==ID_code)
  {
    const code_typet &code_type=to_code_type(dest);
    if(have_to_replace(code_type.return_type()))
      return true;

    const code_typet::parameterst &parameters=code_type.parameters();

    for(code_typet::parameterst::const_iterator
        it=parameters.begin();
        it!=parameters.end();
        it++)
      if(have_to_replace(*it))
        return true;
  }
  else if(dest.id() == ID_symbol_type)
  {
    const irep_idt &identifier = to_symbol_type(dest).get_identifier();
    return type_map.find(identifier) != type_map.end();
  }
  else if(dest.id()==ID_array)
    return have_to_replace(to_array_type(dest).size());

  return false;
}
