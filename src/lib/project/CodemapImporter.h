#ifndef CODEMAP_IMPORTER_H
#define CODEMAP_IMPORTER_H

#include <string>
#include "FilePath.h"

class CodemapImporter
{
public:
	static bool isCodemapFile(const FilePath& path);
	static FilePath importCodemap(const FilePath& path, std::string* errorMsg = nullptr);

private:
	static FilePath resolveTargetFile(const FilePath& path);
	static FilePath findImporterScript();
};

#endif	// CODEMAP_IMPORTER_H
