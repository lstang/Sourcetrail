add_library(Sourcetrail_lib_python
	project/SourceGroupFactoryModulePython.cpp
	project/SourceGroupFactoryModulePython.h
	project/SourceGroupPythonEmpty.cpp
	project/SourceGroupPythonEmpty.h
)
setDefaultTargetOptions(Sourcetrail_lib_python)

target_include_directories(Sourcetrail_lib_python
	PUBLIC
		project
		.
)

target_link_libraries(Sourcetrail_lib_python
	PUBLIC
		Sourcetrail_lib
)
