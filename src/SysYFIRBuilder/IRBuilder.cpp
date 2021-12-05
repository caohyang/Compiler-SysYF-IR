#include "IRBuilder.h"
#include <SyntaxTree.h>
#include <vector>

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

#ifdef DEBUG // 用于调试信息,大家可以在编译过程中通过" -DDEBUG"来开启这一选项
#define DEBUG_OUTPUT std::cout << __LINE__ << std::endl; // 输出行号的简单示例
#else
#define DEBUG_OUTPUT
#endif

// You can define global variables and functions here
// to store state

// store temporary value
Value *tmp_val = nullptr;
// function that is being built
Function *cur_fun = nullptr;
// while exit
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

// used for backpatching
struct true_false_BB {
  BasicBlock *trueBB = nullptr;
  BasicBlock *falseBB = nullptr;
};

std::list<true_false_BB> IF_While_And_Cond_Stack; // used for Cond
std::list<true_false_BB> IF_While_Or_Cond_Stack;  // used for Cond
std::list<true_false_BB> While_Stack;             // used for break and continue
// used for backpatching
std::vector<BasicBlock *> cur_basic_block_list;
std::vector<SyntaxTree::FuncParam> func_fparams;
std::vector<int> array_bounds;
std::vector<int> array_sizes;
int cur_pos;
int cur_depth;
std::map<int, Value *> initval;
std::vector<Constant *> init_val;

// ret BB
BasicBlock *ret_BB;
Value *ret_addr;

bool type_cast(IRStmtBuilder *builder, Value **l_val_p, Value **r_val_p) {
  bool is_int;
  auto &l_val = *l_val_p;
  auto &r_val = *r_val_p;
  if (l_val->get_type() == r_val->get_type()) {
    is_int = l_val->get_type()->is_integer_type();
  } else {
    is_int = false;
    if (l_val->get_type()->is_integer_type())
      l_val = builder->create_sitofp(l_val, FLOAT_T);
    else
      r_val = builder->create_sitofp(r_val, FLOAT_T);
  }
  return is_int;
}

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

void IRBuilder::visit(SyntaxTree::InitVal &node) {
  if (node.isExp) {
    node.expr->accept(*this);
    initval[cur_pos] = tmp_val;
    init_val.push_back(dynamic_cast<Constant *>(tmp_val));
    cur_pos++;
  } else {
    if (cur_depth != 0) {
      while (cur_pos % array_sizes[cur_depth] != 0) {
        init_val.push_back(CONST_INT(0));
        cur_pos++;
      }
    }
    int cur_start_pos = cur_pos;
    for (const auto &elem : node.elementList) {
      cur_depth++;
      elem->accept(*this);
      cur_depth--;
    }
    if (cur_depth != 0) {
      while (cur_pos < cur_start_pos + array_sizes[cur_depth]) {
        init_val.push_back(CONST_INT(0));
        cur_pos++;
      }
    } else { // if (cur_depth==0)
      while (cur_pos < array_sizes[0]) {
        init_val.push_back(CONST_INT(0));
        cur_pos++;
      }
    }
  }
}

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
    if (param->param_type == SyntaxTree::Type::INT) {
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
  cur_basic_block_list.push_back(funBB);
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
      if (node.param_list->params[i]->param_type == SyntaxTree::Type::INT)
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

void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {
  for (const auto &param : node.params) {
    param->accept(*this);
  }
}

void IRBuilder::visit(SyntaxTree::FuncParam &node) {
  func_fparams.push_back(node);
}

void IRBuilder::visit(SyntaxTree::VarDef &node) {
  Type *var_type;
  BasicBlock *cur_fun_entry_block;
  BasicBlock *cur_fun_cur_block;
  if (scope.in_global() == false) {
    cur_fun_entry_block = cur_fun->get_entry_block(); // entry block
    cur_fun_cur_block = cur_basic_block_list.back();  // current block
  }
  if (node.is_constant) {
    Value *var;
    if (node.array_length.empty()) {
      node.initializers->accept(*this);
      if (node.btype == SyntaxTree::Type::INT)
        var = ConstantInt::get(
            dynamic_cast<ConstantInt *>(tmp_val)->get_value(), module.get());
      else
        var = ConstantFloat::get(
            dynamic_cast<ConstantFloat *>(tmp_val)->get_value(), module.get());
      scope.push(node.name, var);
    } else { // array
      array_bounds.clear();
      array_sizes.clear();
      for (const auto &bound_expr : node.array_length) {
        bound_expr->accept(*this);
        auto bound_const = dynamic_cast<ConstantInt *>(tmp_val);
        auto bound = bound_const->get_value();
        array_bounds.push_back(bound);
      }
      int total_size = 1;
      for (auto iter = array_bounds.rbegin(); iter != array_bounds.rend();
           iter++) {
        array_sizes.insert(array_sizes.begin(), total_size);
        total_size *= (*iter);
      }
      array_sizes.insert(array_sizes.begin(), total_size);

      if (node.btype == SyntaxTree::Type::INT)
        var_type = INT32_T;
      else
        var_type = FLOAT_T;
      auto *array_type = ArrayType::get(var_type, total_size);

      initval.clear();
      init_val.clear();
      cur_depth = 0;
      cur_pos = 0;
      node.initializers->accept(*this);
      auto initializer = ConstantArray::get(array_type, init_val);

      if (scope.in_global()) {
        var = GlobalVariable::create(node.name, module.get(), array_type, true,
                                     initializer);
        scope.push(node.name, var);
        // scope.push_size(node.name, array_sizes);
        // scope.push_const(node.name, initializer);
      } else {
        auto tmp_terminator = cur_fun_entry_block->get_terminator();
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->get_instructions().pop_back();
        }
        var = builder->create_alloca(array_type);
        cur_fun_cur_block->get_instructions().pop_back();
        cur_fun_entry_block->add_instruction(dynamic_cast<Instruction *>(var));
        dynamic_cast<Instruction *>(var)->set_parent(cur_fun_entry_block);
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->add_instruction(tmp_terminator);
        }
        for (int i = 0; i < array_sizes[0]; i++) {
          if (initval[i]) {
            builder->create_store(
                initval[i],
                builder->create_gep(var, {CONST_INT(0), CONST_INT(i)}));
          } else {
            builder->create_store(
                CONST_INT(0),
                builder->create_gep(var, {CONST_INT(0), CONST_INT(i)}));
          }
        }
        scope.push(node.name, var);
        // scope.push_size(node.name, array_sizes);
        // scope.push_const(node.name, initializer);
      }
    }
  }

  else {
    if (node.btype == SyntaxTree::Type::INT)
      var_type = INT32_T;
    else
      var_type = FLOAT_T;
    if (node.array_length.empty()) {
      Value *var;
      if (scope.in_global()) {
        if (node.is_inited) {
          node.initializers->accept(*this);
          if (var_type == INT32_T)
            var =
                GlobalVariable::create(node.name, module.get(), var_type, false,
                                       dynamic_cast<ConstantInt *>(tmp_val));
          else
            var =
                GlobalVariable::create(node.name, module.get(), var_type, false,
                                       dynamic_cast<ConstantFloat *>(tmp_val));
          scope.push(node.name, var);
        } else {
          auto initializer = ConstantZero::get(var_type, module.get());
          var = GlobalVariable::create(node.name, module.get(), var_type, false,
                                       initializer);
          scope.push(node.name, var);
        }
      } else {
        auto tmp_terminator = cur_fun_entry_block->get_terminator();
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->get_instructions().pop_back();
        }
        var = builder->create_alloca(var_type);
        cur_fun_cur_block->get_instructions().pop_back();
        cur_fun_entry_block->add_instruction(dynamic_cast<Instruction *>(var));
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->add_instruction(tmp_terminator);
        }
        if (node.is_inited) {
          node.initializers->accept(*this);
          if (var->get_type() != tmp_val->get_type()) {
            if (var->get_type() == FLOAT_T) {
              tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
            } else {
              tmp_val = builder->create_fptosi(tmp_val, INT32_T);
            }
          }
          builder->create_store(tmp_val, var);
        }
        scope.push(node.name, var);
      }
    } else { // array
      array_bounds.clear();
      array_sizes.clear();
      for (const auto &bound_expr : node.array_length) {
        bound_expr->accept(*this);
        auto bound_const = dynamic_cast<ConstantInt *>(tmp_val);
        auto bound = bound_const->get_value();
        array_bounds.push_back(bound);
      }
      int total_size = 1;
      for (auto iter = array_bounds.rbegin(); iter != array_bounds.rend();
           iter++) {
        array_sizes.insert(array_sizes.begin(), total_size);
        total_size *= (*iter);
      }
      array_sizes.insert(array_sizes.begin(), total_size);
      auto *array_type = ArrayType::get(var_type, total_size);

      Value *var;
      if (scope.in_global()) {
        if (node.is_inited) {
          cur_pos = 0;
          cur_depth = 0;
          initval.clear();
          init_val.clear();
          node.initializers->accept(*this);
          auto initializer = ConstantArray::get(array_type, init_val);
          var = GlobalVariable::create(node.name, module.get(), array_type,
                                       false, initializer);
          scope.push(node.name, var);
          // scope.push_size(node.name, array_sizes);
        } else {
          auto initializer = ConstantZero::get(array_type, module.get());
          var = GlobalVariable::create(node.name, module.get(), array_type,
                                       false, initializer);
          scope.push(node.name, var);
          // scope.push_size(node.name, array_sizes);
        }
      } else {
        auto tmp_terminator = cur_fun_entry_block->get_terminator();
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->get_instructions().pop_back();
        }
        var = builder->create_alloca(array_type);
        cur_fun_cur_block->get_instructions().pop_back();
        cur_fun_entry_block->add_instruction(dynamic_cast<Instruction *>(var));
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->add_instruction(tmp_terminator);
        }
        if (node.is_inited) {
          cur_pos = 0;
          cur_depth = 0;
          initval.clear();
          init_val.clear();
          node.initializers->accept(*this);
          for (int i = 0; i < array_sizes[0]; i++) {
            if (initval[i]) {
              builder->create_store(
                  initval[i],
                  builder->create_gep(var, {CONST_INT(0), CONST_INT(i)}));
            } else {
              builder->create_store(
                  CONST_INT(0),
                  builder->create_gep(var, {CONST_INT(0), CONST_INT(i)}));
            }
          }
        }
        scope.push(node.name, var);
        // scope.push_size(node.name, array_sizes);
      } // if of global check
    }
  }
}

void IRBuilder::visit(SyntaxTree::LVal &node) {
  auto var = scope.find(node.name, false); // isfunc = false
  bool should_return_lvalue = require_lvalue;
  require_lvalue = false;
  if (node.array_index.empty()) {
    if (should_return_lvalue) {
      if (var->get_type()->get_pointer_element_type()->is_array_type()) {
        tmp_val = builder->create_gep(var, {CONST_INT(0), CONST_INT(0)});
      } else if (var->get_type()
                     ->get_pointer_element_type()
                     ->is_pointer_type()) {
        tmp_val = builder->create_load(var);
      } else {
        tmp_val = var;
      }
      require_lvalue = false;
    } else {
      auto val_constint = dynamic_cast<ConstantInt *>(var);
      auto val_constfloat = dynamic_cast<ConstantFloat *>(var);
      if (val_constint != nullptr) {
        tmp_val = val_constint;
      } else if (val_constfloat != nullptr) {
        tmp_val = val_constfloat;
      } else {
        tmp_val = builder->create_load(var);
      }
    }
  } else { // only deal with one-dimension array
    node.array_index[0]->accept(*this);
    auto var_with_index =
        builder->create_gep(var, {CONST_INT(0), CONST_INT(tmp_val)});
    if (should_return_lvalue) {
      if (var->get_type()->get_pointer_element_type()->is_array_type()) {
        tmp_val = builder->create_gep(var, {CONST_INT(0), CONST_INT(tmp_val)});
      } else if (var->get_type()
                     ->get_pointer_element_type()
                     ->is_pointer_type()) {
        tmp_val = builder->create_load(var_with_index);
      } else {
        tmp_val = var_with_index;
      }
      require_lvalue = false;
    } else {
      auto val_const = dynamic_cast<Constant *>(var_with_index);
      if (val_const != nullptr) {
        tmp_val = val_const;
      } else {
        tmp_val = builder->create_load(var_with_index);
      }
    }
  }
}

void IRBuilder::visit(SyntaxTree::AssignStmt &node) {
  node.value->accept(*this);
  auto expr_result = tmp_val;
  require_lvalue = true;
  node.target->accept(*this);
  auto var_addr = tmp_val;

  if (var_addr->get_type()->get_pointer_element_type() !=
      expr_result->get_type()) {
    if (expr_result->get_type() == INT32_T) {
      expr_result = builder->create_sitofp(expr_result, FLOAT_T);
    } else {
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
      } else {
        tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
      }
    }
    builder->create_ret(tmp_val);
  }
}

void IRBuilder::visit(SyntaxTree::BlockStmt &node) {
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

void IRBuilder::visit(SyntaxTree::EmptyStmt &node) { tmp_val = nullptr; }

void IRBuilder::visit(SyntaxTree::ExprStmt &node) { node.exp->accept(*this); }

void IRBuilder::visit(SyntaxTree::UnaryCondExpr &node) {
  if (node.op == SyntaxTree::UnaryCondOp::NOT) {
    node.rhs->accept(*this);
    auto r_val = tmp_val;
    tmp_val = builder->create_icmp_eq(r_val, CONST_INT(0));
  }
}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) {
  CmpInst *cond_val;
  if (node.op == SyntaxTree::BinaryCondOp::LAND) {
    auto trueBB = BasicBlock::create(module.get(), "and_lhs_true", cur_fun);
    IF_While_And_Cond_Stack.push_back(
        {trueBB, IF_While_Or_Cond_Stack.back().falseBB});
    node.lhs->accept(*this);
    IF_While_And_Cond_Stack.pop_back();
    auto ret_val = tmp_val;
    cond_val = dynamic_cast<CmpInst *>(ret_val);
    if (cond_val == nullptr) {
      cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
    }
    builder->create_cond_br(cond_val, trueBB,
                            IF_While_Or_Cond_Stack.back().falseBB);
    builder->set_insert_point(trueBB);
    node.rhs->accept(*this);
  } else if (node.op == SyntaxTree::BinaryCondOp::LOR) {
    auto falseBB = BasicBlock::create(module.get(), "and_lhs_false", cur_fun);
    IF_While_Or_Cond_Stack.push_back(
        {IF_While_Or_Cond_Stack.back().trueBB, falseBB});
    node.lhs->accept(*this);
    IF_While_Or_Cond_Stack.pop_back();
    auto ret_val = tmp_val;
    cond_val = dynamic_cast<CmpInst *>(ret_val);
    if (cond_val == nullptr) {
      cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
    }
    builder->create_cond_br(cond_val, IF_While_Or_Cond_Stack.back().trueBB,
                            falseBB);
    builder->set_insert_point(falseBB);
    node.rhs->accept(*this);
  } else {
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
    default:
      break;
    }
    tmp_val = builder->create_zext(cmp, INT32_T);
  }
}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) {
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
    case SyntaxTree::BinOp::MULTIPLY:
      if (is_int)
        tmp_val = builder->create_imul(l_val, r_val);
      else
        tmp_val = builder->create_fmul(l_val, r_val);
      break;
    case SyntaxTree::BinOp::DIVIDE:
      if (is_int)
        tmp_val = builder->create_isdiv(l_val, r_val);
      else
        tmp_val = builder->create_fdiv(l_val, r_val);
      break;
    case SyntaxTree::BinOp::MODULO:
      tmp_val = builder->create_isrem(l_val, r_val);
      break;
    }
  }
}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) {
  node.rhs->accept(*this);
  if (node.op == SyntaxTree::UnaryOp::MINUS) {
    if (tmp_val->get_type() == INT32_T) {
      auto val_const = dynamic_cast<ConstantInt *>(tmp_val);
      auto r_val = tmp_val;
      if (val_const != nullptr) {
        tmp_val = CONST_INT(0 - val_const->get_value());
      } else {
        tmp_val = builder->create_isub(CONST_INT(0), r_val);
      }
    } else { // FLOAT_T
      auto val_const = dynamic_cast<ConstantFloat *>(tmp_val);
      auto r_val = tmp_val;
      if (val_const != nullptr) {
        tmp_val = CONST_FLOAT(0 - val_const->get_value());
      } else {
        tmp_val = builder->create_fsub(CONST_FLOAT(0), r_val);
      }
    }
  }
}

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {
  auto func = static_cast<Function *>(scope.find(node.name, true));
  std::vector<Value *> args;
  auto param_type = func->get_function_type()->param_begin();
  for (auto &arg : node.params) {
    arg->accept(*this);
    if (!tmp_val->get_type()->is_pointer_type() &&
        *param_type != tmp_val->get_type()) {
      if (tmp_val->get_type()->is_pointer_type()) {
        tmp_val = builder->create_sitofp(tmp_val, FLOAT_T);
      } else {
        tmp_val = builder->create_fptosi(tmp_val, INT32_T);
      }
    }
    args.push_back(tmp_val);
    param_type++;
  }
}

void IRBuilder::visit(SyntaxTree::IfStmt &node) {
  auto trueBB = BasicBlock::create(module.get(), "true", cur_fun);
  auto falseBB = BasicBlock::create(module.get(), "false", cur_fun);
  auto nextBB = BasicBlock::create(module.get(), "if_next", cur_fun);
  IF_While_Or_Cond_Stack.push_back({nullptr, nullptr});
  IF_While_Or_Cond_Stack.back().trueBB = trueBB;
  if (node.else_statement == nullptr) {
    IF_While_Or_Cond_Stack.back().falseBB = nextBB;
  } else {
    IF_While_Or_Cond_Stack.back().falseBB = falseBB;
  }
  node.cond_exp->accept(*this);
  IF_While_Or_Cond_Stack.pop_back();
  auto ret_val = tmp_val;
  auto *cond_val = dynamic_cast<CmpInst *>(ret_val);
  if (cond_val == nullptr) {
    cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
  }
  if (node.else_statement == nullptr) {
    builder->create_cond_br(cond_val, trueBB, nextBB);
  } else {
    builder->create_cond_br(cond_val, trueBB, falseBB);
  }
  cur_basic_block_list.pop_back();
  builder->set_insert_point(trueBB);
  cur_basic_block_list.push_back(trueBB);
  if (dynamic_cast<SyntaxTree::BlockStmt *>(node.if_statement.get())) {
    node.if_statement->accept(*this);
  } else {
    scope.enter();
    node.if_statement->accept(*this);
    scope.exit();
  }

  if (builder->get_insert_block()->get_terminator() == nullptr) {
    builder->create_br(nextBB);
  }
  cur_basic_block_list.pop_back();

  if (node.else_statement == nullptr) {
    falseBB->erase_from_parent();
  } else {
    builder->set_insert_point(falseBB);
    cur_basic_block_list.push_back(falseBB);
    if (dynamic_cast<SyntaxTree::BlockStmt *>(node.else_statement.get())) {
      node.else_statement->accept(*this);
    } else {
      scope.enter();
      node.else_statement->accept(*this);
      scope.exit();
    }
    if (builder->get_insert_block()->get_terminator() == nullptr) {
      builder->create_br(nextBB);
    }
    cur_basic_block_list.pop_back();
  }

  builder->set_insert_point(nextBB);
  cur_basic_block_list.push_back(nextBB);
  if (nextBB->get_pre_basic_blocks().size() == 0) {
    builder->set_insert_point(trueBB);
    nextBB->erase_from_parent();
  }
}

void IRBuilder::visit(SyntaxTree::WhileStmt &node) {
  auto whileBB = BasicBlock::create(module.get(), "cond_bb", cur_fun);
  auto trueBB = BasicBlock::create(module.get(), "while_true", cur_fun);
  auto nextBB = BasicBlock::create(module.get(), "while_false", cur_fun);
  While_Stack.push_back({whileBB, nextBB});
  if (builder->get_insert_block()->get_terminator() == nullptr) {
    builder->create_br(whileBB);
  }
  cur_basic_block_list.pop_back();
  builder->set_insert_point(whileBB);
  IF_While_Or_Cond_Stack.push_back({trueBB, nextBB});
  node.cond_exp->accept(*this);
  IF_While_Or_Cond_Stack.pop_back();
  auto ret_val = tmp_val;
  auto *cond_val = dynamic_cast<CmpInst *>(ret_val);
  if (cond_val == nullptr) {
    cond_val = builder->create_icmp_ne(tmp_val, CONST_INT(0));
  }
  builder->create_cond_br(cond_val, trueBB, nextBB);
  builder->set_insert_point(trueBB);
  cur_basic_block_list.push_back(trueBB);
  if (dynamic_cast<SyntaxTree::BlockStmt *>(node.statement.get())) {
    node.statement->accept(*this);
  } else {
    scope.enter();
    node.statement->accept(*this);
    scope.exit();
  }
  if (builder->get_insert_block()->get_terminator() == nullptr) {
    builder->create_br(whileBB);
  }
  cur_basic_block_list.pop_back();
  builder->set_insert_point(nextBB);
  cur_basic_block_list.push_back(nextBB);
  While_Stack.pop_back();
}

void IRBuilder::visit(SyntaxTree::BreakStmt &node) {
  builder->create_br(While_Stack.back().falseBB);
}

void IRBuilder::visit(SyntaxTree::ContinueStmt &node) {
  builder->create_br(While_Stack.back().trueBB);
}
