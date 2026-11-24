cmake_minimum_required(VERSION 3.22)
project(antigrav LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(FetchContent)

# Dépendances épinglées, téléchargées en archives : bien plus rapide qu'un
# clone git. SOURCE_SUBDIR pointe sur un dossier absent pour récupérer les
# sources sans exécuter le CMake du projet amont.
set(GH https://github.com)
FetchContent_Declare(vulkan_headers
  URL ${GH}/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz
  SOURCE_SUBDIR _none)
FetchContent_Declare(volk
  URL ${GH}/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.357.0.tar.gz
  SOURCE_SUBDIR _none)
FetchContent_Declare(vma
  URL ${GH}/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.5.0.tar.gz
  SOURCE_SUBDIR _none)
FetchContent_Declare(glm
  URL ${GH}/g-truc/glm/archive/refs/tags/1.0.3.tar.gz
  SOURCE_SUBDIR _none)
FetchContent_Declare(stb
  URL ${GH}/nothings/stb/archive/6b5799bae8a1fe3792691ef4b72ae26434c3e799.tar.gz
  SOURCE_SUBDIR _none)
FetchContent_MakeAvailable(vulkan_headers volk vma glm stb)

if(ANDROID)
  # SDL est compilé avec le jeu. ANTIGRAV_SDL_ARCHIVE peut désigner une archive
  # déjà téléchargée.
  set(ANTIGRAV_SDL_ARCHIVE "https://www.libsdl.org/release/SDL3-3.4.16.tar.gz"
      CACHE STRING "Archive des sources de SDL3")
  FetchContent_Declare(SDL3 URL ${ANTIGRAV_SDL_ARCHIVE}
    URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL3)
else()
  find_package(SDL3 3.2 REQUIRED CONFIG)
endif()

# Bibliothèques tierces compilées sans avertissements.
add_library(third_party STATIC
  ${volk_SOURCE_DIR}/volk.c
  src/third_party/vma.cpp
  src/third_party/stb.cpp)
target_include_directories(third_party SYSTEM PUBLIC
  ${vulkan_headers_SOURCE_DIR}/include
  ${volk_SOURCE_DIR}
  ${vma_SOURCE_DIR}/include
  ${glm_SOURCE_DIR}
  ${stb_SOURCE_DIR})
target_compile_definitions(third_party PUBLIC
  VK_NO_PROTOTYPES
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYNAMIC_VULKAN_FUNCTIONS=0
  GLM_FORCE_DEPTH_ZERO_TO_ONE
  GLM_FORCE_RADIANS
  GLM_ENABLE_EXPERIMENTAL)
target_compile_options(third_party PRIVATE -w)
set_target_properties(third_party PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Shaders : GLSL compilé en SPIR-V au build, puis inclus dans le binaire sous
# forme de tableaux C. Aucun fichier de shader à charger à l'exécution.
if(ANDROID)
  find_program(GLSLC glslc HINTS ${ANDROID_NDK}/shader-tools/linux-x86_64 REQUIRED NO_CMAKE_FIND_ROOT_PATH)
else()
  find_program(GLSLC glslc REQUIRED)
endif()

set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_SOURCE_DIR}/shaders/*.vert
  ${CMAKE_SOURCE_DIR}/shaders/*.frag)
set(SHADER_OUTPUTS)
foreach(src ${SHADER_SOURCES})
  get_filename_component(name ${src} NAME)
  string(REPLACE "." "_" sym ${name})
  set(out ${SHADER_OUT_DIR}/${sym}.inc)
  add_custom_command(
    OUTPUT ${out}
    COMMAND ${GLSLC} --target-env=vulkan1.3 -O -mfmt=c
            -I ${CMAKE_SOURCE_DIR}/shaders -MD -MF ${out}.d -o ${out} ${src}
    DEPENDS ${src}
    DEPFILE ${out}.d
    COMMENT "glslc ${name}"
    VERBATIM)
  list(APPEND SHADER_OUTPUTS ${out})
endforeach()
# Polices incluses dans le binaire de la même façon.
foreach(font Orbitron-Medium.ttf Orbitron-Bold.ttf)
  set(out ${SHADER_OUT_DIR}/${font}.inc)
  add_custom_command(
    OUTPUT ${out}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${CMAKE_SOURCE_DIR}/assets/fonts/${font} -DOUTPUT=${out}
            -P ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
    DEPENDS ${CMAKE_SOURCE_DIR}/assets/fonts/${font} ${CMAKE_SOURCE_DIR}/cmake/embed.cmake
    COMMENT "embed ${font}"
    VERBATIM)
  list(APPEND SHADER_OUTPUTS ${out})
endforeach()
add_custom_target(shaders DEPENDS ${SHADER_OUTPUTS})

# La simulation ne dépend ni de Vulkan ni de SDL : elle se compile et se teste
# seule.
add_library(sim STATIC src/sim/track.cpp src/sim/world.cpp)
target_include_directories(sim PUBLIC src)
target_include_directories(sim SYSTEM PUBLIC ${glm_SOURCE_DIR})
target_compile_definitions(sim PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS GLM_ENABLE_EXPERIMENTAL)
target_compile_options(sim PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)
set_target_properties(sim PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(NOT ANDROID)
  enable_testing()
  add_executable(sim_tests tests/sim_tests.cpp src/game/synth.cpp)
  target_link_libraries(sim_tests PRIVATE sim)
  add_test(NAME sim COMMAND sim_tests)
endif()

set(GAME_SOURCES
  src/main.cpp
  src/core/log.cpp
  src/gfx/gpu.cpp
  src/gfx/swapchain.cpp
  src/gfx/pipeline.cpp
  src/gfx/renderer.cpp
  src/gfx/capture.cpp
  src/gfx/mesh.cpp
  src/gfx/geometry.cpp
  src/gfx/hud.cpp
  src/game/hud.cpp
  src/game/input.cpp
  src/game/synth.cpp
  src/game/audio.cpp
  src/game/race.cpp)

if(ANDROID)
  # SDL charge le jeu comme bibliothèque partagée nommée "main".
  add_library(main SHARED ${GAME_SOURCES})
  set(GAME_TARGET main)
else()
  add_executable(antigrav ${GAME_SOURCES})
  set(GAME_TARGET antigrav)
endif()

add_dependencies(${GAME_TARGET} shaders)
target_include_directories(${GAME_TARGET} PRIVATE src ${SHADER_OUT_DIR})
target_link_libraries(${GAME_TARGET} PRIVATE sim third_party SDL3::SDL3)
target_compile_options(${GAME_TARGET} PRIVATE
  -Wall -Wextra -Wno-missing-field-initializers -fno-exceptions -fno-rtti)
