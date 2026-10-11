#include "SourceGroupTreeSitter.h"

#include "ApplicationSettings.h"
#include "FileManager.h"
#include "IndexerCommandCustom.h"
#include "ProjectSettings.h"
#include "RefreshInfo.h"
#include "ResourcePaths.h"
#include "SourceGroupSettingsTreeSitter.h"
#include "SqliteIndexStorage.h"
#include "utility.h"

SourceGroupTreeSitter::SourceGroupTreeSitter(std::shared_ptr<SourceGroupSettingsTreeSitter> settings)
	: m_settings(settings)
{
}

bool SourceGroupTreeSitter::allowsPartialClearing() const
{
	return false;
}

std::set<FilePath> SourceGroupTreeSitter::filterToContainedFilePaths(
	const std::set<FilePath>& filePaths) const
{
	return SourceGroup::filterToContainedFilePaths(
		filePaths,
		std::set<FilePath>(),
		utility::toSet(m_settings->getSourcePathsExpandedAndAbsolute()),
		m_settings->getExcludeFiltersExpandedAndAbsolute());
}

std::set<FilePath> SourceGroupTreeSitter::getAllSourceFilePaths() const
{
	FileManager fileManager;
	fileManager.update(
		m_settings->getSourcePathsExpandedAndAbsolute(),
		m_settings->getExcludeFiltersExpandedAndAbsolute(),
		m_settings->getSourceExtensions());
	return fileManager.getAllSourceFilePaths();
}

std::vector<std::shared_ptr<IndexerCommand>> SourceGroupTreeSitter::getIndexerCommands(
	const RefreshInfo& info) const
{
	std::vector<std::string> args;
	args.push_back("index");

	args.push_back("--source-file-path");
	args.push_back("%{SOURCE_FILE_PATH}");
	args.push_back("--database-file-path");
	args.push_back("%{DATABASE_FILE_PATH}");

	if (ApplicationSettings::getInstance()->getVerboseIndexerLoggingEnabled())
	{
		args.push_back("--verbose");
	}


	std::vector<std::shared_ptr<IndexerCommand>> indexerCommands;
	for (const FilePath& sourceFilePath: getAllSourceFilePaths())
	{
		if (info.filesToIndex.find(sourceFilePath) != info.filesToIndex.end())
		{
			indexerCommands.push_back(std::make_shared<IndexerCommandCustom>(
				INDEXER_COMMAND_TREE_SITTER,
				ResourcePaths::getTreeSitterIndexerFilePath().str(),
				args,
				m_settings->getProjectSettings()->getProjectFilePath(),
				m_settings->getProjectSettings()->getTempDBFilePath(),
				std::to_string(SqliteIndexStorage::getStorageVersion()),
				sourceFilePath,
				true));
		}
	}

	return indexerCommands;
}

std::shared_ptr<SourceGroupSettings> SourceGroupTreeSitter::getSourceGroupSettings()
{
	return m_settings;
}

std::shared_ptr<const SourceGroupSettings> SourceGroupTreeSitter::getSourceGroupSettings() const
{
	return m_settings;
}
