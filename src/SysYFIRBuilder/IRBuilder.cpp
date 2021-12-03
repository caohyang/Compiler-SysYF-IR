#include "IRBuilder.h"
#include <SyntaxTree.h>
#include <vector>

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

// You can define global variables and functions here
// to store state

// store temporary value
Value *tmp_val = nullptr;
// function that is being built
Function *cur_fun = nullptr;
// while exit
std::vector<BasicBlock *> while_exit;
// while begin
std::vector<BasicBlock *> while_begin;
// detect scope pre-enter (for elegance only)
bool pre_enter_scope = false;
// whether require lvalue
bool require_lvalue = false;

// types
Type *VOID_T;
Type *INT1_T;
Type *INT32_T;
Type *FLOAT_T;
Type *INT32PTR_T;
Type *FLOATPTR_T;

void IRBuilder::visit(SyntaxTree::Assembly &node) {
  VOID_T = Type::get_void_type(module.get());
  INT1_T = Type::get_int1_type(module.get());
  INT32_T = Type::get_int32_type(module.get());
  FLOAT_T = Type::get_float_type(module.get());
  INT32PTR_T = Type::get_int32_ptr_type(module.get());
  FLOATPTR_T = Type::get_float_ptr_type(module.get());
  for (const auto &def : node.global_defs) {
    def->accept(*this);
  }
}

// You need to fill them

void IRBuilder::visit(SyntaxTree::InitVal &node) {}

void IRBuilder::visit(SyntaxTree::FuncDef &node) {
  FunctionType *fun_type;
  Type *ret_type;
  std::vector<Type *> param_types;
  if (node.ret_type == SyntaxTree::Type::INT)
    ret_type = INT32_T;
  else if (node.ret_type == SyntaxTree::Type::FLOAT)
    ret_type = FLOAT_T;
  else
    ret_type = VOID_T;

  for (auto &param : node.param_list->params) {
    if (param->param_type ==  SyntaxTree::Type::INT) {
      if (param->array_index.size() != 0) {
        param_types.push_back(INT32PTR_T);
      } else {
        param_types.push_back(INT32_T);
      }
    } else {
      if (param->array_index.size() != 0) {
        param_types.push_back(FLOATPTR_T);
      } else {
        param_types.push_back(FLOAT_T);
      }
    }
  }

  fun_type = FunctionType::get(ret_type, param_types);
  auto fun = Function::create(fun_type, node.name, module.get());
  scope.push(node.name, fun);
  cur_fun = fun;
  auto funBB = BasicBlock::create(module.get(), "entry", fun);
  builder->set_insert_point(funBB);
  scope.enter();
  pre_enter_scope = true;
  std::vector<Value *> args;
  for (auto arg = fun->arg_begin(); arg != fun->arg_end(); arg++) {
    args.push_back(*arg);
  }
  for (int i = 0; i < node.param_list->params.size(); ++i) {
    if (node.param_list->params[i]->array_index.size() != 0) {
      Value *array_alloc;
      if (node.param_list->params[i]->param_type == SyntaxTree::Type::INT)
        array_alloc = builder->create_alloca(INT32PTR_T);
      else
        array_alloc = builder->create_alloca(FLOATPTR_T);
      builder->create_store(args[i], array_alloc);
      scope.push(node.param_list->params[i]->name, array_alloc);
    } else {
      Value *alloc;
      if (node.param_list->params[i]->param_type ==SyntaxTree::Type::INT)
        alloc = builder->create_alloca(INT32_T);
      else
        alloc = builder->create_alloca(FLOAT_T);
      builder->create_store(args[i], alloc);
      scope.push(node.param_list->params[i]->name, alloc);
    }
  }
  node.body->accept(*this);
  if (builder->get_insert_block()->get_terminator() == nullptr) {
    if (cur_fun->get_return_type()->is_void_type())
      builder->create_void_ret();
    else if (cur_fun->get_return_type()->is_float_type())
      builder->create_ret(CONST_FLOAT(0.));
    else
      builder->create_ret(CONST_INT(0));
  }
  scope.exit();
}

void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {}

void IRBuilder::visit(SyntaxTree::FuncParam &node) {}

void IRBuilder::visit(SyntaxTree::VarDef &node) {}

void IRBuilder::visit(SyntaxTree::LVal &node) {}

void IRBuilder::visit(SyntaxTree::AssignStmt &node) {
  node.value->accept(*this);
  auto expr_result = tmp_val;
  require_lvalue = true;
  node.target->accept(*this);
  auto var_addr = tmp_val;
  if (var_addr->get_type()->get_pointer_element_type() != expr_result->get_type()) {
    if (expr_result->get_type() == INT32_T) {
      expr_result = builder->create_sitofp(expr_result, FLOAT_T);
    }
    else {
      expr_result = builder->create_fptosi(expr_result, INT32_T);
    }
  }
  builder->create_store(expr_result, var_addr);
  tmp_val = expr_result;
}

void IRBuilder::visit(SyntaxTree::Literal &node) {
  if (node.literal_type == SyntaxTree::Type::INT)
    tmp_val = CONST_INT(node.int_const);
  else
    tmp_val = CONST_FLOAT(node.float_const);
}

void IRBuilder::visit(SyntaxTree::ReturnStmt &node) {
  if (node.ret == nullptr) {
    builder->create_void_ret();
  } else {
    auto func_ret_type = cur_fun->get_function_type()->get_return_type();
    node.ret->accept(*this);
    if (func_ret_type != tmp_val->get_type()) {
      if (func_ret_type->is_integer_type()) {
        tmp_val = builder->create_fptosi(tmp_val, INT32_T);
      }
      else {
        tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
      }
    }
    builder->create_ret(tmp_val);
  }
}

void IRBuilder::visit(SyntaxTree::BlockStmt &node) { //@cyy
  bool need_exit_scope = !pre_enter_scope;
  if (pre_enter_scope) 
    pre_enter_scope = false;
  else 
    scope.enter();
  for (auto &decl : node.body) {
    decl->accept(*this);
    if (builder->get_insert_block()->get_terminator() != nullptr)
      break;
  }
  if (need_exit_scope) {
    scope.exit();
  }
}

void IRBuilder::visit(SyntaxTree::EmptyStmt &node) { tmp_val = nullptr; } //@cyy

void IRBuilder::visit(SyntaxTree::ExprStmt &node) { node.exp->accept(*this); } //@cyy

void IRBuilder::visit(SyntaxTree::UnaryCondExpr &node) { //@cyy
  if (node.op == SyntaxTree::UnaryCondOp::NOT) {
    node.rhs->accept(*this);
    auto r_val = tmp_val;
    tmp_val = builder->create_icmp_eq(r_val, CONST_INT(0));
  }
}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) { //@cyy
  CmpInst *cond_val;
  if (node.op == SyntaxTree::BinaryCondOp::LAND) {
    auto trueBB = BasicBlock::create(module.get(), "", cur_fun);
    IF_While_And_Cond_Stack.push_back({trueBB, IF_While_Or_Cond_Stack.back().falseBB});
    node.lhs->accept(*this);
    IF_While_And_Cond_Stack.pop_back();
    auto ret_val = tmp_val;
    cond_val = dynamic_cast<CmpInst *>(ret_val);
    if (cond_val == nullptr) {
      cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
    }
    builder->create_cond_br(cond_val, trueBB, IF_While_Or_Cond_Stack.back().falseBB);
    builder->set_insert_point(trueBB);
    node.rhs->accept(*this);
  }
  else if (node.op == SyntaxTree::BinaryCondOp::LOR) {
    auto falseBB = BasicBlock::create(module.get(), "", cur_fun);
    IF_While_Or_Cond_Stack.push_back({IF_While_Or_Cond_Stack.back().trueBB, falseBB});
    node.lhs->accept(*this);
    IF_While_Or_Cond_Stack.pop_back();
    auto ret_val = tmp_val;
    cond_val = dynamic_cast<CmpInst *>(ret_val);
    if (cond_val == nullptr) {
      cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
    }
    builder->create_cond_br(cond_val, IF_While_Or_Cond_Stack.back().trueBB, falseBB);
    builder->set_insert_point(falseBB);
    node.rhs->accept(*this);
  }
  else {
    node.lhs->accept(*this);
    auto l_val = tmp_val;
    node.rhs->accept(*this);
    auto r_val = tmp_val;
    bool is_int = type_cast(builder, &l_val, &r_val);
    Value *cmp;
    switch (node.op) {
      case SyntaxTree::BinaryCondOp::LT:
        if (is_int)
          cmp = builder->create_icmp_lt(l_val, r_val);
        else  
          cmp = builder->create_fcmp_lt(l_val, r_val);
        break;
      case SyntaxTree::BinaryCondOp::LTE:
        if (is_int)
          cmp = builder->create_icmp_le(l_val, r_val);
        else  
          cmp = builder->create_fcmp_le(l_val, r_val);
        break;
      case SyntaxTree::BinaryCondOp::GTE:
        if (is_int)
          cmp = builder->create_icmp_ge(l_val, r_val);
        else  
          cmp = builder->create_fcmp_ge(l_val, r_val);
        break;
      case SyntaxTree::BinaryCondOp::GT:
        if (is_int)
          cmp = builder->create_icmp_gt(l_val, r_val);
        else  
          cmp = builder->create_fcmp_gt(l_val, r_val);
        break;
      case SyntaxTree::BinaryCondOp::EQ:
        if (is_int)
          cmp = builder->create_icmp_eq(l_val, r_val);
        else  
          cmp = builder->create_fcmp_eq(l_val, r_val);
        break;
      case SyntaxTree::BinaryCondOp::NEQ:
        if (is_int)
          cmp = builder->create_icmp_ne(l_val, r_val);
        else  
          cmp = builder->create_fcmp_ne(l_val, r_val);
        break;
      default: break;
    }
    tmp_val = builder->create_zext(cmp, INT32_T);
  }
}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) { //@cyy
  if (node.rhs == nullptr) 
    node.lhs->accept(*this);
  else {
    node.lhs->accept(*this);
    auto l_val = tmp_val;
    node.rhs->accept(*this);
    auto r_val = tmp_val;
    bool is_int = type_cast(builder, &l_val, &r_val);
    switch (node.op) {
    case SyntaxTree::BinOp::PLUS:
      if (is_int)
          tmp_val = builder->create_iadd(l_val, r_val);
      else
          tmp_val = builder->create_fadd(l_val, r_val);
      break;
    case SyntaxTree::BinOp::MINUS:
      if (is_int)
          tmp_val = builder->create_isub(l_val, r_val);
      else
          tmp_val = builder->create_fsub(l_val, r_val);
      break;
    }
    case SyntaxTree::BinOp::MULTIPLY:
      if (is_int)
          tmp_val = builder->create_imul(l_val, r_val);
      else
          tmp_val = builder->create_fmul(l_val, r_val);
      break;
    }
    case SyntaxTree::BinOp::DIVIDE:
      if (is_int)
          tmp_val = builder->create_isdiv(l_val, r_val);
      else
          tmp_val = builder->create_fdiv(l_val, r_val);
      break;
    }
    case SyntaxTree::BinOp::MODULO:
      tmp_val = builder->create_isrem(l_val, r_val);
      break;
    }
  }
}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) { //@cyy
  node.rhs->accept(*this);
  if (node.op == SyntaxTree::UnaryOp::MINUS) {
    if (tmp_val->get_type() == INT32_T) {
      auto val_const = dynamic_cast<ConstantInt *>(tmp_val);
      auto r_val = tmp_val;
      if (val_const != nullptr){
        tmp_val = CONST_INT(0 - val_const->get_value());
      }
      else{
        tmp_val = builder->create_isub(CONST_INT(0), r_val);
      }
    }
    else{ //FLOAT_T
      auto val_const = dynamic_cast<ConstantFloat *>(tmp_val);
      auto r_val = tmp_val;
      if (val_const != nullptr){
        tmp_val = CONST_FLOAT(0 - val_const->get_value());
      }
      else{
        tmp_val = builder->create_fsub(CONST_FLOAT(0), r_val);
      }
    }
  }
}


void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) { 
  auto func = static_cast<Function *>(scope.find(node.name, true));
  std::vector<Value *>args;
  auto param_type = func->get_function_type()->param_begin();
  for (auto &arg : node.params) {
    arg->accept(*this);
    if (!tmp_val->get_type()->is_pointer_type() && *param_type!=tmp_val->get_type()) {
      if (tmp_val->get_type()->is_pointer_type()) {
        tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
      }
      else {
        tmp_val = builder->create_fptosi(tmp_val, INT32_T);
      }
    }
    args.push_back(tmp_val);
    param_type++;
  }
}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {
  node.cond_exp->accept(*this);
  auto ret_val = tmp_val;
  auto trueBB = BasicBlock::create(module.get(), "true", cur_fun);
  auto falseBB = BasicBlock::create(module.get(), "false", cur_fun);
  auto nextBB = BasicBlock::create(module.get(), "if_next", cur_fun);
  Value *cond_val;
  if (ret_val->get_type()->is_integer_type())
    cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
  else
    cond_val = builder->create_fcmp_ne(ret_val, CONST_FLOAT(0.));

  if (node.else_statement == nullptr) {
    builder->create_cond_br(cond_val, trueBB, nextBB);
  } else {
    builder->create_cond_br(cond_val, trueBB, falseBB);
  }
  builder->set_insert_point(trueBB);
  node.if_statement->accept(*this);

  if (builder->get_insert_block()->get_terminator() == nullptr)
    builder->create_br(nextBB);

  if (node.else_statement == nullptr) {
    falseBB->erase_from_parent();
  } else {
    builder->set_insert_point(falseBB);
    node.else_statement->accept(*this);
    if (builder->get_insert_block()->get_terminator() == nullptr)
      builder->create_br(nextBB);
  }

  builder->set_insert_point(nextBB);
}

void IRBuilder::visit(SyntaxTree::WhileStmt &node) {
  auto condBB = BasicBlock::create(module.get(), "while_cond", cur_fun);
  if (builder->get_insert_block()->get_terminator() == nullptr)
    builder->create_br(condBB);
  builder->set_insert_point(condBB);
  node.cond_exp->accept(*this);
  auto ret_val = tmp_val;
  auto trueBB = BasicBlock::create(module.get(), "while_true", cur_fun);
  auto falseBB = BasicBlock::create(module.get(), "while_false", cur_fun);
  while_exit.push_back(falseBB);
  while_begin.push_back(condBB);
  Value *cond_val;
  if (ret_val->get_type()->is_integer_type())
    cond_val = builder->create_icmp_ne(ret_val, CONST_INT(0));
  else
    cond_val = builder->create_fcmp_ne(ret_val, CONST_FLOAT(0.));

  builder->create_cond_br(cond_val, trueBB, falseBB);
  builder->set_insert_point(trueBB);
  node.statement->accept(*this);
  if (builder->get_insert_block()->get_terminator() == nullptr)
    builder->create_br(condBB);
  builder->set_insert_point(falseBB);
  while_begin.pop_back();
  while_exit.pop_back();
}

void IRBuilder::visit(SyntaxTree::BreakStmt &node) {
  auto exit_ = while_exit.back();
  builder->create_br(exit_);
}

void IRBuilder::visit(SyntaxTree::ContinueStmt &node) {
  auto beg = while_begin.back();
  builder->create_br(beg);
}
