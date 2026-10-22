/**
 ******************************************************************************
 * @file    GuiApp.cpp
 * @brief   GUI entry: una_lvgl_app_init() is called by the SDK's LVGL entry
 *          point once LVGL and the display are up. The model and UI live in
 *          static storage for the life of the process.
 ******************************************************************************
 */

#include <new>

#include "gui/Assets.hpp"
#include "gui/Ui.hpp"
#include "gui/model/Model.hpp"

#include "SDK/GUI/LVGL/Draw.hpp"

namespace
{
alignas(Model) uint8_t sModelStorage[sizeof(Model)];
alignas(Ui)    uint8_t sUiStorage[sizeof(Ui)];
} // namespace

/// LVGL's default font; every label sets its own, so this only backs strays.
extern "C" const lv_font_t* una_lvgl_default_font(void)
{
    return &poppins_regular_15;
}

extern "C" void una_lvgl_app_init(void)
{
    SDK::LVGL::Draw::init();
    Model* model = new (sModelStorage) Model();
    new (sUiStorage) Ui(*model);
}
