#include "IRBuilder.h"
#include <vector>

#define CONST_INT(num) ConstantInt::get(num, module.get())
#define CONST_FLOAT(num) ConstantFloat::get(num, module.get())

// You can define global variables and functions here
// to store state

// store temporary value
bool require_lvalue = false;
// function that is being built
Function *cur_fun = nullptr;
// detect scope pre-enter (for elegance only)
bool pre_enter_scope = false;
// store temporary value
Value *tmp_val = nullptr;
// function that is being built
Function *cur_fun = nullptr;
// while exit
vector<BasicBlock *> while_exit;
// while begin
vector<BasicBlock *> while_bigen;
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

// used for backpatching
struct true_false_BB {
  BasicBlock *trueBB = nullptr;
  BasicBlock *falseBB = nullptr;
};

std::list<true_false_BB> IF_While_And_Cond_Stack; // used for Cond
std::list<true_false_BB> IF_While_Or_Cond_Stack; // used for Cond
std::list<true_false_BB> While_Stack;    // used for break and continue
// used for backpatching
std::vector<BasicBlock*> cur_basic_block_list;
std::vector<SyntaxTree::FuncParam> func_fparams;
std::vector<int> array_bounds;
std::vector<int> array_sizes;
int cur_pos;
int cur_depth;
std::map<int, Value *> initval;
std::vector<Constant *> init_val;

//ret BB
BasicBlock *ret_BB;
Value *ret_addr;

bool type_cast(IRBuilder *builder, Value **l_val_p, Value **r_val_p) {
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
    }
    else {
        if (cur_depth!=0) {
            while (cur_pos % array_sizes[cur_depth] != 0) {
            init_val.push_back(CONST_INT(0));
            cur_pos++;
            }
        }
        int cur_start_pos = cur_pos;
        for (const auto& elem : node.elementList) {
            cur_depth++;
            elem->accept(*this);
            cur_depth--;
        }
        if (cur_depth!=0) {
            while (cur_pos < cur_start_pos + array_sizes[cur_depth]) {
            init_val.push_back(CONST_INT(0));
            cur_pos++;
            }
        }
        if (cur_depth==0) {
            while (cur_pos < array_sizes[0]){
                init_val.push_back(CONST_INT(0));
                cur_pos++;
            }
        }
    }
}

void IRBuilder::visit(SyntaxTree::FuncDef &node) {
    FunctionType *fun_type;
    Type *ret_type;
    if (node.ret_type == SyntaxTree::Type::INT)
        ret_type = INT32_T;
    else
        ret_type = VOID_T;

    std::vector<Type *> param_types;
    std::vector<SyntaxTree::FuncParam>().swap(func_fparams);
    node.param_list->accept(*this);
    for (const auto& param : func_fparams) {
        if (param.param_type == SyntaxTree::Type::INT) {
            if (param.array_index.empty()) {
                param_types.push_back(INT32_T);
            }
            else {
                param_types.push_back(INT32PTR_T);
            }
        }
    }
    fun_type = FunctionType::get(ret_type, param_types);
    auto fun = Function::create(fun_type, node.name, module.get());
    scope.push_func(node.name, fun);
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
    int param_num = func_fparams.size();
    for (int i = 0; i < param_num; i++) {
        if (func_fparams[i].array_index.empty()) {
            Value *alloc;
            alloc = builder->create_alloca(INT32_T);
            builder->create_store(args[i], alloc);
            scope.push(func_fparams[i].name, alloc);
        }
        else {
            Value *alloc_array;
            alloc_array = builder->create_alloca(INT32PTR_T);
            builder->create_store(args[i], alloc_array);
            scope.push(func_fparams[i].name, alloc_array);
            array_bounds.clear();
            array_sizes.clear();
            for (auto bound_expr : func_fparams[i].array_index) {
                int bound;
                if (bound_expr==nullptr){
                    bound = 1;
                }
                else{
                    bound_expr->accept(*this);
                    auto bound_const = dynamic_cast<ConstantInt *>(tmp_val);
                    bound = bound_const->get_value();
                }
                array_bounds.push_back(bound);
            }
            int total_size = 1;
            for (auto iter = array_bounds.rbegin(); iter != array_bounds.rend();iter++) {
                array_sizes.insert(array_sizes.begin(), total_size);
                total_size *= (*iter);
            }
            array_sizes.insert(array_sizes.begin(), total_size);
            scope.push_size(func_fparams[i].name, array_sizes);
        }
    }
  //ret BB
    if (ret_type == INT32_T){
        ret_addr = builder->create_alloca(INT32_T);
    }
    ret_BB = BasicBlock::create(module.get(), "ret", fun);

    node.body->accept(*this);

    if (builder->get_insert_block()->get_terminator() == nullptr) {
        if (cur_fun->get_return_type()->is_void_type()){
            builder->create_br(ret_BB);
        }
        else {
            builder->create_store(CONST_INT(0), ret_addr);
            builder->create_br(ret_BB);
        }
    }
    scope.exit();
    cur_basic_block_list.pop_back();

  //ret BB
    builder->set_insert_point(ret_BB);
    if (fun->get_return_type()== VOID_T){
        builder->create_void_ret();
    }
    else{
        auto ret_val = builder->create_load(ret_addr);
        builder->create_ret(ret_val);
    }
}

void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {
    for (const auto& Param : node.params) {
        Param->accept(*this);
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
    cur_fun_entry_block = cur_fun->get_entry_block();   // entry block
    cur_fun_cur_block = cur_basic_block_list.back();                // current block
  }
  if (node.is_constant) {
    // constant
    Value *var;
    if (node.array_length.empty()){
      node.initializers->accept(*this);
      auto initializer = dynamic_cast<ConstantInt *>(tmp_val)->get_value();
      var = ConstantInt::get(initializer, module.get());
      scope.push(node.name, var);
    }
    else {
      //array
      array_bounds.clear();
      array_sizes.clear();
      for (const auto& bound_expr : node.array_length) {
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

      var_type = INT32_T;
      auto *array_type = ArrayType::get(var_type, total_size);

      initval.clear();
      init_val.clear();
      cur_depth = 0;
      cur_pos = 0;
      node.initializers->accept(*this);
      auto initializer = ConstantArray::get(array_type, init_val);

      if (scope.in_global()){
        var = GlobalVariable::create(node.name, module.get(), array_type, true, initializer);
        scope.push(node.name, var);
        scope.push_size(node.name, array_sizes);
        scope.push_const(node.name, initializer);
      }
      else {
        auto tmp_terminator = cur_fun_entry_block->get_terminator();
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->get_instructions().pop_back();
        }
        var = builder->create_alloca(array_type);
        cur_fun_cur_block->get_instructions().pop_back();
        cur_fun_entry_block->add_instruction(dynamic_cast<Instruction*>(var));
        dynamic_cast<Instruction*>(var)->set_parent(cur_fun_entry_block);
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->add_instruction(tmp_terminator);
        }
        for (int i = 0; i < array_sizes[0]; i++) {
          if (initval[i]) {
            builder->create_store(initval[i], builder->create_gep(var, {CONST_INT(0), CONST_INT(i)}));
          }
          else {
            builder->create_store(CONST_INT(0), builder->create_gep(var, {CONST_INT(0), CONST_INT(i)}));
          }
        }
        scope.push(node.name, var);
        scope.push_size(node.name, array_sizes);
        scope.push_const(node.name, initializer);
      }
    }
  }
  else {
    var_type = INT32_T;
    if (node.array_length.empty()) {
      Value *var;
      if (scope.in_global()) {
        if (node.is_inited) {
          node.initializers->accept(*this);
          auto initializer = dynamic_cast<ConstantInt *>(tmp_val);
          var = GlobalVariable::create(node.name, module.get(), var_type, false, initializer);
          scope.push(node.name, var);
        }
        else{
          auto initializer = ConstantZero::get(var_type, module.get());
          var = GlobalVariable::create(node.name, module.get(), var_type, false, initializer);
          scope.push(node.name, var);
        }
      }
      else {
        auto tmp_terminator = cur_fun_entry_block->get_terminator();
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->get_instructions().pop_back();
        }
        var = builder->create_alloca(var_type);
        cur_fun_cur_block->get_instructions().pop_back();
        cur_fun_entry_block->add_instruction(dynamic_cast<Instruction*>(var));
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->add_instruction(tmp_terminator);
        }
        if (node.is_inited) {
          node.initializers->accept(*this);
          builder->create_store(tmp_val, var);
        }
        scope.push(node.name, var);
      }
    }
    else {
      // array
      array_bounds.clear();
      array_sizes.clear();
      for (const auto& bound_expr : node.array_length) {
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
        if (node.is_inited ){
          cur_pos = 0;
          cur_depth = 0;
          initval.clear();
          init_val.clear();
          node.initializers->accept(*this);
          auto initializer = ConstantArray::get(array_type, init_val);
          var = GlobalVariable::create(node.name, module.get(), array_type, false, initializer);
          scope.push(node.name, var);
          scope.push_size(node.name, array_sizes);
        }
        else {
          auto initializer = ConstantZero::get(array_type, module.get());
          var = GlobalVariable::create(node.name, module.get(), array_type, false, initializer);
          scope.push(node.name, var);
          scope.push_size(node.name, array_sizes);
        }
      }
      else {
        auto tmp_terminator = cur_fun_entry_block->get_terminator();
        if (tmp_terminator != nullptr) {
          cur_fun_entry_block->get_instructions().pop_back();
        }
        var = builder->create_alloca(array_type);
        cur_fun_cur_block->get_instructions().pop_back();
        cur_fun_entry_block->add_instruction(dynamic_cast<Instruction*>(var));
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
              builder->create_store(initval[i], builder->create_gep(var, {CONST_INT(0), CONST_INT(i)}));
            }
            else {
              builder->create_store(CONST_INT(0), builder->create_gep(var, {CONST_INT(0), CONST_INT(i)}));
            }
          }
        }
        scope.push(node.name, var);
        scope.push_size(node.name, array_sizes);
      }//if of global check
    }
  }
}

void IRBuilder::visit(SyntaxTree::LVal &node) {}

void IRBuilder::visit(SyntaxTree::AssignStmt &node) {}

void IRBuilder::visit(SyntaxTree::Literal &node) {
  if (node.literal_type == TYPE_INT)
    tmp_val = CONST_INT(node.int_const);
  else
    tmp_val = CONST_FLOAT(node.float_const);
}

void IRBuilder::visit(SyntaxTree::ReturnStmt &node) {}

void IRBuilder::visit(SyntaxTree::BlockStmt &node) {}

void IRBuilder::visit(SyntaxTree::EmptyStmt &node) {}

void IRBuilder::visit(SyntaxTree::ExprStmt &node) {}

void IRBuilder::visit(SyntaxTree::UnaryCondExpr &node) {}

void IRBuilder::visit(SyntaxTree::BinaryCondExpr &node) {}

void IRBuilder::visit(SyntaxTree::BinaryExpr &node) {}

void IRBuilder::visit(SyntaxTree::UnaryExpr &node) {}

void IRBuilder::visit(SyntaxTree::FuncCallStmt &node) {}

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
