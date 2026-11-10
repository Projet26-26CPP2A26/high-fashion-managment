cmake_minimum_required(VERSION 3.16)
project(AureaMatieres VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport Network Svg)

# --- Connexion Oracle (ODBC / QODBC) : singleton + test de bout en bout (--oracle-test) ---
add_library(oracle_core STATIC
    common/oracleconnection.cpp common/oracleconnection.h
    common/oraclecheck.cpp common/oraclecheck.h)
target_include_directories(oracle_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(oracle_core PUBLIC Qt6::Core Qt6::Sql)

# --- Cœur « Matières » (stock SQLite ou Oracle, QR codes, rapports PDF) ---
add_library(material_core STATIC
    materials/inventoryrepository.cpp materials/inventoryrepository.h
    materials/qrservice.cpp materials/qrservice.h
    materials/reports.cpp materials/reports.h)
target_include_directories(material_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/materials)
target_link_libraries(material_core PUBLIC oracle_core Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::Network Qt6::Svg)

# --- Interface ---
set(UI_SOURCES
    common/icons.h common/sqldialect.h
    materials/materialwindow.cpp materials/materialwindow.h materials/materialwindow.ui materials/materials.qrc)

add_executable(AureaMatieres WIN32 main.cpp ${UI_SOURCES})
target_include_directories(AureaMatieres PRIVATE common materials)
target_link_libraries(AureaMatieres PRIVATE material_core oracle_core)

if(MINGW)
    target_compile_options(AureaMatieres PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(material_tests tests/test_materials.cpp ${UI_SOURCES})
    target_include_directories(material_tests PRIVATE common materials)
    target_link_libraries(material_tests PRIVATE material_core Qt6::Test)
    add_test(NAME material_module COMMAND material_tests -platform offscreen)
endif()
