if(NOT ${OSMSCOUT_BUILD_MAP_QT})
	message(SEND_ERROR "The Qt map drawing backend is required for OSMScout2 application")
endif()
if(NOT Qt5Core_FOUND)
	message(SEND_ERROR "Qt5Core is required for OSMScout2 application")
endif()
if(NOT Qt5Gui_FOUND)
	message(SEND_ERROR "Qt5Gui is required for OSMScout2 application")
endif()
if(NOT Qt5Widgets_FOUND)
	message(SEND_ERROR "Qt5Widgets is required for OSMScout2 application")
endif()
if(NOT Qt5Qml_FOUND)
	message(SEND_ERROR "Qt5Qml is required for OSMScout2 application")
endif()
if(NOT Qt5Quick_FOUND)
	message(SEND_ERROR "Qt5Quick is required for OSMScout2 application")
endif()
if(NOT Qt5Svg_FOUND)
	message(SEND_ERROR "Qt5Svg is required for OSMScout2 application")
endif()
if(NOT Qt5Positioning_FOUND)
	message(SEND_ERROR "Qt5Positioning is required for OSMScout2 application")
endif()
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(HEADER_FILES
    src/DBThread.h
    src/MapWidget.h
    src/RoutingModel.h
    src/SearchLocationModel.h
    src/Settings.h
    src/Theme.h
)

set(SOURCE_FILES
    src/DBThread.cpp
    src/MapWidget.cpp
    src/OSMScout.cpp
    src/RoutingModel.cpp
    src/SearchLocationModel.cpp
    src/Settings.cpp
    src/Theme.cpp
)

set(RESOURCE_FILES
    res.qrc
)

add_executable(OSMScout2 ${SOURCE_FILES} ${HEADER_FILES} ${RESOURCE_FILES})

target_include_directories(OSMScout2 PRIVATE 
		${OSMSCOUT_BASE_DIR_SOURCE}/libosmscout/include
		${OSMSCOUT_BASE_DIR_SOURCE}/libosmscout-map/include
		${OSMSCOUT_BASE_DIR_SOURCE}/libosmscout-map-qt/include)

target_link_libraries(OSMScout2
        libosmscout
        libosmscout_map
        Qt5::Core
        Qt5::Gui
        Qt5::Widgets
        Qt5::Qml
        Qt5::Quick
        Qt5::Svg
        Qt5::Positioning
        libosmscout_map_qt)

set_property(TARGET OSMScout2 PROPERTY CXX_STANDARD 11)
