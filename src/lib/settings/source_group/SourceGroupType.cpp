#include "SourceGroupType.h"

std::string sourceGroupTypeToString(SourceGroupType v)
{
	switch (v)
	{
#if BUILD_CXX_LANGUAGE_PACKAGE
	case SourceGroupType::C_EMPTY:
		return "C Source Group";
	case SourceGroupType::CXX_EMPTY:
		return "C++ Source Group";
	case SourceGroupType::CXX_CDB:
		return "C/C++ from Compilation Database";
	case SourceGroupType::CXX_CODEBLOCKS:
		return "C/C++ from Code::Blocks";
	case SourceGroupType::CXX_VS:
		return "C/C++ from Visual Studio";
#endif	  // BUILD_CXX_LANGUAGE_PACKAGE
#if BUILD_JAVA_LANGUAGE_PACKAGE
	case SourceGroupType::JAVA_EMPTY:
		return "Java Source Group";
	case SourceGroupType::JAVA_MAVEN:
		return "Java Source Group from Maven";
	case SourceGroupType::JAVA_GRADLE:
		return "Java Source Group from Gradle";
#endif	  // BUILD_JAVA_LANGUAGE_PACKAGE
#if BUILD_PYTHON_LANGUAGE_PACKAGE
	case SourceGroupType::PYTHON_EMPTY:
		return "Python Source Group";
#endif	  // BUILD_PYTHON_LANGUAGE_PACKAGE
	case SourceGroupType::CSHARP_EMPTY:
		return "C# Source Group";
	case SourceGroupType::TREE_SITTER:
		return "Tree-sitter Source Group";
	case SourceGroupType::CUSTOM_COMMAND:
		return "Custom Command Source Group";
	case SourceGroupType::UNKNOWN:
		break;
	}
	return "unknown";
}

std::string sourceGroupTypeToProjectSetupString(SourceGroupType v)
{
	switch (v)
	{
#if BUILD_CXX_LANGUAGE_PACKAGE
	case SourceGroupType::C_EMPTY:
		return "Empty C Source Group";
	case SourceGroupType::CXX_EMPTY:
		return "Empty C++ Source Group";
	case SourceGroupType::CXX_CDB:
		return "C/C++ from Compilation Database";
	case SourceGroupType::CXX_CODEBLOCKS:
		return "C/C++ from Code::Blocks";
	case SourceGroupType::CXX_VS:
		return "C/C++ from Visual Studio";
#endif	  // BUILD_CXX_LANGUAGE_PACKAGE
#if BUILD_JAVA_LANGUAGE_PACKAGE
	case SourceGroupType::JAVA_EMPTY:
		return "Empty Java Source Group";
	case SourceGroupType::JAVA_MAVEN:
		return "Java Source Group from Maven";
	case SourceGroupType::JAVA_GRADLE:
		return "Java Source Group from Gradle";
#endif	  // BUILD_JAVA_LANGUAGE_PACKAGE
#if BUILD_PYTHON_LANGUAGE_PACKAGE
	case SourceGroupType::PYTHON_EMPTY:
		return "Empty Python Source Group";
#endif	  // BUILD_PYTHON_LANGUAGE_PACKAGE
	case SourceGroupType::CSHARP_EMPTY:
		return "Empty C# Source Group";
	case SourceGroupType::TREE_SITTER:
		return "Tree-sitter Source Group";
	case SourceGroupType::CUSTOM_COMMAND:
		return "Custom Command Source Group";
	case SourceGroupType::UNKNOWN:
		break;
	}
	return "unknown";
}

SourceGroupType stringToSourceGroupType(const std::string& v)
{
#if BUILD_CXX_LANGUAGE_PACKAGE
	if (v == sourceGroupTypeToString(SourceGroupType::C_EMPTY))
	{
		return SourceGroupType::C_EMPTY;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CXX_EMPTY))
	{
		return SourceGroupType::CXX_EMPTY;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CXX_CDB))
	{
		return SourceGroupType::CXX_CDB;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CXX_CODEBLOCKS))
	{
		return SourceGroupType::CXX_CODEBLOCKS;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CXX_VS))
	{
		return SourceGroupType::CXX_VS;
	}
#endif	  // BUILD_CXX_LANGUAGE_PACKAGE
#if BUILD_JAVA_LANGUAGE_PACKAGE
	if (v == sourceGroupTypeToString(SourceGroupType::JAVA_EMPTY))
	{
		return SourceGroupType::JAVA_EMPTY;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::JAVA_MAVEN))
	{
		return SourceGroupType::JAVA_MAVEN;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::JAVA_GRADLE))
	{
		return SourceGroupType::JAVA_GRADLE;
	}
#endif	  // BUILD_JAVA_LANGUAGE_PACKAGE
#if BUILD_PYTHON_LANGUAGE_PACKAGE
	if (v == sourceGroupTypeToString(SourceGroupType::PYTHON_EMPTY) ||
		v == sourceGroupTypeToProjectSetupString(SourceGroupType::PYTHON_EMPTY))
	{
		return SourceGroupType::PYTHON_EMPTY;
	}
#endif	  // BUILD_PYTHON_LANGUAGE_PACKAGE
	if (v == sourceGroupTypeToString(SourceGroupType::CSHARP_EMPTY) || v == sourceGroupTypeToProjectSetupString(SourceGroupType::CSHARP_EMPTY))
	{
		return SourceGroupType::CSHARP_EMPTY;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::TREE_SITTER) || v == "tree-sitter" || v == "tree_sitter" || v == "Tree-sitter")
	{
		return SourceGroupType::TREE_SITTER;
	}
	if (v == sourceGroupTypeToString(SourceGroupType::CUSTOM_COMMAND))
	{
		return SourceGroupType::CUSTOM_COMMAND;
	}

	return SourceGroupType::UNKNOWN;
}
