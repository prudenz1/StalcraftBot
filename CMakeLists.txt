cmake_minimum_required(VERSION 3.20)
project(StalcraftBot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Charts)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PostgreSQL REQUIRED)

set(SOURCES
    src/main.cpp

    src/core/ApiClient.cpp
    src/core/Database.cpp
    src/core/PriceAnalyzer.cpp
    src/core/DealDetector.cpp
    src/core/Scheduler.cpp
    src/core/Config.cpp
    src/core/ItemCatalogLoader.cpp

    src/ui/MainWindow.cpp
    src/ui/ItemManagerWidget.cpp
    src/ui/ActiveLotsWidget.cpp
    src/ui/PriceTableWidget.cpp
    src/ui/PriceChartWidget.cpp
    src/ui/LogWidget.cpp
    src/ui/SettingsWidget.cpp

    src/utils/Logger.cpp
)

set(HEADERS
    src/core/ApiClient.h
    src/core/Database.h
    src/core/PriceAnalyzer.h
    src/core/DealDetector.h
    src/core/Scheduler.h
    src/core/Config.h
    src/core/ItemCatalogLoader.h

    src/models/Item.h
    src/models/Lot.h
    src/models/PriceSnapshot.h
    src/models/Alert.h

    src/ui/MainWindow.h
    src/ui/ItemManagerWidget.h
    src/ui/ActiveLotsWidget.h
    src/ui/PriceTableWidget.h
    src/ui/PriceChartWidget.h
    src/ui/LogWidget.h
    src/ui/SettingsWidget.h

    src/utils/Logger.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PostgreSQL_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Charts
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ${PostgreSQL_LIBRARIES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:QT_QML_DEBUG>
)
