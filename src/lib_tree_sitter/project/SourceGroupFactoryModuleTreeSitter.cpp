#include "SourceGroupFactoryModuleTreeSitter.h"

#include "SourceGroupTreeSitter.h"
#include "SourceGroupSettingsTreeSitter.h"

bool SourceGroupFactoryModuleTreeSitter::supports(SourceGroupType type) const
{
	switch (type)
	{
	case SOURCE_GROUP_TREE_SITTER:
		return true;
	default:
		break;
	}
	return false;
}

std::shared_ptr<SourceGroup> SourceGroupFactoryModuleTreeSitter::createSourceGroup(
	std::shared_ptr<SourceGroupSettings> settings) const
{
	std::shared_ptr<SourceGroup> sourceGroup;
	if (std::shared_ptr<SourceGroupSettingsTreeSitter> treeSitterSettings =
			std::dynamic_pointer_cast<SourceGroupSettingsTreeSitter>(settings))
	{
		sourceGroup = std::shared_ptr<SourceGroup>(new SourceGroupTreeSitter(treeSitterSettings));
	}
	return sourceGroup;
}
