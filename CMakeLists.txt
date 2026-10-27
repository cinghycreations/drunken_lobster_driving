cmake_minimum_required( VERSION 3.31 )
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_SCAN_FOR_MODULES OFF )
project( drunken_lobster_driving VERSION 0.0.0.0 )

option( CC_HAS_BUILDER "Add builder project" ON )
option( CC_HAS_DEVUI "Enable Dev Ui" ON )

set( BUILD_SHARED_LIBS OFF )
set( RMLUI_BACKEND "SDL_SDLrenderer" )
set( SDL_STATIC ON )
set( SDLIMAGE_AVIF OFF )
set( SDLIMAGE_BMP OFF )
set( SDLIMAGE_DEPS_SHARED OFF )
set( SDLIMAGE_GIF OFF )
set( SDLIMAGE_JPG ON )
set( SDLIMAGE_JXL OFF )
set( SDLIMAGE_LBM OFF )
set( SDLIMAGE_PCX OFF )
set( SDLIMAGE_PNG ON )
set( SDLIMAGE_PNM OFF )
set( SDLIMAGE_QOI OFF )
set( SDLIMAGE_STRICT ON )
set( SDLIMAGE_SVG OFF )
set( SDLIMAGE_TGA ON )
set( SDLIMAGE_TIF OFF )
set( SDLIMAGE_VENDORED ON )
set( SDLIMAGE_WEBP ON )
set( SDLIMAGE_XCF OFF )
set( SDLIMAGE_XPM OFF )
set( SDLIMAGE_XV OFF )
add_subdirectory( ext/CLI11 EXCLUDE_FROM_ALL )
add_subdirectory( ext/fmt EXCLUDE_FROM_ALL )
add_subdirectory( ext/freetype EXCLUDE_FROM_ALL )
add_subdirectory( ext/glm EXCLUDE_FROM_ALL )
add_subdirectory( ext/json EXCLUDE_FROM_ALL )
add_subdirectory( ext/SDL EXCLUDE_FROM_ALL )
add_subdirectory( ext/SDL_image EXCLUDE_FROM_ALL )

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

add_library( tileson INTERFACE )
target_include_directories( tileson INTERFACE ext/tileson )

add_library( RmlUiBackend ext/RmlUi/Backends/RmlUi_Platform_SDL.cpp ext/RmlUi/Backends/RmlUi_Renderer_SDL.cpp )
target_compile_definitions( RmlUiBackend PUBLIC RMLUI_SDL_VERSION_MAJOR=3 )
target_include_directories( RmlUiBackend PUBLIC ext/RmlUi/Backends )
target_link_libraries( RmlUiBackend PUBLIC RmlUi::Core SDL3-static SDL3_image-static )

add_library( stb INTERFACE )
target_include_directories( stb INTERFACE ext/stb )

add_library( gamelib src/gamelib/gamelib.cpp src/gamelib/gamelib.hpp )
target_include_directories( gamelib PUBLIC src/gamelib )
target_compile_definitions( gamelib PUBLIC GLM_ENABLE_EXPERIMENTAL SDL_ASSERT_LEVEL=2 )
target_compile_definitions( gamelib PUBLIC CC_VERSION_MAJOR="${CMAKE_PROJECT_VERSION_MAJOR}" CC_VERSION_MINOR="${CMAKE_PROJECT_VERSION_MINOR}" CC_VERSION_PATCH="${CMAKE_PROJECT_VERSION_PATCH}" CC_VERSION_BUILD="${CMAKE_PROJECT_VERSION_TWEAK}" )
target_link_libraries( gamelib PUBLIC SDL3-static SDL3_image-static fmt glm nlohmann_json tileson RmlUiBackend stb )
if ( CC_HAS_DEVUI )
	target_link_libraries( gamelib PUBLIC ImGuiBackend RmlUi::Debugger )
	target_compile_definitions( gamelib PUBLIC CC_HAS_DEVUI )
endif()

if ( "${CMAKE_SYSTEM_NAME}" STREQUAL "Android" )
	add_library( game SHARED src/game/main.cpp )
else()
	add_executable( game src/game/main.cpp )
endif()
target_link_libraries( game PUBLIC gamelib )

if ( CC_HAS_BUILDER )
	add_executable( builder src/builder/builder.cpp )
	target_link_libraries( builder PUBLIC gamelib CLI11 )
	install( TARGETS builder RUNTIME DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/content" )
endif()

if ( "${CMAKE_SYSTEM_NAME}" STREQUAL "Android" )
	install( TARGETS game LIBRARY DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/android-project/drunken_lobster_driving/libs/arm64-v8a" )
	install( CODE "execute_process( COMMAND gradlew.bat build TIMEOUT 120 WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/android-project\" )" )
	install( CODE "execute_process( COMMAND gradlew.bat bundle TIMEOUT 120 WORKING_DIRECTORY \"${CMAKE_CURRENT_SOURCE_DIR}/android-project\" )" )
	install( CODE "file( COPY \"${CMAKE_CURRENT_SOURCE_DIR}/android-project/drunken_lobster_driving/build/outputs/apk/release/drunken_lobster_driving-release.apk\" DESTINATION \"${CMAKE_INSTALL_PREFIX}\" )" )
	install( CODE "file( COPY \"${CMAKE_CURRENT_SOURCE_DIR}/android-project/drunken_lobster_driving/build/outputs/bundle/release/drunken_lobster_driving-release.aab\" DESTINATION \"${CMAKE_INSTALL_PREFIX}\" )" )
elseif ( "${CMAKE_SYSTEM_NAME}" STREQUAL "Emscripten" )
	set_property( TARGET game PROPERTY OUTPUT_NAME index )
	set_property( TARGET game PROPERTY SUFFIX .html )
	install( FILES "$<TARGET_FILE_DIR:game>/index.js" DESTINATION "${CMAKE_INSTALL_PREFIX}" )
	install( FILES "$<TARGET_FILE_DIR:game>/index.html" DESTINATION "${CMAKE_INSTALL_PREFIX}" )
	install( FILES "$<TARGET_FILE_DIR:game>/index.wasm" DESTINATION "${CMAKE_INSTALL_PREFIX}" )
else()
	install( TARGETS game RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}" )
endif()
