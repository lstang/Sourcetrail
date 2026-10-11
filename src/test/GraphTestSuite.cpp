#include "Catch2.hpp"

#include "Graph.h"
#include "DummyNode.h"
#include "NodeOrderType.h"

namespace
{
class TestToken: public Token
{
public:
	TestToken(): Token(0) {}

	TestToken(const TestToken& other) = default;

	bool isNode() const override
	{
		return false;
	}

	bool isEdge() const override
	{
		return false;
	}

	void addComponent(std::shared_ptr<TokenComponent> component)
	{
		Token::addComponent(component);
	}

	template <typename ComponentType>
	std::shared_ptr<ComponentType> removeComponent()
	{
		return Token::removeComponent<ComponentType>();
	}

	std::string getReadableTypeString() const override
	{
		return "";
	}
};

class TestComponent: public TokenComponent
{
public:
	std::shared_ptr<TokenComponent> copy() const override
	{
		return std::make_shared<TestComponent>(*this);
	}
};

class Test2Component: public TokenComponent
{
public:
	std::shared_ptr<TokenComponent> copy() const override
	{
		return std::make_shared<Test2Component>(*this);
	}
};
}	 // namespace

TEST_CASE("tokens save location ids")
{
	TestToken a;
	a.addLocationId(23);
	a.addLocationId(5);

	REQUIRE(a.getLocationIds().size() == 2);
	REQUIRE(a.getLocationIds()[0] == 23);
	REQUIRE(a.getLocationIds()[1] == 5);
}

TEST_CASE("tokens remove location ids")
{
	TestToken a;
	a.addLocationId(23);
	a.addLocationId(5);
	a.removeLocationId(42);
	a.removeLocationId(5);

	REQUIRE(a.getLocationIds().size() == 1);
	REQUIRE(a.getLocationIds()[0] == 23);
}

TEST_CASE("token saves component")
{
	TestToken a;
	std::shared_ptr<TestComponent> component = std::make_shared<TestComponent>();
	a.addComponent(component);

	REQUIRE(a.getComponent<TestComponent>());
	REQUIRE(!a.getComponent<Test2Component>());

	REQUIRE(a.getComponent<TestComponent>() == component.get());
}

TEST_CASE("token saves multiple components")
{
	TestToken a;
	std::shared_ptr<TestComponent> component = std::make_shared<TestComponent>();
	std::shared_ptr<Test2Component> component2 = std::make_shared<Test2Component>();
	a.addComponent(component2);
	a.addComponent(component);

	REQUIRE(a.getComponent<TestComponent>());
	REQUIRE(a.getComponent<Test2Component>());

	REQUIRE(a.getComponent<TestComponent>() == component.get());
	REQUIRE(a.getComponent<Test2Component>() == component2.get());
}

TEST_CASE("token removes component")
{
	TestToken a;
	std::shared_ptr<TestComponent> component = std::make_shared<TestComponent>();
	std::shared_ptr<Test2Component> component2 = std::make_shared<Test2Component>();
	a.addComponent(component2);
	a.addComponent(component);
	std::shared_ptr<TestComponent> component3 = a.removeComponent<TestComponent>();

	REQUIRE(!a.getComponent<TestComponent>());
	REQUIRE(a.getComponent<Test2Component>());

	REQUIRE(component3.get() == component.get());
	REQUIRE(a.getComponent<Test2Component>() == component2.get());
}

TEST_CASE("token copies components when token is copied")
{
	TestToken a;
	std::shared_ptr<TestComponent> component = std::make_shared<TestComponent>();
	std::shared_ptr<Test2Component> component2 = std::make_shared<Test2Component>();
	a.addComponent(component2);
	a.addComponent(component);

	TestToken b(a);

	REQUIRE(b.getComponent<TestComponent>());
	REQUIRE(b.getComponent<Test2Component>());

	REQUIRE(b.getComponent<TestComponent>() != component.get());
	REQUIRE(b.getComponent<Test2Component>() != component2.get());
}

TEST_CASE("nodes are nodes")
{
	Node a(1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);

	REQUIRE(a.isNode());
	REQUIRE(!a.isEdge());
}

TEST_CASE("edges are edges")
{
	Node a(1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node b(2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Edge e(3, Edge::EDGE_USAGE, &a, &b);

	REQUIRE(!e.isNode());
	REQUIRE(e.isEdge());
}

TEST_CASE("set type of node from constructor")
{
	Node n(1, NodeType(NODE_FUNCTION), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	REQUIRE(NodeType(NODE_FUNCTION) == n.getType());
}

TEST_CASE("set type of node from non indexed")
{
	Node n(2, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	n.setType(NodeType(NODE_CLASS));
	REQUIRE(NodeType(NODE_CLASS) == n.getType());
}

TEST_CASE("can not change type of node after it was set")
{
	Node n(3, NodeType(NODE_NAMESPACE), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	n.setType(NodeType(NODE_CLASS));
	REQUIRE(NodeType(NODE_CLASS) != n.getType());
}

TEST_CASE("node can be copied and keeps same id")
{
	Node n(4, NodeType(NODE_NAMESPACE), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node n2(n);

	REQUIRE(&n != &n2);
	REQUIRE(n.getId() == n2.getId());
	REQUIRE(n.getName() == n2.getName());
	REQUIRE(n.getType() == n2.getType());
}

TEST_CASE("node type bit masking")
{
	Node n(1, NodeType(NODE_NAMESPACE), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	REQUIRE(n.isType(NODE_FUNCTION | NODE_NAMESPACE | NODE_CLASS));
	REQUIRE(!n.isType(NODE_FUNCTION | NODE_METHOD | NODE_CLASS));
}

TEST_CASE("get type of edges")
{
	Node a(1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node b(2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Edge e(3, Edge::EDGE_USAGE, &a, &b);

	REQUIRE(Edge::EDGE_USAGE == e.getType());
}

TEST_CASE("edge can be copied and keeps same id")
{
	Node a(1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node b(2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Edge e(3, Edge::EDGE_USAGE, &a, &b);
	Edge e2(e, &a, &b);

	REQUIRE(&e != &e2);
	REQUIRE(e.getId() == e2.getId());
	REQUIRE(e.getType() == e2.getType());
}

TEST_CASE("edge type bit masking")
{
	Node a(1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node b(2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Edge e(3, Edge::EDGE_USAGE, &a, &b);

	REQUIRE(e.isType(Edge::EDGE_MEMBER | Edge::EDGE_CALL | Edge::EDGE_USAGE));
	REQUIRE(!e.isType(Edge::EDGE_MEMBER | Edge::EDGE_CALL));
}

TEST_CASE("node finds child node")
{
	Node a(1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node b(2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node c(3, NodeType(NODE_SYMBOL), NameHierarchy("C", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Edge e(4, Edge::EDGE_MEMBER, &a, &b);
	Edge e2(5, Edge::EDGE_MEMBER, &a, &c);

	Node* x = a.findChildNode([](Node* n) { return n->getName() == "C"; });

	REQUIRE(x == &c);
	REQUIRE(x != &b);
}

TEST_CASE("node can not find child node")
{
	Node a(1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node b(2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node c(3, NodeType(NODE_SYMBOL), NameHierarchy("C", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Edge e(4, Edge::EDGE_MEMBER, &a, &b);
	Edge e2(5, Edge::EDGE_MEMBER, &a, &c);

	Node* x = a.findChildNode([](Node* n) { return n->getName() == "D"; });

	REQUIRE(!x);
}

TEST_CASE("node visits child nodes")
{
	Node a(1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node b(2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node c(3, NodeType(NODE_SYMBOL), NameHierarchy("C", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Edge e(4, Edge::EDGE_MEMBER, &a, &b);
	Edge e2(5, Edge::EDGE_MEMBER, &a, &c);

	std::vector<Node*> children;
	a.forEachChildNode([&children](Node* n) { return children.push_back(n); });

	REQUIRE(children.size() == 2);
	REQUIRE(children[0] == &b);
	REQUIRE(children[1] == &c);
}

TEST_CASE("graph saves nodes")
{
	Graph graph;
	Node* a = graph.createNode(
		1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node* b = graph.createNode(
		2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);

	REQUIRE(2 == graph.getNodeCount());
	REQUIRE(0 == graph.getEdgeCount());

	REQUIRE(graph.getNodeById(a->getId()));
	REQUIRE("A" == graph.getNodeById(a->getId())->getName());

	REQUIRE(graph.getNodeById(b->getId()));
	REQUIRE("B" == graph.getNodeById(b->getId())->getName());

	REQUIRE(!graph.getNodeById(0));
}

TEST_CASE("graph saves edges")
{
	Graph graph;

	Node* a = graph.createNode(
		1, NodeType(NODE_FUNCTION), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	Node* b = graph.createNode(
		2, NodeType(NODE_FUNCTION), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);

	Edge* e = graph.createEdge(3, Edge::EDGE_CALL, a, b);

	REQUIRE(2 == graph.getNodeCount());
	REQUIRE(1 == graph.getEdgeCount());

	REQUIRE(graph.getEdgeById(e->getId()));
	REQUIRE(Edge::EDGE_CALL == graph.getEdgeById(e->getId())->getType());
}

TEST_CASE("graph removes nodes")
{
	Graph graph;

	Node* a = graph.createNode(
		1, NodeType(NODE_SYMBOL), NameHierarchy("A", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);
	graph.createNode(
		2, NodeType(NODE_SYMBOL), NameHierarchy("B", NameDelimiterType::CXX), DefinitionKind::EXPLICIT);

	REQUIRE(2 == graph.getNodeCount());
	REQUIRE(0 == graph.getEdgeCount());

	graph.removeNode(graph.getNodeById(a->getId()));

	REQUIRE(1 == graph.getNodeCount());
}

TEST_CASE("dummy node ordering by name")
{
	auto a = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	a->name = "Beta";
	a->tokenId = 1;

	auto b = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	b->name = "Alpha";
	b->tokenId = 2;

	REQUIRE(!DummyNode::compareNodes(a, b, NodeOrderType::BY_NAME));
	REQUIRE(DummyNode::compareNodes(b, a, NodeOrderType::BY_NAME));
}

TEST_CASE("dummy node ordering by lines of code")
{
	auto a = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	a->name = "ShortMethod";
	a->lineCount = 10;
	a->tokenId = 1;

	auto b = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	b->name = "LongMethod";
	b->lineCount = 250;
	b->tokenId = 2;

	// Higher lines of code comes first (descending)
	REQUIRE(DummyNode::compareNodes(b, a, NodeOrderType::BY_LINES_OF_CODE));
	REQUIRE(!DummyNode::compareNodes(a, b, NodeOrderType::BY_LINES_OF_CODE));
}

TEST_CASE("dummy node ordering by callers")
{
	auto a = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	a->name = "PopularMethod";
	a->callerCount = 50;
	a->tokenId = 1;

	auto b = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	b->name = "RareMethod";
	b->callerCount = 2;
	b->tokenId = 2;

	REQUIRE(DummyNode::compareNodes(a, b, NodeOrderType::BY_CALLERS));
	REQUIRE(!DummyNode::compareNodes(b, a, NodeOrderType::BY_CALLERS));
}

TEST_CASE("dummy node ordering by callees")
{
	auto a = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	a->name = "LeafMethod";
	a->calleeCount = 0;
	a->tokenId = 1;

	auto b = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	b->name = "OrchestratorMethod";
	b->calleeCount = 35;
	b->tokenId = 2;

	REQUIRE(DummyNode::compareNodes(b, a, NodeOrderType::BY_CALLEES));
	REQUIRE(!DummyNode::compareNodes(a, b, NodeOrderType::BY_CALLEES));
}

TEST_CASE("dummy node ordering by callers and callees")
{
	auto a = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	a->name = "CentralNode";
	a->callerCount = 20;
	a->calleeCount = 15; // total 35
	a->tokenId = 1;

	auto b = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	b->name = "PeripheralNode";
	b->callerCount = 5;
	b->calleeCount = 5; // total 10
	b->tokenId = 2;

	REQUIRE(DummyNode::compareNodes(a, b, NodeOrderType::BY_CALLERS_AND_CALLEES));
	REQUIRE(!DummyNode::compareNodes(b, a, NodeOrderType::BY_CALLERS_AND_CALLEES));
}

TEST_CASE("dummy node sortSubNodes with different criteria")
{
	auto parent = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	parent->name = "Class";

	auto m1 = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	m1->name = "Zeta";
	m1->lineCount = 100;
	m1->callerCount = 1;
	m1->calleeCount = 20;
	m1->tokenId = 1;

	auto m2 = std::make_shared<DummyNode>(DummyNode::DUMMY_DATA);
	m2->name = "Alpha";
	m2->lineCount = 20;
	m2->callerCount = 50;
	m2->calleeCount = 2;
	m2->tokenId = 2;

	parent->subNodes = {m1, m2};

	// By Name: Alpha, then Zeta
	parent->sortSubNodes(NodeOrderType::BY_NAME);
	REQUIRE(parent->subNodes[0]->name == "Alpha");
	REQUIRE(parent->subNodes[1]->name == "Zeta");

	// By Lines of Code: Zeta (100), then Alpha (20)
	parent->sortSubNodes(NodeOrderType::BY_LINES_OF_CODE);
	REQUIRE(parent->subNodes[0]->name == "Zeta");
	REQUIRE(parent->subNodes[1]->name == "Alpha");

	// By Callers: Alpha (50), then Zeta (1)
	parent->sortSubNodes(NodeOrderType::BY_CALLERS);
	REQUIRE(parent->subNodes[0]->name == "Alpha");
	REQUIRE(parent->subNodes[1]->name == "Zeta");

	// By Callees: Zeta (20), then Alpha (2)
	parent->sortSubNodes(NodeOrderType::BY_CALLEES);
	REQUIRE(parent->subNodes[0]->name == "Zeta");
	REQUIRE(parent->subNodes[1]->name == "Alpha");

	// By Callers + Callees: Alpha (52), then Zeta (21)
	parent->sortSubNodes(NodeOrderType::BY_CALLERS_AND_CALLEES);
	REQUIRE(parent->subNodes[0]->name == "Alpha");
	REQUIRE(parent->subNodes[1]->name == "Zeta");
}
