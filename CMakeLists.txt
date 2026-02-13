	# Minimum CMake version required, we'll just use the latest version.
	cmake_minimum_required(VERSION 3.22)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
	# Project name, version and description
	project(discord-bot VERSION 1.0 DESCRIPTION "A discord bot")
	 
	list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
	 
	# Create an executable
	add_executable(${PROJECT_NAME}
      src/sessions/session_manager.cpp
	    src/main.cpp
	    src/commands/pomodoro/pomodoro.cpp
      src/commands/registry.cpp
      src/utils.cpp
	)
	 
	# Find our pre-installed DPP package (using FindDPP.cmake).
	find_package(DPP REQUIRED)
  find_package(fmt CONFIG REQUIRED)

  # liboggz
  find_path(OGGZ_INCLUDE_DIR NAMES oggz/oggz.h)

  find_library(OGGZ_LIBRARIES
    NAMES oggz liboggz
  )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OGGZ DEFAULT_MSG
    OGGZ_LIBRARIES
    OGGZ_INCLUDE_DIR
)
	 
	# Link the pre-installed DPP package.
	target_link_libraries(${PROJECT_NAME} 
	    ${DPP_LIBRARIES}
      fmt::fmt
      ${OGGZ_LIBRARIES}
	)
	 
	# Include the DPP directories.
	target_include_directories(${PROJECT_NAME} PRIVATE
	    ${DPP_INCLUDE_DIR}
      ${OGGZ_INCLUDE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/src/sessions
      ${CMAKE_CURRENT_SOURCE_DIR}/src/commands
      ${CMAKE_CURRENT_SOURCE_DIR}/src/commands/pomodoro
	)
	 
	# Set C++ version
	set_target_properties(${PROJECT_NAME} PROPERTIES
	    CXX_STANDARD 23
	    CXX_STANDARD_REQUIRED ON
	)

  find_path(DPP_INCLUDE_DIR NAMES dpp/dpp.h HINTS ${DPP_ROOT_DIR})
	 
	find_library(DPP_LIBRARIES NAMES dpp "libdpp.a" HINTS ${DPP_ROOT_DIR})
	 
	include(FindPackageHandleStandardArgs)
	 
	find_package_handle_standard_args(DPP DEFAULT_MSG DPP_LIBRARIES DPP_INCLUDE_DIR)


