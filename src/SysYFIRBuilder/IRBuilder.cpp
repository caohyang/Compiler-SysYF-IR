#include "IRBuilder.h"
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

void IRBuilder::visit(SyntaxTree::InitVal &node) {}

void IRBuilder::visit(SyntaxTree::FuncDef &node) {}

void IRBuilder::visit(SyntaxTree::FuncFParamList &node) {}

void IRBuilder::visit(SyntaxTree::FuncParam &node) {}

void IRBuilder::visit(SyntaxTree::VarDef &node) {}

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
