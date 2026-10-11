#include "CxxSpecifierNameResolver.h"

#include "CxxDeclNameResolver.h"
#include "CxxTypeNameResolver.h"

#include <clang/Basic/Version.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/PrettyPrinter.h>

CxxSpecifierNameResolver::CxxSpecifierNameResolver(CanonicalFilePathCache* canonicalFilePathCache)
	: CxxNameResolver(canonicalFilePathCache)
{
}

CxxSpecifierNameResolver::CxxSpecifierNameResolver(const CxxNameResolver* other)
	: CxxNameResolver(other)
{
}
#if CLANG_VERSION_MAJOR >= 22
std::unique_ptr<CxxName> CxxSpecifierNameResolver::getName(
	clang::NestedNameSpecifier nestedNameSpecifier)
{
	if (nestedNameSpecifier)
	{
		switch (nestedNameSpecifier.getKind())
		{
		case clang::NestedNameSpecifier::Kind::Namespace:
		{
			auto nsAndPrefix = nestedNameSpecifier.getAsNamespaceAndPrefix();
			std::unique_ptr<CxxName> name = CxxDeclNameResolver(this).getName(nsAndPrefix.Namespace);
			if (nsAndPrefix.Prefix)
			{
				std::unique_ptr<CxxName> parentName = getName(nsAndPrefix.Prefix);
				if (parentName && name)
				{
					name->setParent(std::move(parentName));
				}
			}
			return name;
		}
		case clang::NestedNameSpecifier::Kind::Type:
			return CxxTypeName::makeUnsolvedIfNull(
				CxxTypeNameResolver(this).getName(nestedNameSpecifier.getAsType()));
		case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
			return CxxDeclNameResolver(this).getName(nestedNameSpecifier.getAsRecordDecl());
		case clang::NestedNameSpecifier::Kind::Global:
		case clang::NestedNameSpecifier::Kind::Null:
			break;
		}
	}
	return nullptr;
}
#else
std::unique_ptr<CxxName> CxxSpecifierNameResolver::getName(
	const clang::NestedNameSpecifier* nestedNameSpecifier)
{
	if (nestedNameSpecifier)
	{
		clang::NestedNameSpecifier::SpecifierKind nnsKind = nestedNameSpecifier->getKind();
		switch (nnsKind)
		{
		case clang::NestedNameSpecifier::Identifier:
		{
			std::unique_ptr<CxxName> name = std::make_unique<CxxDeclName>(
				nestedNameSpecifier->getAsIdentifier()->getName().str());

			if (const clang::NestedNameSpecifier* prefix = nestedNameSpecifier->getPrefix())
			{
				std::unique_ptr<CxxName> parentName = getName(prefix);
				if (parentName)
				{
					name->setParent(std::move(parentName));
				}
			}

			return name;
		}

		case clang::NestedNameSpecifier::Namespace:
			return CxxDeclNameResolver(this).getName(nestedNameSpecifier->getAsNamespace());

		case clang::NestedNameSpecifier::NamespaceAlias:
			return CxxDeclNameResolver(this).getName(nestedNameSpecifier->getAsNamespaceAlias());

		case clang::NestedNameSpecifier::TypeSpec:
			return CxxTypeName::makeUnsolvedIfNull(
				CxxTypeNameResolver(this).getName(nestedNameSpecifier->getAsType()));

		case clang::NestedNameSpecifier::Global:
			// no context name hierarchy needed.
			break;

		case clang::NestedNameSpecifier::Super:
			return CxxDeclNameResolver(this).getName(nestedNameSpecifier->getAsRecordDecl());
		}
	}
	return nullptr;
}
#endif
