#ifndef SOURCE_GROUP_SETTINGS_CSHARP_EMPTY_H
#define SOURCE_GROUP_SETTINGS_CSHARP_EMPTY_H

#include "SourceGroupSettingsWithComponents.h"
#include "SourceGroupSettingsWithExcludeFilters.h"
#include "SourceGroupSettingsWithSourceExtensionsCsharp.h"
#include "SourceGroupSettingsWithSourcePaths.h"

class SourceGroupSettingsCsharpEmpty
	: public SourceGroupSettingsWithComponents<
		  SourceGroupSettingsWithExcludeFilters,
		  SourceGroupSettingsWithSourceExtensionsCsharp,
		  SourceGroupSettingsWithSourcePaths>
{
public:
	SourceGroupSettingsCsharpEmpty(const std::string& id, const ProjectSettings* projectSettings)
		: SourceGroupSettingsWithComponents(SourceGroupType::CSHARP_EMPTY, id, projectSettings)
	{
	}

	std::shared_ptr<SourceGroupSettings> createCopy() const override
	{
		return std::make_shared<SourceGroupSettingsCsharpEmpty>(*this);
	}
};

#endif	  // SOURCE_GROUP_SETTINGS_CSHARP_EMPTY_H
