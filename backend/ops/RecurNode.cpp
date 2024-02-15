#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const RecurNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto uniqueId = TheProgramme->RecurTargets.find(subnode.loopid())->second;
  auto &functionBody = TheProgramme->Functions.find(uniqueId)->second; // TODO: Fix functionBody
  
  InvokeNode *invoke = new InvokeNode();
  Node *bodyCopy = new Node(functionBody);
  invoke->set_allocated_fn(bodyCopy);
  for(int i=0; i<subnode.exprs_size();i++) {
    Node *arg = invoke->add_args();
    *arg = subnode.exprs(i);
  }

  Subnode *invokeSubnode = new Subnode();
  invokeSubnode->set_allocated_invoke(invoke);

  Node invokeNode = Node(node);
  invokeNode.set_op(opInvoke);
  invokeNode.set_allocated_subnode(invokeSubnode);
  return codegen(invokeNode, *invoke, typeRestrictions);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const RecurNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto uniqueId = TheProgramme->RecurTargets.find(subnode.loopid())->second;
  auto &functionBody = TheProgramme->Functions.find(uniqueId)->second;
  
  InvokeNode *invoke = new InvokeNode();
  Node *bodyCopy = new Node(functionBody);
  invoke->set_allocated_fn(bodyCopy);
  for(int i=0; i<subnode.exprs_size();i++) {
    Node *arg = invoke->add_args();
    *arg = subnode.exprs(i);
  }

  Subnode *invokeSubnode = new Subnode();
  invokeSubnode->set_allocated_invoke(invoke);

  Node invokeNode = Node(node);
  invokeNode.set_op(opInvoke);
  invokeNode.set_allocated_subnode(invokeSubnode);
  return getType(invokeNode, *invoke, typeRestrictions);
}
