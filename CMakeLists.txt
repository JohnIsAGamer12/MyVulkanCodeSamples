﻿# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
# if (POLICY CMP0141)
#   cmake_policy(SET CMP0141 NEW)
#   set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,  
#	$<CXX_COMPILER_ID:MSVC>>,
# $<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,
# $<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
# endif()

project ("MyVulkanCodeSamples")

# Include sub-projects.
add_subdirectory("push_constants")
add_subdirectory("gltfModelLoader")
add_subdirectory("2D_Starfield")
add_subdirectory("bindlesstexturearray")
add_subdirectory("pbrRenderer")
add_subdirectory("storage_buffers")
add_subdirectory("uniform_buffers")
