cmake_minimum_required(VERSION 3.18)

project(scene-notes-dock VERSION 0.0.1)
set(PROJECT_FULL_NAME "Scene Notes Dock")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version.h)

# Fix undefined symbol
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

add_library(scene-notes-dock MODULE
	${scene-notes-dock_HEADERS}
	${scene-notes-dock_SOURCES})

target_link_libraries(scene-notes-dock
		obs-frontend-api
		Qt5::Widgets
		libobs)

set_target_properties(scene-notes-dock PROPERTIES FOLDER "plugins/exeldro")
install_obs_plugin_with_data(scene-notes-dock data)
