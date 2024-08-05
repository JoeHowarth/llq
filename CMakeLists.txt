# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

project(llq VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
IF( NOT CMAKE_BUILD_TYPE )
    # set(CMAKE_BUILD_TYPE Debug)
   set( CMAKE_BUILD_TYPE Release )
ENDIF()

# Enable vcpkg integration
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")


# Add source files
add_executable(
    ${PROJECT_NAME} 
    src/main.cpp 
    )

add_executable(
    test_main 
    src/test_main.cpp 
    )

add_executable(
    logProducerBin
    src/logProducerBin.cpp 
    )

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(test_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED TRUE)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG main # Important: Specify a version or a commit hash here.
)
FetchContent_MakeAvailable(ftxui)

# Find and link dependencies (example: fmt)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(folly REQUIRED)


target_link_libraries(${PROJECT_NAME}
    PRIVATE
        fmt::fmt
        nlohmann_json::nlohmann_json
        ftxui::screen
        ftxui::dom
        ftxui::component
        Folly::folly
)

target_link_libraries(test_main
    PRIVATE
        fmt::fmt
        nlohmann_json::nlohmann_json
        ftxui::screen
        ftxui::dom
        ftxui::component
        Folly::folly
)

target_link_libraries(logProducerBin
    PRIVATE
        fmt::fmt
        nlohmann_json::nlohmann_json
        ftxui::screen
        ftxui::dom
        ftxui::component
        Folly::folly
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
