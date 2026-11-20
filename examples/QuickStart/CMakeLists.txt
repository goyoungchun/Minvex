cmake_minimum_required(VERSION 3.24)
if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    # Keep compiler probes independent of the MSVC compiler PDB service.
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
endif()
project(MinvexQuickStart LANGUAGES CXX)

find_package(Minvex 1.0.2 CONFIG REQUIRED COMPONENTS viewer graphics)
add_executable(minvex_quickstart Main.cpp)
target_link_libraries(minvex_quickstart PRIVATE minvex::viewer)
add_executable(minvex_quickstart_procedural Procedural.cpp)
target_link_libraries(minvex_quickstart_procedural PRIVATE minvex::viewer)
