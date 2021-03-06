/*******************************************************************\

Module: Destructor Calls

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

/// \file
/// Destructor Calls

#include "destructor.h"

#include <util/std_expr.h>
#include <util/std_types.h>
#include <util/std_code.h>

code_function_callt get_destructor(
  const namespacet &ns,
  const typet &type)
{
  if(type.id() == ID_symbol_type)
  {
    return get_destructor(ns, ns.follow(type));
  }
  else if(type.id()==ID_struct)
  {
    const exprt &methods=static_cast<const exprt&>(type.find(ID_methods));

    forall_operands(it, methods)
    {
      if(it->type().id()==ID_code)
      {
        const code_typet &code_type=to_code_type(it->type());

        if(code_type.return_type().id()==ID_destructor &&
           code_type.parameters().size()==1)
        {
          const typet &arg_type=code_type.parameters().front().type();

          if(arg_type.id()==ID_pointer &&
             ns.follow(arg_type.subtype())==type)
          {
            const symbol_exprt symbol_expr(it->get(ID_name), it->type());

            code_function_callt function_call;
            function_call.function()=symbol_expr;

            return function_call;
          }
        }
      }
    }
  }

  return static_cast<const code_function_callt &>(get_nil_irep());
}
