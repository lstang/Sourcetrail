#include "ResourcePaths.h"
#include "AppPath.h"
#include "Platform.h"

FilePath ResourcePaths::getColorSchemesDirectoryPath()
{
	return AppPath::getSharedDataDirectoryPath().concatenate("data/color_schemes/");
}

FilePath ResourcePaths::getSyntaxHighlightingRulesDirectoryPath()
{
	return AppPath::getSharedDataDirectoryPath().concatenate("data/syntax_highlighting_rules/");
}

FilePath ResourcePaths::getFallbackDirectoryPath()
{
	return AppPath::getSharedDataDirectoryPath().concatenate("data/fallback/");
}

FilePath ResourcePaths::getFontsDirectoryPath()
{
	return AppPath::getSharedDataDirectoryPath().concatenate("data/fonts/");
}

FilePath ResourcePaths::getJavaDirectoryPath()
{
	return AppPath::getSharedDataDirectoryPath().concatenate("data/java/");
}

FilePath ResourcePaths::getCxxCompilerHeaderDirectoryPath()
{
	return AppPath::getSharedDataDirectoryPath().concatenate("data/cxx/include/").getCanonical();
}

FilePath ResourcePaths::getCsharpDirectoryPath()
{
	return AppPath::getSharedDataDirectoryPath().concatenate("data/csharp/");
}

FilePath ResourcePaths::getCsharpIndexerFilePath()
{
	if constexpr (utility::Platform::isWindows())
	{
		return getCsharpDirectoryPath().concatenate("SourcetrailCsharpIndexer.exe");
	}
	return getCsharpDirectoryPath().concatenate("SourcetrailCsharpIndexer");
}

FilePath ResourcePaths::getTreeSitterDirectoryPath()
{
	return AppPath::getSharedDataDirectoryPath().concatenate("data/tree_sitter/");
}

FilePath ResourcePaths::getTreeSitterIndexerFilePath()
{
	if constexpr (utility::Platform::isWindows())
	{
		return getTreeSitterDirectoryPath().concatenate("SourcetrailTreeSitterIndexer.exe");
	}
	return getTreeSitterDirectoryPath().concatenate("SourcetrailTreeSitterIndexer");
}
