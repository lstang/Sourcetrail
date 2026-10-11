#ifndef CXX_SPECIFIER_NAME_RESOLVER_H
#define CXX_SPECIFIER_NAME_RESOLVER_H

#include <clang/Basic/Version.h>
#include <clang/AST/NestedNameSpecifier.h>

#include "CxxNameResolver.h"

class CxxName;

class CxxSpecifierNameResolver: public CxxNameResolver
{
public:
	CxxSpecifierNameResolver(CanonicalFilePathCache* canonicalFilePathCache);
	CxxSpecifierNameResolver(const CxxNameResolver* other);

#if CLANG_VERSION_MAJOR >= 22
	std::unique_ptr<CxxName> getName(clang::NestedNameSpecifier nestedNameSpecifier);
#else
	std::unique_ptr<CxxName> getName(const clang::NestedNameSpecifier* nestedNameSpecifier);
#endif
};

#endif	  // CXX_SPECIFIER_NAME_RESOLVER_H
