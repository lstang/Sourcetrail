add_executable(Sourcetrail WIN32
	main.cpp
	Sourcetrail.rc
)
setDefaultTargetOptions(Sourcetrail)

setAppOutputDirectory(Sourcetrail)

target_link_libraries(Sourcetrail
	PRIVATE
		Sourcetrail_lib
)

if (WIN32)
	find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin" "F:/Qt/6.11.1/msvc2022_64/bin")
	if (WINDEPLOYQT_EXECUTABLE)
		add_custom_command(
			TARGET Sourcetrail
			POST_BUILD
			COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-compiler-runtime --no-opengl-sw "$<TARGET_FILE:Sourcetrail>"
			COMMENT "Deploying Qt binaries with windeployqt"
		)
	endif()
endif()
