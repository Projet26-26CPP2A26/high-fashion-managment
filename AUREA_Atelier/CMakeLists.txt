cmake_minimum_required(VERSION 3.16)
project(AureaAtelier VERSION 1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport Charts)

# --- Cœur « Employés » (base de données, règles métier, PDF) ---
add_library(employee_core STATIC
    employees/database.cpp employees/database.h
    employees/employeerepository.cpp employees/employeerepository.h
    employees/pdfexport.cpp employees/pdfexport.h)
target_include_directories(employee_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/employees)
target_link_libraries(employee_core PUBLIC oracle_core Qt6::Widgets Qt6::Sql Qt6::PrintSupport)

# --- Cœur « Matières » (stock SQLite, QR codes, rapports PDF) ---
add_library(quirc STATIC
    materials/third_party/quirc/quirc.c materials/third_party/quirc/decode.c
    materials/third_party/quirc/identify.c materials/third_party/quirc/version_db.c)
target_include_directories(quirc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/materials/third_party/quirc)
if(NOT MSVC)
    target_link_libraries(quirc PUBLIC m)
endif()
add_library(material_core STATIC
    materials/inventoryrepository.cpp materials/inventoryrepository.h
    materials/qrservice.cpp materials/qrservice.h
    materials/reports.cpp materials/reports.h
    materials/third_party/qrcodegen/qrcodegen.cpp)
target_include_directories(material_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/materials ${CMAKE_CURRENT_SOURCE_DIR}/materials/third_party/qrcodegen)
target_link_libraries(material_core PUBLIC quirc oracle_core Qt6::Widgets Qt6::Sql Qt6::PrintSupport)

# --- Connexion Oracle (ODBC / QODBC) : singleton + test de bout en bout (--oracle-test) ---
add_library(oracle_core STATIC
    common/oracleconnection.cpp common/oracleconnection.h
    common/oraclecheck.cpp common/oraclecheck.h)
target_include_directories(oracle_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)
target_link_libraries(oracle_core PUBLIC Qt6::Core Qt6::Sql)

# --- Interfaces : employés, matières, production, fenêtre principale ---
set(UI_SOURCES
    common/icons.h common/sqldialect.h
    common/oraclemodulecheck.cpp common/oraclemodulecheck.h
    employees/employeewindow.cpp employees/employeewindow.h employees/employeewindow.ui
    employees/employeemodule.cpp employees/employeemodule.h employees/employees.ui
    employees/assets.qrc
    materials/materialwindow.cpp materials/materialwindow.h materials/materialwindow.ui materials/materials.qrc
    production/productionwindow.cpp production/productionwindow.h
    production/productionpages.cpp production/productionpages.h
    production/productionstore.cpp production/productionstore.h
    production/productionkit.cpp production/productionkit.h
    production/production.qrc
    shell/shellwindow.cpp shell/shellwindow.h shell/shell.qrc)

add_executable(AureaAtelier WIN32 main.cpp ${UI_SOURCES})
target_include_directories(AureaAtelier PRIVATE common employees materials production shell)
target_link_libraries(AureaAtelier PRIVATE employee_core material_core oracle_core Qt6::Charts)

if(MINGW)
    target_compile_options(employee_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(AureaAtelier PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(employee_tests tests/test_employees.cpp ${UI_SOURCES})
    target_include_directories(employee_tests PRIVATE common employees materials production shell)
    target_link_libraries(employee_tests PRIVATE employee_core material_core Qt6::Charts Qt6::Test)
    add_test(NAME employee_module COMMAND employee_tests -platform offscreen)

    add_executable(material_tests tests/test_materials.cpp
        materials/materialwindow.cpp materials/materialwindow.h materials/materialwindow.ui materials/materials.qrc)
    target_include_directories(material_tests PRIVATE common materials)
    target_link_libraries(material_tests PRIVATE material_core Qt6::Test)
    add_test(NAME material_module COMMAND material_tests -platform offscreen)
endif()
