cmake_minimum_required(VERSION 3.19)

project(amber-player
    VERSION 1.0.0
    DESCRIPTION "A music player for local MP3, FLAC and WebM files"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Multimedia Concurrent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib>=1.12)

qt_standard_project_setup()

qt_add_executable(amber-player
    src/main.cpp
    src/CoverLyricsView.cpp src/CoverLyricsView.h
    src/Icons.cpp src/Icons.h
    src/ImageUtils.cpp src/ImageUtils.h
    src/JumpSlider.h
    src/Library.cpp src/Library.h
    src/LibraryView.cpp src/LibraryView.h
    src/Lyrics.cpp src/Lyrics.h
    src/MainWindow.cpp src/MainWindow.h
    src/MetadataReader.cpp src/MetadataReader.h
    src/NowPlayingPanel.cpp src/NowPlayingPanel.h
    src/PlayQueue.cpp src/PlayQueue.h
    src/PlayerController.cpp src/PlayerController.h
    src/QueueView.cpp src/QueueView.h
    src/Theme.cpp src/Theme.h
    src/Track.cpp src/Track.h
)

target_compile_definitions(amber-player PRIVATE
    AMBER_VERSION="${PROJECT_VERSION}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(amber-player PRIVATE -Wall -Wextra)
endif()

target_link_libraries(amber-player PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Concurrent
    PkgConfig::TAGLIB
)

install(TARGETS amber-player RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/amber-player.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/amber-player.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

option(AMBER_BUILD_TESTS "Build the unit tests" OFF)
if(AMBER_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    foreach(test lyrics playqueue)
        qt_add_executable(tst_${test} tests/tst_${test}.cpp src/Lyrics.cpp src/PlayQueue.cpp src/Track.cpp)
        target_include_directories(tst_${test} PRIVATE src)
        target_link_libraries(tst_${test} PRIVATE Qt6::Core Qt6::Test)
        add_test(NAME ${test} COMMAND tst_${test})
    endforeach()

    qt_add_executable(tst_library tests/tst_library.cpp
        src/Library.cpp src/MetadataReader.cpp src/ImageUtils.cpp src/Lyrics.cpp src/Track.cpp)
    target_include_directories(tst_library PRIVATE src)
    target_link_libraries(tst_library PRIVATE Qt6::Gui Qt6::Concurrent Qt6::Test PkgConfig::TAGLIB)
    add_test(NAME library COMMAND tst_library)
endif()
