add_library(Sourcetrail_lib_tree_sitter
	project/SourceGroupFactoryModuleTreeSitter.cpp
	project/SourceGroupFactoryModuleTreeSitter.h
	project/SourceGroupTreeSitter.cpp
	project/SourceGroupTreeSitter.h
)
setDefaultTargetOptions(Sourcetrail_lib_tree_sitter)

target_include_directories(Sourcetrail_lib_tree_sitter
	PUBLIC
		project
		.
)

target_link_libraries(Sourcetrail_lib_tree_sitter
	PUBLIC
		Sourcetrail_lib
)
