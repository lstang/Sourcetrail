#ifndef SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_TREE_SITTER_H
#define SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_TREE_SITTER_H

#include "SourceGroupSettingsWithSourceExtensions.h"

class SourceGroupSettingsWithSourceExtensionsTreeSitter: public SourceGroupSettingsWithSourceExtensions
{
private:
	std::vector<std::wstring> getDefaultSourceExtensions() const override
	{
		return {
			L".rs", L".py", L".c", L".cpp", L".h", L".hpp",
			L".js", L".jsx", L".ts", L".tsx", L".go", L".cs", L".java"
		};
	}
};

#endif	  // SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_TREE_SITTER_H
