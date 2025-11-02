cmake_minimum_required( VERSION 3.31 )
set( CMAKE_CXX_STANDARD 20 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
project( drunken_lobster_driving )
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
add_subdirectory( ext/SDL EXCLUDE_FROM_ALL )
add_subdirectory( ext/SDL_image EXCLUDE_FROM_ALL )
add_subdirectory( ext/glm EXCLUDE_FROM_ALL )
add_subdirectory( ext/json EXCLUDE_FROM_ALL )
add_subdirectory( ext/fmt EXCLUDE_FROM_ALL )

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
)
target_include_directories( ImGui PUBLIC ext/imgui )
target_compile_definitions( ImGui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS )

add_library( ImGuiBackend
	ext/imgui/backends/imgui_impl_sdl3.cpp
	ext/imgui/backends/imgui_impl_sdl3.h
	ext/imgui/backends/imgui_impl_sdlrenderer3.cpp
	ext/imgui/backends/imgui_impl_sdlrenderer3.h
)
target_include_directories( ImGuiBackend PUBLIC ext/imgui/backends )
target_link_libraries( ImGuiBackend PUBLIC ImGui SDL3-static )

add_library( tileson INTERFACE )
target_include_directories( tileson INTERFACE ext/tileson )

add_executable( game src/game/game.cpp )
target_include_directories( game PUBLIC src/game )
target_compile_definitions( game PUBLIC GLM_ENABLE_EXPERIMENTAL SDL_ASSERT_LEVEL=2 )
target_link_libraries( game PUBLIC SDL_uclibc SDL3-static SDL3_image-static fmt glm nlohmann_json ImGui ImGuiBackend tileson )
