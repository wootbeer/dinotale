// DinoTale's own settings in Settings > Graphics, applied by our RT64 patches
// (render/rt64_screen_filter.h):
// - Screen Filter: post-process filters for the final picture (rt64/0017).
// - Hybrid Dimming: how dark Hybrid's stretched sides are (rt64/0015, 0018).
// - Texture Filtering: Sharp samples textures without filtering (rt64/0024).

#include <string>
#include <variant>
#include <vector>

#include "librecomp/config.hpp"
#include "recompui/config.h"
#include "ultramodern/config.hpp"
#include "rt64_screen_filter.h"

namespace {
    const std::string screen_filter_option = "screen_filter";
    const std::string hybrid_dimming_option = "hybrid_dimming";
    const std::string texture_filter_option = "texture_filter";
    // Percent the sides are darkened by; the default was chosen by eye.
    constexpr double default_hybrid_dimming = 88.0;

    const std::vector<recomp::config::ConfigOptionEnumOption> screen_filter_options = {
        {RT64::ScreenFilter::None, "None"},
        {RT64::ScreenFilter::CRT, "CRT"},
        {RT64::ScreenFilter::Scanlines, "Scanlines"},
        {RT64::ScreenFilter::LCD, "LCD"},
        {RT64::ScreenFilter::XBR, "xBR", "xBR"},
        {RT64::ScreenFilter::XBRZ, "xBRZ", "xBRZ"},
        {RT64::ScreenFilter::HQ2x, "HQ2x", "HQ2x"},
    };

    const std::vector<recomp::config::ConfigOptionEnumOption> texture_filter_options = {
        {0, "Smooth", "Smooth"},
        {1, "Sharp", "Sharp"},
    };
}

void dinotale_add_display_options(recomp::config::Config &config) {
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

    config.add_enum_option(
        texture_filter_option,
        "Texture Filtering",
        "<recomp-color primary>Smooth</recomp-color> is the game's own filtering. "
        "<recomp-color primary>Sharp</recomp-color> draws every texture (Yoshi, enemies, the HUD, backgrounds) with sharp pixels.",
        texture_filter_options,
        0
    );

    config.add_percent_number_option(
        hybrid_dimming_option,
        "Hybrid Dimming",
        "How much the stretched sides are darkened with the <recomp-color primary>Hybrid</recomp-color> aspect ratio, "
        "so they fill the window without drawing the eye. <b>0%</b> leaves them as bright as the rest of the picture.",
        default_hybrid_dimming
    );
    config.add_option_disable_dependency(
        hybrid_dimming_option,
        recompui::config::graphics::options::ar_option,
        ultramodern::renderer::AspectRatio::Original,
        ultramodern::renderer::AspectRatio::Manual,
        ultramodern::renderer::AspectRatio::Stretch,
        ultramodern::renderer::AspectRatio::Expand
    );
}

// Called every VI: hands the applied settings to the renderer.
void dinotale_update_display_options() {
    recomp::config::Config &config = recompui::config::get_config(recompui::config::graphics::id);
    const auto filter = config.get_option_value(screen_filter_option);
    if (const uint32_t* value = std::get_if<uint32_t>(&filter)) {
        RT64::setScreenFilter(RT64::ScreenFilter(*value));
    }

    const auto texture_filter = config.get_option_value(texture_filter_option);
    if (const uint32_t* value = std::get_if<uint32_t>(&texture_filter)) {
        RT64::setSharpTextures(*value == 1);
    }

    const auto dimming = config.get_option_value(hybrid_dimming_option);
    if (const double* value = std::get_if<double>(&dimming)) {
        RT64::setHybridSideBrightness(float(1.0 - *value / 100.0));
    }
}
