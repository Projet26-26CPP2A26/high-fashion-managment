cmake_minimum_required(VERSION 3.16)
project(AureaEmployes VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport Charts)

add_executable(AureaEmployes WIN32
    main.cpp common/icons.h common/appsetup.h
    employees/employeewindow.cpp employees/employeewindow.h employees/employeewindow.ui
    employees/employeemodule.cpp employees/employeemodule.h employees/employees.ui
    employees/employeerepository.cpp employees/employeerepository.h
    employees/database.cpp employees/database.h
    employees/pdfexport.cpp employees/pdfexport.h
    employees/assets.qrc)
target_include_directories(AureaEmployes PRIVATE common employees)
target_link_libraries(AureaEmployes PRIVATE Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::Charts)
