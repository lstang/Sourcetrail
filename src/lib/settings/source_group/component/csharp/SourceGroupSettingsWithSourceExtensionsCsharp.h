#ifndef SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_CSHARP_H
#define SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_CSHARP_H

#include "SourceGroupSettingsWithSourceExtensions.h"

class SourceGroupSettingsWithSourceExtensionsCsharp: public SourceGroupSettingsWithSourceExtensions
{
private:
	std::vector<std::string> getDefaultSourceExtensions() const override
	{
		return {".cs"};
	}
};

#endif	  // SOURCE_GROUP_SETTINGS_WITH_SOURCE_EXTENSIONS_CSHARP_H
