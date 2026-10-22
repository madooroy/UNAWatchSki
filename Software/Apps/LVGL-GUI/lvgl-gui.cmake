# Sources and include directories of the Ski Day LVGL GUI process. The
# generated font C files under assets/ are committed, so no converter is
# needed to build (regenerate with the SDK's lvgl_assets.py, see README).
file(GLOB_RECURSE GUI_APP_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/gui/src/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/assets/fonts/*.c
)

set(GUI_APP_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/gui/include
)
