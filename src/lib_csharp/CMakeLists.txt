add_library(Sourcetrail_lib_csharp
	project/SourceGroupFactoryModuleCsharp.cpp
	project/SourceGroupFactoryModuleCsharp.h
	project/SourceGroupCsharpEmpty.cpp
	project/SourceGroupCsharpEmpty.h
)
setDefaultTargetOptions(Sourcetrail_lib_csharp)

target_include_directories(Sourcetrail_lib_csharp
	PUBLIC
		project
		.
)

target_link_libraries(Sourcetrail_lib_csharp
	PUBLIC
		Sourcetrail_lib
)
