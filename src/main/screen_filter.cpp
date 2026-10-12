// Settings > Graphics > Screen Filter: post-process filters for the final
// picture, drawn by our RT64 patch (rt64/0017, render/rt64_screen_filter.h).

#include <string>
#include <variant>
#include <vector>

#include "librecomp/config.hpp"
#include "recompui/config.h"
#include "rt64_screen_filter.h"

namespace {
    const std::string screen_filter_option = "screen_filter";

    const std::vector<recomp::config::ConfigOptionEnumOption> screen_filter_options = {
        {RT64::ScreenFilter::None, "None"},
        {RT64::ScreenFilter::CRT, "CRT"},
        {RT64::ScreenFilter::Scanlines, "Scanlines"},
        {RT64::ScreenFilter::LCD, "LCD"},
        {RT64::ScreenFilter::XBR, "xBR", "xBR"},
        {RT64::ScreenFilter::XBRZ, "xBRZ", "xBRZ"},
        {RT64::ScreenFilter::HQ2x, "HQ2x", "HQ2x"},
    };
}

void dinotale_add_screen_filter_option(recomp::config::Config &config) {
    config.add_enum_option(
        screen_filter_option,
        "Screen Filter",
        "A filter for the final picture. "
        "<recomp-color primary>CRT</recomp-color>: a flat CRT TV look, with scanlines, glow and a shadow mask. "
        "<recomp-color primary>Scanlines</recomp-color>: dark lines between the N64's lines. "
        "<recomp-color primary>LCD</recomp-color>: a handheld LCD grid. "
        "<recomp-color primary>xBR</recomp-color>, <recomp-color primary>xBRZ</recomp-color> and <recomp-color primary>HQ2x</recomp-color>: "
        "pixel-art upscalers that smooth the edges of the game's 240p picture.",
        screen_filter_options,
        RT64::ScreenFilter::None
    );
}

// Called every VI: hands the applied setting to the renderer.
void dinotale_update_screen_filter() {
    const auto value = recompui::config::get_config(recompui::config::graphics::id).get_option_value(screen_filter_option);
    if (const uint32_t* filter = std::get_if<uint32_t>(&value)) {
        RT64::setScreenFilter(RT64::ScreenFilter(*filter));
    }
}
