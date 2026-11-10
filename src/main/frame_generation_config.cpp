// Settings > Graphics > Frame Generation: RT64's own interpolation (the
// Framerate option), or Lossless Scaling's frame generation from the player's
// own Lossless.dll (lib/lsfg).
//
// Lossless Scaling needs Vulkan, and makes the frames between the game's own,
// so choosing it also picks the Vulkan graphics API (from the next start) and
// the Original framerate (RT64 then makes no frames of its own). Load
// Lossless.dll copies the chosen DLL into the app folder and translates its
// shaders once, so a bad file is caught there and the game starts quickly.
//
// From BattleTanx: Global Assault Recompiled (its src/main/
// frame_generation_config.cpp), which wrote lib/lsfg to carry over to other
// RecompFrontend + RT64 ports.

#include <filesystem>
#include <string>
#include <system_error>

#include "librecomp/config.hpp"
#include "recompui/config.h"
#include "recompui/recompui.h"
#include "util/file.h"
#include "ultramodern/config.hpp"

#include "rt64_frame_generation.h"
#if !defined(__APPLE__)
#include "lsfg/lsfg.hpp"
#endif

namespace {
    const std::string mode_id = "frame_generation";
    const std::string multiplier_id = "lsfg_multiplier";
    const std::string quality_id = "lsfg_quality";

    enum class Mode : uint32_t { RT64, LosslessScaling };
    enum class Quality : uint32_t { Quality, Performance };

    // In the game's own UI: a system message box can open behind the window.
    void show_message(const std::string &header, const std::string &text) {
        recompui::open_info_prompt(header, text, "OK", [] {}, recompui::ButtonStyle::Primary);
    }

    std::filesystem::path dll_path() {
        return recompui::file::get_app_folder_path() / "lossless_scaling" / "Lossless.dll";
    }

    std::filesystem::path cache_path() {
        return recompui::file::get_app_folder_path() / "lossless_scaling" / "shaders.bin";
    }

    struct State {
        Mode mode = Mode::RT64;
        uint32_t multiplier = 2;
        Quality quality = Quality::Quality;
    } state;

    void apply() {
        RT64::FrameGenerationSettings settings;
        settings.enabled = (state.mode == Mode::LosslessScaling);
        settings.generatedFrames = state.multiplier - 1;
        settings.performance = (state.quality == Quality::Performance);
        std::error_code ec;
        if (std::filesystem::exists(dll_path(), ec)) {
            settings.dllPath = dll_path();
            settings.cachePath = cache_path();
        }
        RT64::setFrameGeneration(settings);
    }

    void load_dll() {
        recompui::file::open_file_dialog([](bool success, const std::filesystem::path &path) {
            if (!success) {
                return;
            }
#if defined(__APPLE__)
            (void)(path);
            show_message("Lossless Scaling", "Lossless Scaling frame generation isn't available on macOS.");
#else
            lsfg::ShaderSet shaders;
            std::string error;
            std::error_code ec;
            std::filesystem::create_directories(dll_path().parent_path(), ec);
            std::filesystem::remove(cache_path(), ec);
            // Translate from the chosen file first, so a wrong one doesn't replace a good copy.
            if (!lsfg::load_shaders(path, cache_path(), shaders, error)) {
                show_message("Lossless.dll", "This file can't be used: " + error);
                return;
            }
            if (!std::filesystem::equivalent(path, dll_path(), ec)) {
                std::filesystem::copy_file(path, dll_path(), std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    show_message("Lossless.dll", "Failed to copy Lossless.dll into the app folder.");
                    return;
                }
            }
            apply();
            show_message("Lossless.dll", "Lossless.dll is ready. Set Frame Generation to Lossless Scaling to use it.");
#endif
        });
    }
}

void dinotale_add_frame_generation_options(recomp::config::Config &graphics) {
    graphics.add_enum_option(
        mode_id,
        "Frame Generation",
        "How frames are added above the game's own frame rate. <b>RT64</b> interpolates, at the Framerate set "
        "above. <b>Lossless Scaling</b> uses Lossless Scaling's frame generation (LSFG 3.1): it needs your own "
        "copy of Lossless Scaling's Lossless.dll (press Load Lossless.dll), and the Vulkan graphics API.<br/><br/>"
        "Choosing Lossless Scaling sets the Graphics API to Vulkan (from the next start) and the Framerate to "
        "Original. It adds about one game frame of input delay.",
        { {Mode::RT64, "RT64", "RT64"}, {Mode::LosslessScaling, "LosslessScaling", "Lossless Scaling"} },
        Mode::RT64
    );
    graphics.add_enum_option(
        multiplier_id,
        "Lossless Scaling Multiplier",
        "How many times the game's frame rate Lossless Scaling shows: <b>2x</b> doubles it, <b>3x</b> triples it.",
        { {2u, "2x", "2x"}, {3u, "3x", "3x"}, {4u, "4x", "4x"} },
        2u
    );
    graphics.add_enum_option(
        quality_id,
        "Lossless Scaling Mode",
        "<b>Performance</b> is lighter on the GPU and less accurate with fast motion.",
        { {Quality::Quality, "Quality", "Quality"}, {Quality::Performance, "Performance", "Performance"} },
        Quality::Quality
    );
    graphics.add_option_disable_dependency(multiplier_id, mode_id, Mode::RT64);
    graphics.add_option_disable_dependency(quality_id, mode_id, Mode::RT64);
    // Lossless Scaling makes the frames above the game's: RT64 makes none of its own.
    graphics.add_option_disable_dependency(recompui::config::graphics::options::rr_option, mode_id, Mode::LosslessScaling);
    recompui::config::add_option_button(mode_id, "Load Lossless.dll", load_dll);

    // Also called when the saved settings load.
    graphics.add_option_change_callback(mode_id,
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            state.mode = static_cast<Mode>(std::get<uint32_t>(value));
            if ((state.mode == Mode::LosslessScaling) && (context != recomp::config::OptionChangeContext::Load)) {
                recomp::config::Config &config = recompui::config::get_graphics_config();
                config.update_option_value(recompui::config::graphics::options::api_option,
                    static_cast<uint32_t>(ultramodern::renderer::GraphicsApi::Vulkan));
                config.update_option_value(recompui::config::graphics::options::rr_option,
                    static_cast<uint32_t>(ultramodern::renderer::RefreshRate::Original));
            }
            apply();
        });
    graphics.add_option_change_callback(multiplier_id,
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            state.multiplier = std::get<uint32_t>(value);
            apply();
        });
    graphics.add_option_change_callback(quality_id,
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            state.quality = static_cast<Quality>(std::get<uint32_t>(value));
            apply();
        });
}
