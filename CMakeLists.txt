cmake_minimum_required(VERSION 3.16)
project(AureaCommandes VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql PrintSupport Charts)

add_executable(AureaCommandes WIN32
    main.cpp common/icons.h common/appsetup.h
    orders/orderwindow.cpp orders/orderwindow.h orders/orders.qrc
    clients/clientdatabase.cpp clients/clientdatabase.h
    employees/pdfexport.cpp employees/pdfexport.h
    assets.qrc chevrons.qrc)
target_include_directories(AureaCommandes PRIVATE common orders clients employees)
target_link_libraries(AureaCommandes PRIVATE Qt6::Widgets Qt6::Sql Qt6::PrintSupport Qt6::Charts)
