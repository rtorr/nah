add_executable(nah tools.cpp)

target_link_libraries(nah PRIVATE CLI11::CLI11 nahhost nah_packaging)
target_include_directories(nah PRIVATE ${PROJECT_SOURCE_DIR}/include)
