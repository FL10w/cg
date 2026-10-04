cmake_minimum_required(VERSION 3.20)
include(FetchContent)

project(cg-lab-01 LANGUAGES C CXX)

# Optional SDK installed locally by tools/setup-local.sh (no root privileges).
set(LAB_SYSROOT "${CMAKE_SOURCE_DIR}/.local/sysroot/usr")
if(UNIX AND EXISTS "${LAB_SYSROOT}/include/vulkan/vulkan.h")
    list(PREPEND CMAKE_PREFIX_PATH "${LAB_SYSROOT}")
    list(PREPEND CMAKE_INCLUDE_PATH "${LAB_SYSROOT}/include")
    list(PREPEND CMAKE_LIBRARY_PATH "${LAB_SYSROOT}/lib/x86_64-linux-gnu")
    set(Vulkan_INCLUDE_DIR "${LAB_SYSROOT}/include" CACHE PATH "Vulkan headers")
    set(Vulkan_LIBRARY "${LAB_SYSROOT}/lib/x86_64-linux-gnu/libvulkan.so" CACHE FILEPATH "Vulkan loader")
    set(Vulkan_GLSLC_EXECUTABLE "${LAB_SYSROOT}/bin/glslc" CACHE FILEPATH "Shader compiler")
endif()

add_executable(${PROJECT_NAME} source/main.cpp source/graphics_internal.cpp source/application.cpp)

if(UNIX AND NOT APPLE)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
elseif(APPLE)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
elseif(WIN32)
	if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4201 /utf-8)
	else()
		target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
	endif()
	target_compile_definitions(${PROJECT_NAME} PRIVATE -D_USE_MATH_DEFINES)
else()
	message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD_REQUIRED TRUE CXX_STANDARD 20)

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw.git
	GIT_TAG 3.4
)

FetchContent_Declare(
	vk-bootstrap
	GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
	GIT_TAG v1.4.321
)

FetchContent_Declare(
	VulkanMemoryAllocator
	GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
	GIT_TAG v3.4.0
)

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui.git
	GIT_TAG v1.92.9
)

find_package(Vulkan REQUIRED)

set(GLFW_LIBRARY_TYPE STATIC)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Use the X11 backend on Linux")
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)

# Reuse locally cached dependency sources after moving the lab directory.
foreach(DEPENDENCY GLFW VK-BOOTSTRAP VULKANMEMORYALLOCATOR IMGUI)
    string(TOLOWER "${DEPENDENCY}" DEPENDENCY_DIRECTORY)
    set(LOCAL_SOURCE "${CMAKE_SOURCE_DIR}/.local/sources/${DEPENDENCY_DIRECTORY}")
    if(IS_DIRECTORY "${LOCAL_SOURCE}")
        set("FETCHCONTENT_SOURCE_DIR_${DEPENDENCY}" "${LOCAL_SOURCE}" CACHE PATH "Local dependency source")
    endif()
endforeach()

FetchContent_MakeAvailable(
	glfw
	vk-bootstrap
	VulkanMemoryAllocator
	imgui
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	glfw
	Vulkan::Vulkan
	vk-bootstrap::vk-bootstrap
	GPUOpen::VulkanMemoryAllocator
)

target_include_directories(${PROJECT_NAME} PRIVATE ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_sources(${PROJECT_NAME} PRIVATE
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
	${imgui_SOURCE_DIR}/imgui.cpp
	${imgui_SOURCE_DIR}/imgui_draw.cpp
	${imgui_SOURCE_DIR}/imgui_tables.cpp
	${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

# Shader compilation is mandatory: fail during configuration if glslc is missing.
find_program(LAB_GLSLC glslc HINTS "${LAB_SYSROOT}/bin" "$ENV{VULKAN_SDK}/Bin" REQUIRED)
set(SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_DIR}")
foreach(SHADER torus.vert torus.frag)
    add_custom_command(OUTPUT "${SHADER_DIR}/${SHADER}.spv"
        COMMAND ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${LAB_SYSROOT}/lib/x86_64-linux-gnu:$ENV{LD_LIBRARY_PATH}" "${LAB_GLSLC}" --target-env=vulkan1.1 "${CMAKE_SOURCE_DIR}/shaders/${SHADER}"
                -o "${SHADER_DIR}/${SHADER}.spv"
        DEPENDS "${CMAKE_SOURCE_DIR}/shaders/${SHADER}" VERBATIM)
    list(APPEND SHADER_BINARIES "${SHADER_DIR}/${SHADER}.spv")
endforeach()
add_custom_target(shaders DEPENDS ${SHADER_BINARIES})
add_dependencies(${PROJECT_NAME} shaders)
target_compile_definitions(${PROJECT_NAME} PRIVATE LAB_SHADER_DIR="${SHADER_DIR}" LAB_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
include(CTest)
if(BUILD_TESTING)
    add_executable(scene-tests tests/scene_tests.cpp)
    target_include_directories(scene-tests PRIVATE source)
    target_compile_features(scene-tests PRIVATE cxx_std_20)
    add_test(NAME scene-math-and-geometry COMMAND scene-tests)
endif()
