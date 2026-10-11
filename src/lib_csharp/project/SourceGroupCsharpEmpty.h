#ifndef SOURCE_GROUP_CSHARP_EMPTY_H
#define SOURCE_GROUP_CSHARP_EMPTY_H

#include <memory>
#include <vector>

#include "SourceGroup.h"

class SourceGroupSettingsCsharpEmpty;

class SourceGroupCsharpEmpty: public SourceGroup
{
public:
	SourceGroupCsharpEmpty(std::shared_ptr<SourceGroupSettingsCsharpEmpty> settings);

	bool allowsPartialClearing() const override;
	std::set<FilePath> filterToContainedFilePaths(const std::set<FilePath>& filePaths) const override;
	std::set<FilePath> getAllSourceFilePaths() const override;
	std::vector<std::shared_ptr<IndexerCommand>> getIndexerCommands(const RefreshInfo& info) const override;

private:
	std::shared_ptr<SourceGroupSettings> getSourceGroupSettings() override;
	std::shared_ptr<const SourceGroupSettings> getSourceGroupSettings() const override;

	std::shared_ptr<SourceGroupSettingsCsharpEmpty> m_settings;
};

#endif	  // SOURCE_GROUP_CSHARP_EMPTY_H
