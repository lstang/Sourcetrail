#ifndef SOURCE_GROUP_TREE_SITTER_H
#define SOURCE_GROUP_TREE_SITTER_H

#include <memory>
#include <vector>

#include "SourceGroup.h"

class SourceGroupSettingsTreeSitter;

class SourceGroupTreeSitter: public SourceGroup
{
public:
	SourceGroupTreeSitter(std::shared_ptr<SourceGroupSettingsTreeSitter> settings);

	bool allowsPartialClearing() const override;
	bool allowsShallowIndexing() const override;
	std::set<FilePath> filterToContainedFilePaths(const std::set<FilePath>& filePaths) const override;
	std::set<FilePath> getAllSourceFilePaths() const override;
	std::vector<std::shared_ptr<IndexerCommand>> getIndexerCommands(const RefreshInfo& info) const override;

private:
	std::shared_ptr<SourceGroupSettings> getSourceGroupSettings() override;
	std::shared_ptr<const SourceGroupSettings> getSourceGroupSettings() const override;

	std::shared_ptr<SourceGroupSettingsTreeSitter> m_settings;
};

#endif	  // SOURCE_GROUP_TREE_SITTER_H
