project(scene-notes-dock VERSION 0.0.2)
set(PROJECT_FULL_NAME "Scene Notes Dock")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

set(CMAKE_AUTOMOC ON)

set(scene-notes-dock_HEADERS
	scene-notes-dock.hpp
	version.h)
set(scene-notes-dock_SOURCES
	scene-notes-dock.cpp)

if(WIN32)
	get_filename_component(ISS_FILES_DIR "${CMAKE_BINARY_DIR}\\..\\package" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_FILES_DIR}" ISS_FILES_DIR)
	get_filename_component(ISS_PACKAGE_DIR "${CMAKE_PACKAGE_PREFIX}\\.." ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_PACKAGE_DIR}" ISS_PACKAGE_DIR)
	get_filename_component(ISS_SOURCE_DIR "${PROJECT_SOURCE_DIR}" ABSOLUTE)
	file(TO_NATIVE_PATH "${ISS_SOURCE_DIR}" ISS_SOURCE_DIR)
	configure_file("installer.iss.in"
			"${PROJECT_BINARY_DIR}/installer.iss"
		)
	configure_file(resource.rc.in scene-notes-dock.rc)
	list(APPEND scene-notes-dock_SOURCES
		scene-notes-dock.rc)
endif()

if(BUILD_OUT_OF_TREE)
    find_package(LibObs REQUIRED)
endif()

add_library(scene-notes-dock MODULE
	${scene-notes-dock_HEADERS}
	${scene-notes-dock_SOURCES})

target_link_libraries(scene-notes-dock
		obs-frontend-api
		Qt5::Widgets
		libobs)

if(BUILD_OUT_OF_TREE)
    if(NOT LIB_OUT_DIR)
        set(LIB_OUT_DIR "/lib/obs-plugins")
    endif()
    if(NOT DATA_OUT_DIR)
        set(DATA_OUT_DIR "/share/obs/obs-plugins/scene-notes-dock")
    endif()
    set_target_properties(scene-notes-dock PROPERTIES PREFIX "")
    install(TARGETS scene-notes-dock
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${LIB_OUT_DIR})
    install(DIRECTORY data/locale
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${DATA_OUT_DIR})
else()
    target_include_directories(scene-notes-dock PRIVATE
        "${CMAKE_SOURCE_DIR}/UI/obs-frontend-api")
    set_target_properties(scene-notes-dock PROPERTIES FOLDER "plugins/exeldro")
    install_obs_plugin_with_data(scene-notes-dock data)
endif()
