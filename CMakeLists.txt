cmake_minimum_required( VERSION 3.31 )
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_SCAN_FOR_MODULES OFF )
project( drunken_lobster_driving )

option( CC_HAS_DEVUI "Enable Dev Ui" ON )
option( CC_HAS_EMBED_CONTENT "Embed content in executable" OFF )
option( CC_HAS_EXECUTABLE_AS_LIBRARY "Build a shared library instead of an executable" OFF )
set( CC_BUILD_VERSION "local" CACHE STRING "Build Version" )
set( CC_BUILD_PLATFORM "local" CACHE STRING "Build Platform" )
set( CC_BUILD_CONFIG "local" CACHE STRING "Build Config" )
set( CC_BUILD_NUMBER "local" CACHE STRING "Build number" )
set( CC_BUILD_COMMIT "local" CACHE STRING "Build commit" )

set( BUILD_SHARED_LIBS OFF )
set( SDL_STATIC ON )
set( SDLIMAGE_AVIF OFF )
set( SDLIMAGE_BMP OFF )
set( SDLIMAGE_GIF OFF )
set( SDLIMAGE_JPG ON )
set( SDLIMAGE_JXL OFF )
set( SDLIMAGE_LBM OFF )
set( SDLIMAGE_PCX OFF )
set( SDLIMAGE_PNG ON )
set( SDLIMAGE_PNM OFF )
set( SDLIMAGE_QOI OFF )
set( SDLIMAGE_SVG OFF )
set( SDLIMAGE_TGA ON )
set( SDLIMAGE_TIF OFF )
set( SDLIMAGE_WEBP OFF )
set( SDLIMAGE_XCF OFF )
set( SDLIMAGE_XPM OFF )
set( SDLIMAGE_XV OFF )
set( RMLUI_BACKEND "SDL_SDLrenderer" )
add_subdirectory( ext/SDL EXCLUDE_FROM_ALL )
add_subdirectory( ext/SDL_image EXCLUDE_FROM_ALL )
add_subdirectory( ext/glm EXCLUDE_FROM_ALL )
add_subdirectory( ext/json EXCLUDE_FROM_ALL )
add_subdirectory( ext/fmt EXCLUDE_FROM_ALL )
add_subdirectory( ext/freetype EXCLUDE_FROM_ALL )

add_library( Freetype::Freetype ALIAS freetype )
add_subdirectory( ext/RmlUi EXCLUDE_FROM_ALL )

if ( CC_HAS_DEVUI )
	add_library( ImGui
		ext/imgui/imconfig.h
		ext/imgui/imgui.cpp
		ext/imgui/imgui.h
		ext/imgui/imgui_demo.cpp
		ext/imgui/imgui_draw.cpp
		ext/imgui/imgui_internal.h
		ext/imgui/imgui_tables.cpp
		ext/imgui/imgui_widgets.cpp
		ext/imgui/imstb_rectpack.h
		ext/imgui/imstb_textedit.h
		ext/imgui/imstb_truetype.h
		ext/imgui/misc/cpp/imgui_stdlib.h
		ext/imgui/misc/cpp/imgui_stdlib.cpp
	)
	target_include_directories( ImGui PUBLIC ext/imgui ext/imgui/misc/cpp )
	target_compile_definitions( ImGui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS )

	add_library( ImGuiBackend
		ext/imgui/backends/imgui_impl_sdl3.cpp
		ext/imgui/backends/imgui_impl_sdl3.h
		ext/imgui/backends/imgui_impl_sdlrenderer3.cpp
		ext/imgui/backends/imgui_impl_sdlrenderer3.h
	)
	target_include_directories( ImGuiBackend PUBLIC ext/imgui/backends )
	target_link_libraries( ImGuiBackend PUBLIC ImGui SDL3-static )
endif()

if ( CC_HAS_EMBED_CONTENT )
	include( ext/cmrc/CMakeRC.cmake )
	file( GLOB_RECURSE CONTENT_FILES LIST_DIRECTORIES false "${CMAKE_CURRENT_SOURCE_DIR}/content/*.*" )
	cmrc_add_resource_library( content WHENCE content ${CONTENT_FILES} )
endif()

add_library( tileson INTERFACE )
target_include_directories( tileson INTERFACE ext/tileson )

add_library( RmlUiBackend ext/RmlUi/Backends/RmlUi_Platform_SDL.cpp ext/RmlUi/Backends/RmlUi_Renderer_SDL.cpp )
target_compile_definitions( RmlUiBackend PUBLIC RMLUI_SDL_VERSION_MAJOR=3 )
target_include_directories( RmlUiBackend PUBLIC ext/RmlUi/Backends )
target_link_libraries( RmlUiBackend PUBLIC RmlUi::Core SDL3-static SDL3_image-static )

add_library( build_metadata INTERFACE )
target_compile_definitions( build_metadata INTERFACE
	CC_BUILD_VERSION="${CC_BUILD_VERSION}"
	CC_BUILD_PLATFORM="${CC_BUILD_PLATFORM}"
	CC_BUILD_CONFIG="${CC_BUILD_CONFIG}"
	CC_BUILD_NUMBER="${CC_BUILD_NUMBER}"
	CC_BUILD_COMMIT="${CC_BUILD_COMMIT}"
)

if ( CC_HAS_EXECUTABLE_AS_LIBRARY )
	add_library( drunken_lobster_driving SHARED src/game/game.cpp )
else()
	add_executable( drunken_lobster_driving src/game/game.cpp )
endif()
target_include_directories( drunken_lobster_driving PUBLIC src/game )
target_compile_definitions( drunken_lobster_driving PUBLIC GLM_ENABLE_EXPERIMENTAL SDL_ASSERT_LEVEL=2 )
target_link_libraries( drunken_lobster_driving PUBLIC SDL_uclibc SDL3-static SDL3_image-static fmt glm nlohmann_json tileson RmlUiBackend build_metadata )
if ( CC_HAS_DEVUI )
	target_link_libraries( drunken_lobster_driving PUBLIC ImGuiBackend RmlUi::Debugger )
	target_compile_definitions( drunken_lobster_driving PUBLIC CC_HAS_DEVUI )
endif()
if ( CC_HAS_EMBED_CONTENT )
	target_link_libraries( drunken_lobster_driving PUBLIC content )
	target_compile_definitions( drunken_lobster_driving PUBLIC CC_HAS_EMBED_CONTENT )
endif()

if ( "${CMAKE_SYSTEM_NAME}" STREQUAL "Android" )
	install( TARGETS drunken_lobster_driving LIBRARY DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/android-project/drunken_lobster_driving/libs/arm64-v8a" )
	install( CODE "execute_process( COMMAND gradlew.bat build WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/android-project\" )" )
	install( CODE "execute_process( COMMAND gradlew.bat bundle WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/android-project\" )" )
	install( CODE "file( COPY \"${CMAKE_CURRENT_SOURCE_DIR}/android-project/drunken_lobster_driving/build/outputs/apk/release/drunken_lobster_driving-release.apk\" DESTINATION \"${CMAKE_INSTALL_PREFIX}\" )" )
	install( CODE "file( COPY \"${CMAKE_CURRENT_SOURCE_DIR}/android-project/drunken_lobster_driving/build/outputs/bundle/release/drunken_lobster_driving-release.aab\" DESTINATION \"${CMAKE_INSTALL_PREFIX}\" )" )
elseif ( "${CMAKE_SYSTEM_NAME}" STREQUAL "Emscripten" )
	install( FILES "$<TARGET_FILE_DIR:drunken_lobster_driving>/drunken_lobster_driving.js" DESTINATION "${CMAKE_INSTALL_PREFIX}" )
	install( FILES "$<TARGET_FILE_DIR:drunken_lobster_driving>/drunken_lobster_driving.html" DESTINATION "${CMAKE_INSTALL_PREFIX}" )
	install( FILES "$<TARGET_FILE_DIR:drunken_lobster_driving>/drunken_lobster_driving.wasm" DESTINATION "${CMAKE_INSTALL_PREFIX}" )
else()
	install( TARGETS drunken_lobster_driving RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}" )
endif()
