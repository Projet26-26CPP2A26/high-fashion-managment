cmake_minimum_required(VERSION 3.16)
project(AureaClients VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Charts)

add_executable(AureaClients WIN32
    main.cpp common/icons.h common/appsetup.h
    clients/clientwindow.cpp clients/clientwindow.h clients/clientwindow.ui
    clients/clientdatabase.cpp clients/clientdatabase.h clients/clients.qrc
    assets.qrc)
target_include_directories(AureaClients PRIVATE common clients)
target_link_libraries(AureaClients PRIVATE Qt6::Widgets Qt6::Sql Qt6::Charts)
