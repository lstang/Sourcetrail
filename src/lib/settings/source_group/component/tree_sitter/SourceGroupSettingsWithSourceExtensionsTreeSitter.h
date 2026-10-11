#ifndef SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_TREE_SITTER_H
#define SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_TREE_SITTER_H

#include "SourceGroupSettingsWithSourceExtensions.h"

class SourceGroupSettingsWithSourceExtensionsTreeSitter: public SourceGroupSettingsWithSourceExtensions
{
private:
	std::vector<std::string> getDefaultSourceExtensions() const override
	{
		return {
			".rs", ".py", ".c", ".cpp", ".h", ".hpp",
			".js", ".jsx", ".ts", ".tsx", ".go", ".cs", ".java"
		};
	}
};

#endif	  // SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_TREE_SITTER_H
