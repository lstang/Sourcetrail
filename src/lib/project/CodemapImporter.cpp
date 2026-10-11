#include "CodemapImporter.h"

#include <chrono>
#include <vector>

#include "AppPath.h"
#include "FileSystem.h"
#include "TimeStamp.h"
#include "logging.h"
#include "utilityApp.h"
#include "utilityString.h"

bool CodemapImporter::isCodemapFile(const FilePath& path)
{
	if (path.empty())
	{
		return false;
	}

	const std::string ext = utility::toLowerCase(path.extension());
	const std::string filename = utility::toLowerCase(path.fileName());

	if (ext == ".db" || ext == ".json")
	{
		return true;
	}

	if (filename == "codegraph.db" || filename == "graph.json")
	{
		return true;
	}

	if (path.isDirectory())
	{
		const FilePath cgDb = path.getConcatenated(FilePath("/.codegraph/codegraph.db"));
		const FilePath cgDbDirect = path.getConcatenated(FilePath("/codegraph.db"));
		const FilePath gfJson = path.getConcatenated(FilePath("/graphify-out/graph.json"));
		const FilePath gfJsonDirect = path.getConcatenated(FilePath("/graph.json"));

		if (cgDb.exists() || cgDbDirect.exists() || gfJson.exists() || gfJsonDirect.exists())
		{
			return true;
		}
	}

	return false;
}

FilePath CodemapImporter::resolveTargetFile(const FilePath& path)
{
	if (path.isDirectory())
	{
		const FilePath cgDb = path.getConcatenated(FilePath("/.codegraph/codegraph.db"));
		if (cgDb.exists())
		{
			return cgDb;
		}

		const FilePath cgDbDirect = path.getConcatenated(FilePath("/codegraph.db"));
		if (cgDbDirect.exists())
		{
			return cgDbDirect;
		}

		const FilePath gfJson = path.getConcatenated(FilePath("/graphify-out/graph.json"));
		if (gfJson.exists())
		{
			return gfJson;
		}

		const FilePath gfJsonDirect = path.getConcatenated(FilePath("/graph.json"));
		if (gfJsonDirect.exists())
		{
			return gfJsonDirect;
		}
	}

	return path;
}

FilePath CodemapImporter::findImporterScript()
{
	const FilePath sharedDataDir = AppPath::getSharedDataDirectoryPath();
	const std::vector<FilePath> candidates = {
		sharedDataDir.getParentDirectory().getConcatenated(FilePath("/script/import_codemap.py")),
		sharedDataDir.getConcatenated(FilePath("/script/import_codemap.py")),
		sharedDataDir.getParentDirectory().getConcatenated(FilePath("/../../script/import_codemap.py")),
		FilePath("F:/dev/Sourcetrail/script/import_codemap.py")
	};

	for (const auto& candidate : candidates)
	{
		if (candidate.exists())
		{
			return candidate;
		}
	}

	return FilePath();
}

FilePath CodemapImporter::importCodemap(const FilePath& path, std::string* errorMsg)
{
	if (path.empty())
	{
		if (errorMsg)
		{
			*errorMsg = "Empty path provided.";
		}
		return FilePath();
	}

	const std::string ext = utility::toLowerCase(path.extension());
	if (ext == ".srctrlprj")
	{
		return path;
	}

	const FilePath targetFile = resolveTargetFile(path);
	if (!targetFile.exists())
	{
		const std::string err = "Target codemap file does not exist: " + targetFile.str();
		if (errorMsg)
		{
			*errorMsg = err;
		}
		LOG_ERROR(err);
		return FilePath();
	}

	const FilePath targetDir = targetFile.getParentDirectory();
	const std::string baseName = targetFile.withoutExtension().fileName();

	const FilePath companionPrj = targetDir.getConcatenated(FilePath("/" + baseName + ".srctrlprj"));
	const FilePath companionDb = targetDir.getConcatenated(FilePath("/" + baseName + ".srctrldb"));

	// Cache check: if companion files exist and DB is at least as new as target file
	if (companionPrj.exists() && companionDb.exists())
	{
		TimeStamp targetTime = FileSystem::getLastWriteTime(targetFile);
		TimeStamp dbTime = FileSystem::getLastWriteTime(companionDb);

		if (dbTime >= targetTime)
		{
			LOG_INFO("Using up-to-date cached project for codemap: " + companionPrj.str());
			return companionPrj;
		}
	}

	const FilePath scriptPath = findImporterScript();
	if (scriptPath.empty())
	{
		const std::string err = "import_codemap.py script not found.";
		if (errorMsg)
		{
			*errorMsg = err;
		}
		LOG_ERROR(err);
		return FilePath();
	}

	LOG_INFO("Running codemap importer for: " + targetFile.str());

	const std::vector<std::string> pyCommands = {
		"python", "py", "python3", "C:/ProgramData/miniconda3/envs/py314/python.exe"
	};

	utility::ProcessOutput out;
	bool success = false;

	for (const auto& pyCmd : pyCommands)
	{
		const std::vector<std::string> args = { scriptPath.str(), targetFile.str() };
		out = utility::executeProcess(pyCmd, args, targetDir, false, std::chrono::minutes(5), true);
		if (out.exitCode == 0 && companionPrj.exists())
		{
			success = true;
			break;
		}
	}

	if (!success || !companionPrj.exists())
	{
		const std::string err = "Failed to import codemap: " + out.error;
		if (errorMsg)
		{
			*errorMsg = err;
		}
		LOG_ERROR(err);
		return FilePath();
	}

	LOG_INFO("Successfully imported codemap: " + companionPrj.str());
	return companionPrj;
}
