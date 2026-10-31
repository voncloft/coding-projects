cmake_minimum_required(VERSION 3.21)

project(Todolist VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(todolist
    assets/resources.qrc
    src/EditableLabel.cpp
    src/EditableLabel.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/TodoItemWidget.cpp
    src/TodoItemWidget.h
    src/main.cpp
)

target_link_libraries(todolist PRIVATE Qt6::Widgets)
