#include "catch.hpp"

#include <fstream>
#include <memory>

#include "FileSystem.h"
#include "Graph.h"
#include "IndexerCommandCustom.h"
#include "Node.h"
#include "PersistentStorage.h"
#include "ResourcePaths.h"
#include "SqliteIndexStorage.h"
#include "TestStorage.h"
#include "utilityApp.h"

namespace
{
void deleteAllContents(const FilePath& tempPath)
{
	if (tempPath.recheckExists())
	{
		for (const FilePath& path: FileSystem::getFilePathsFromDirectory(tempPath))
		{
			FileSystem::remove(path);
		}
	}
}

std::pair<std::shared_ptr<PersistentStorage>, std::shared_ptr<TestStorage>> parseCsharpCode(
	std::string code)
{
	const FilePath rootPath = FilePath(L"data/CsharpIndexerTestSuite/temp/").makeAbsolute();

	if (!rootPath.exists())
	{
		FileSystem::createDirectory(rootPath);
	}

	deleteAllContents(rootPath);

	const FilePath sourceFilePath = rootPath.getConcatenated(L"Test.cs");
	const FilePath tempDbPath = rootPath.getConcatenated(L"temp.srctrldb");

	{
		std::ofstream codeFile;
		codeFile.open(sourceFilePath.str());
		codeFile << code;
		codeFile.close();
	}

	{
		std::vector<std::wstring> args;
		args.push_back(L"index");
		args.push_back(L"--source-file-path");
		args.push_back(L"%{SOURCE_FILE_PATH}");
		args.push_back(L"--database-file-path");
		args.push_back(L"%{DATABASE_FILE_PATH}");
		args.push_back(L"--shallow");

		std::shared_ptr<IndexerCommandCustom> indexerCommand = std::make_shared<IndexerCommandCustom>(
			INDEXER_COMMAND_CSHARP,
			FilePath("../app")
				.getConcatenated(ResourcePaths::getCsharpIndexerFilePath())
				.makeAbsolute()
				.makeCanonical()
				.wstr(),
			args,
			rootPath,
			tempDbPath,
			std::to_wstring(SqliteIndexStorage::getStorageVersion()),
			sourceFilePath,
			true);

		const utility::ProcessOutput out = utility::executeProcess(
			indexerCommand->getCommand(), indexerCommand->getArguments(), rootPath, false, -1, true);

		if (!out.error.empty())
		{
			FAIL(
				"Error occurred while running C# indexer: \"" + utility::encodeToUtf8(out.error) +
				"\". Process output was: \"" + utility::encodeToUtf8(out.output) + "\"");
		}
		REQUIRE(out.exitCode == 0);
	}

	std::shared_ptr<PersistentStorage> persistentStorage = std::make_shared<PersistentStorage>(
		tempDbPath, FilePath());
	persistentStorage->setup();
	persistentStorage->buildCaches();

	std::shared_ptr<TestStorage> testStorage = TestStorage::create(persistentStorage);
	return {persistentStorage, testStorage};
}
} // namespace

TEST_CASE("csharp indexer creates classes, methods, calls, and graph elements")
{
	auto res = parseCsharpCode(
		"namespace SampleNamespace\n"
		"{\n"
		"    public interface IWorker\n"
		"    {\n"
		"        void DoWork();\n"
		"    }\n"
		"    public class Worker : IWorker\n"
		"    {\n"
		"        public void DoWork()\n"
		"        {\n"
		"            Log();\n"
		"        }\n"
		"        private void Log()\n"
		"        {\n"
		"        }\n"
		"    }\n"
		"}\n");

	auto storage = res.first;
	auto testStorage = res.second;

	// Check symbols extracted
	REQUIRE(testStorage->classes.size() >= 1);
	REQUIRE(testStorage->interfaces.size() >= 1);
	REQUIRE(testStorage->methods.size() >= 2);
	REQUIRE(testStorage->inheritances.size() >= 1);
	REQUIRE(testStorage->calls.size() >= 1);

	// Check graph contains the symbols when file contents are queried
	const FilePath sourceFilePath = FilePath(L"data/CsharpIndexerTestSuite/temp/Test.cs").makeAbsolute();
	Id fileId = storage->getNodeIdForFileNode(sourceFilePath);
	REQUIRE(fileId > 0);

	std::shared_ptr<Graph> graph = storage->getGraphForActiveTokenIds({fileId}, {}, nullptr);
	REQUIRE(graph != nullptr);

	// The code graph must contain more than just the file node!
	// It must contain classes, methods, interface, etc.
	REQUIRE(graph->getNodeCount() > 1);

	Node* fileNode = graph->getNodeById(fileId);
	REQUIRE(fileNode != nullptr);
	REQUIRE(fileNode->getChildCount() > 0);
}

