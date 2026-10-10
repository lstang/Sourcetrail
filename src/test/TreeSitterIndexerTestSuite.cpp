#include "catch.hpp"

#include <fstream>
#include <memory>

#include "CodeController.h"
#include "CodeView.h"
#include "Component.h"
#include "FileSystem.h"
#include "Graph.h"
#include "IndexerCommandCustom.h"
#include "MessageActivateOverview.h"
#include "MessageActivateTokens.h"
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

std::pair<std::shared_ptr<PersistentStorage>, std::shared_ptr<TestStorage>> parseTreeSitterCode(
	std::string code, std::wstring fileName)
{
	const FilePath rootPath = FilePath(L"data/TreeSitterIndexerTestSuite/temp/").makeAbsolute();

	if (!rootPath.exists())
	{
		FileSystem::createDirectory(rootPath);
	}

	deleteAllContents(rootPath);

	const FilePath sourceFilePath = rootPath.getConcatenated(fileName);
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
			INDEXER_COMMAND_TREE_SITTER,
			FilePath("../app")
				.getConcatenated(ResourcePaths::getTreeSitterIndexerFilePath())
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
				"Error occurred while running Tree-sitter indexer: \"" + utility::encodeToUtf8(out.error) +
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

class MockTreeSitterCodeView : public CodeView
{
public:
	MockTreeSitterCodeView() : CodeView(nullptr), m_inListMode(true), shownSnippetCount(0) {}
	void clear() override {}
	void showSnippets(const std::vector<CodeFileParams>& files, const CodeParams& params, const CodeScrollParams& scrollParams) override
	{
		for (const auto& f: files)
		{
			shownSnippetCount += f.snippetParams.size();
			for (const auto& s: f.snippetParams)
			{
				lastCode += s.code;
			}
		}
	}
	void showSingleFile(const CodeFileParams& file, const CodeParams& params, const CodeScrollParams& scrollParams) override {}
	void updateSourceLocations(const std::vector<CodeFileParams>& files) override {}
	void scrollTo(const CodeScrollParams& params, bool animated) override {}
	bool showsErrors() const override { return false; }
	void coFocusTokenIds(const std::vector<Id>& coFocusedTokenIds) override {}
	void deCoFocusTokenIds() override {}
	bool isInListMode() const override { return m_inListMode; }
	void setMode(bool listMode) override { m_inListMode = listMode; }
	bool hasSingleFileCached(const FilePath& filePath) const override { return false; }
	void createWidgetWrapper() override {}
	void refreshView() override {}
	void setNavigationFocus(bool) override {}
	bool hasNavigationFocus() const override { return false; }
	bool isVisible() const override { return true; }
	void findMatches(ScreenSearchSender* sender, const std::wstring& query) override {}
	void activateMatch(size_t matchIndex) override {}
	void deactivateMatch(size_t matchIndex) override {}
	void clearMatches() override {}

	bool m_inListMode;
	size_t shownSnippetCount;
	std::string lastCode;
};
} // namespace

TEST_CASE("tree-sitter indexer indexes Rust code, generates graph, and shows code snippet")
{
	auto res = parseTreeSitterCode(
		"pub struct Calculator {\n"
		"    value: i32,\n"
		"}\n"
		"\n"
		"impl Calculator {\n"
		"    pub fn add(&mut self, n: i32) {\n"
		"        self.log();\n"
		"        self.value += n;\n"
		"    }\n"
		"    fn log(&self) {}\n"
		"}\n",
		L"calculator.rs");

	auto storage = res.first;
	auto testStorage = res.second;

	// Check symbols extracted
	REQUIRE((testStorage->structs.size() >= 1 || testStorage->classes.size() >= 1));
	REQUIRE(testStorage->methods.size() >= 2);
	REQUIRE(testStorage->calls.size() >= 1);

	// Check graph contains the symbols when file contents are queried
	const FilePath sourceFilePath = FilePath(L"data/TreeSitterIndexerTestSuite/temp/calculator.rs").makeAbsolute();
	Id fileId = storage->getNodeIdForFileNode(sourceFilePath);
	REQUIRE(fileId > 0);

	std::shared_ptr<Graph> graph = storage->getGraphForActiveTokenIds({fileId}, {}, nullptr);
	REQUIRE(graph != nullptr);
	REQUIRE(graph->getNodeCount() > 1);

	Node* fileNode = graph->getNodeById(fileId);
	REQUIRE(fileNode != nullptr);
	REQUIRE(fileNode->getChildCount() > 0);

	// Check CodeController shows code snippet when an element is activated
	auto view = std::make_shared<MockTreeSitterCodeView>();
	auto controller = std::make_shared<CodeController>(storage.get());
	Component component(view, controller);

	Id targetNodeId = 0;
	for (const auto& node: storage->getStorageNodes())
	{
		if (node.type == nodeKindToInt(NODE_STRUCT) || node.type == nodeKindToInt(NODE_CLASS))
		{
			targetNodeId = node.id;
			break;
		}
	}
	REQUIRE(targetNodeId > 0);

	MessageActivateOverview dummy;
	MessageActivateTokens msgClass(&dummy);
	msgClass.tokenIds = {targetNodeId};
	static_cast<MessageListener<MessageActivateTokens>*>(controller.get())->handleMessageBase(&msgClass);

	REQUIRE(view->shownSnippetCount > 0);
	REQUIRE(!view->lastCode.empty());
	REQUIRE(view->lastCode.find("Calculator") != std::string::npos);
}

TEST_CASE("tree-sitter indexer indexes JavaScript code")
{
	auto res = parseTreeSitterCode(
		"function helper() {}\n"
		"class Service {\n"
		"    run() {\n"
		"        helper();\n"
		"    }\n"
		"}\n",
		L"service.js");

	auto testStorage = res.second;
	REQUIRE(testStorage->classes.size() >= 1);
	REQUIRE(testStorage->functions.size() >= 1);
	REQUIRE(testStorage->methods.size() >= 1);
	REQUIRE(testStorage->calls.size() >= 1);
}
