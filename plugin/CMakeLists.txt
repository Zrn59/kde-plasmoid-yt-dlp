cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOMOC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Widgets)
find_package(KF6 REQUIRED COMPONENTS I18n Notifications)

add_library(ytdlpplugin SHARED
    plugin.cpp
    ytdlpbackend.cpp
    ytdlpformats.cpp
    ytdlpactivemodel.cpp
)

target_link_libraries(ytdlpplugin
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Widgets
        KF6::I18n
        KF6::Notifications
        ${CMAKE_DL_LIBS}
)

target_compile_options(ytdlpplugin PRIVATE -Wall -Wextra)

set(YTLP_QML_MODULE_DIR "${CMAKE_BINARY_DIR}/qml/com/zrn59/ytdlp")
set_target_properties(ytdlpplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${YTLP_QML_MODULE_DIR}"
    PREFIX "lib"
    OUTPUT_NAME "ytdlpplugin"
)

add_custom_command(TARGET ytdlpplugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/qmldir"
            "${YTLP_QML_MODULE_DIR}/qmldir"
)

install(TARGETS ytdlpplugin LIBRARY DESTINATION "${PLUGIN_QML_DIR}")
install(FILES qmldir DESTINATION "${PLUGIN_QML_DIR}")
