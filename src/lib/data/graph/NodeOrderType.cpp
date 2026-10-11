#include "NodeOrderType.h"

std::string nodeOrderTypeToString(NodeOrderType type)
{
	switch (type)
	{
	case NodeOrderType::BY_LINES_OF_CODE:
		return "lines_of_code";
	case NodeOrderType::BY_CALLERS:
		return "callers";
	case NodeOrderType::BY_CALLEES:
		return "callees";
	case NodeOrderType::BY_CALLERS_AND_CALLEES:
		return "callers_and_callees";
	case NodeOrderType::BY_NAME:
	default:
		return "name";
	}
}

NodeOrderType stringToNodeOrderType(const std::string& value)
{
	if (value == "lines_of_code")
	{
		return NodeOrderType::BY_LINES_OF_CODE;
	}
	else if (value == "callers")
	{
		return NodeOrderType::BY_CALLERS;
	}
	else if (value == "callees")
	{
		return NodeOrderType::BY_CALLEES;
	}
	else if (value == "callers_and_callees")
	{
		return NodeOrderType::BY_CALLERS_AND_CALLEES;
	}
	return NodeOrderType::BY_NAME;
}
