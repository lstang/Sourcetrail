#ifndef SOURCE_GROUP_SETTINGS_TREE_SITTER_H
#define SOURCE_GROUP_SETTINGS_TREE_SITTER_H

#include "SourceGroupSettingsWithComponents.h"
#include "SourceGroupSettingsWithExcludeFilters.h"
#include "SourceGroupSettingsWithSourceExtensionsTreeSitter.h"
#include "SourceGroupSettingsWithSourcePaths.h"

class SourceGroupSettingsTreeSitter
	: public SourceGroupSettingsWithComponents<
		  SourceGroupSettingsWithExcludeFilters,
		  SourceGroupSettingsWithSourceExtensionsTreeSitter,
		  SourceGroupSettingsWithSourcePaths>
{
public:
	SourceGroupSettingsTreeSitter(const std::string& id, const ProjectSettings* projectSettings)
		: SourceGroupSettingsWithComponents(SOURCE_GROUP_TREE_SITTER, id, projectSettings)
	{
	}

	std::shared_ptr<SourceGroupSettings> createCopy() const override
	{
		return std::make_shared<SourceGroupSettingsTreeSitter>(*this);
	}
};

#endif	  // SOURCE_GROUP_SETTINGS_TREE_SITTER_H
