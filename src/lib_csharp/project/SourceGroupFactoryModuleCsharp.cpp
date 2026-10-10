#include "SourceGroupFactoryModuleCsharp.h"

#include "SourceGroupCsharpEmpty.h"
#include "SourceGroupSettingsCsharpEmpty.h"

bool SourceGroupFactoryModuleCsharp::supports(SourceGroupType type) const
{
	switch (type)
	{
	case SOURCE_GROUP_CSHARP_EMPTY:
		return true;
	default:
		break;
	}
	return false;
}

std::shared_ptr<SourceGroup> SourceGroupFactoryModuleCsharp::createSourceGroup(
	std::shared_ptr<SourceGroupSettings> settings) const
{
	std::shared_ptr<SourceGroup> sourceGroup;
	if (std::shared_ptr<SourceGroupSettingsCsharpEmpty> csharpSettings =
			std::dynamic_pointer_cast<SourceGroupSettingsCsharpEmpty>(settings))
	{
		sourceGroup = std::shared_ptr<SourceGroup>(new SourceGroupCsharpEmpty(csharpSettings));
	}
	return sourceGroup;
}
