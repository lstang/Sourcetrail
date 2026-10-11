#ifndef NODE_ORDER_TYPE_H
#define NODE_ORDER_TYPE_H

#include <string>

enum class NodeOrderType
{
	BY_NAME = 0,
	BY_LINES_OF_CODE = 1,
	BY_CALLERS = 2,
	BY_CALLEES = 3,
	BY_CALLERS_AND_CALLEES = 4
};

std::string nodeOrderTypeToString(NodeOrderType type);
NodeOrderType stringToNodeOrderType(const std::string& value);

#endif	// NODE_ORDER_TYPE_H
