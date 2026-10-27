cmake_minimum_required(VERSION 3.16)

project(kde-plasmoid-yt-dlp VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

set(PLASMOID_ID "com.zrn59.ytdlp")
set(PLASMOID_INSTALL_DIR "${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/${PLASMOID_ID}")
set(PLUGIN_QML_DIR "${PLASMOID_INSTALL_DIR}/contents/qml/com/zrn59/ytdlp")

add_subdirectory(plugin)

install(FILES package/metadata.json DESTINATION "${PLASMOID_INSTALL_DIR}")
install(DIRECTORY package/contents/config DESTINATION "${PLASMOID_INSTALL_DIR}/contents")
install(DIRECTORY package/contents/ui DESTINATION "${PLASMOID_INSTALL_DIR}/contents"
        PATTERN "*.qmlc" EXCLUDE
        PATTERN "*.jsc" EXCLUDE)
install(FILES package/notifyrc/com.zrn59.ytdlp.notifyrc
        DESTINATION "${CMAKE_INSTALL_DATADIR}/knotifications6")
install(PROGRAMS scripts/yt-dlp-update.sh
        DESTINATION "${CMAKE_INSTALL_DATADIR}/com.zrn59.ytdlp/bin")
install(PROGRAMS scripts/yt-dlp-update.sh
        DESTINATION "${PLASMOID_INSTALL_DIR}/contents/scripts")

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    add_executable(ytdlp-format-test tests/format_test.cpp plugin/ytdlpformats.cpp)
    target_link_libraries(ytdlp-format-test PRIVATE Qt6::Core ${CMAKE_DL_LIBS})
    target_include_directories(ytdlp-format-test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/plugin")
    add_test(NAME formats COMMAND ytdlp-format-test)
endif()
