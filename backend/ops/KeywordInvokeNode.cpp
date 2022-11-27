#include "../codegen.h"  

using namespace std;
using namespace llvm;

TypedValue CodeGenerator::codegen(const Node &node, const KeywordInvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto &functionBody = subnode.target();
  
  InvokeNode *invoke = new InvokeNode();
  Node *bodyCopy = new Node(functionBody);
  invoke->set_allocated_fn(bodyCopy);
  Node *arg = invoke->add_args();
  *arg = subnode.keyword();

  Subnode *invokeSubnode = new Subnode();
  invokeSubnode->set_allocated_invoke(invoke);

  Node invokeNode = Node(node);
  invokeNode.set_op(opInvoke);
  invokeNode.set_allocated_subnode(invokeSubnode);
  return codegen(invokeNode, *invoke, typeRestrictions);
}

ObjectTypeSet CodeGenerator::getType(const Node &node, const KeywordInvokeNode &subnode, const ObjectTypeSet &typeRestrictions) {
  auto &functionBody = subnode.target();
  
  InvokeNode *invoke = new InvokeNode();
  Node *bodyCopy = new Node(functionBody);
  invoke->set_allocated_fn(bodyCopy);
  Node *arg = invoke->add_args();
  *arg = subnode.keyword();

  Subnode *invokeSubnode = new Subnode();
  invokeSubnode->set_allocated_invoke(invoke);

  Node invokeNode = Node(node);
  invokeNode.set_op(opInvoke);
  invokeNode.set_allocated_subnode(invokeSubnode);
  return getType(invokeNode, *invoke, typeRestrictions);
}
