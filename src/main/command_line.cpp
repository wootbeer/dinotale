// Command-line options (DinoTale --help lists them) and portable mode by
// default.
//
// Settings: any option on the General, Graphics and Sound tabs can be set
// with --set <option>=<value>, and the common ones have their own names
// (--aspect expand, --fullscreen, ...). They're applied after the saved
// settings load, the way Apply applies them, and are used for that run only:
// --save-settings also writes them to the settings files. (A setting changed
// on the command line is saved too if Apply is pressed in Settings.)
//
// Starting: --rom <file> picks the ROM (.z64, .n64, .v64, or inside a .zip or
// .7z), as the launcher's file picker does; --play skips the launcher and
// starts the game (the runtime's own --game yoshisstory).
//
// Portable mode: a new install keeps its settings, saves and mods in the
// game's own folder (portable.txt next to the executable). An install that
// already has them in the usual place (%LOCALAPPDATA%\dinotale,
// ~/.config/dinotale) keeps using that; deleting portable.txt switches back.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

#include "librecomp/config.hpp"
#include "librecomp/game.hpp"
#include "recompui/config.h"
#include "util/file.h"

#include "dinotale_command_line.h"

namespace {
    // The tabs whose options the command line can set (the Controls tab holds bindings).
    const std::vector<std::string> setting_tabs = { "general", "graphics", "sound" };

    // Short names for common settings: --<name> <value> is --set <option>=<value>.
    struct Alias {
        const char *name;
        const char *option_id;
        const char *help;
    };

    const std::vector<Alias> aliases = {
        { "resolution", "res_option", "Original, Original2x, Auto" },
        { "aspect", "ar_option", "Original, Stretch, Hybrid, Expand" },
        { "hud", "hr_option", "HUD Placement: Original, Clamp16x9, Expand" },
        { "framerate", "rr_option", "Original, Display, Manual" },
        { "fps", "rr_manual_value", "Manual framerate (sets Framerate to Manual)" },
        { "msaa", "msaa_option", "None, MSAA2X, MSAA4X" },
        { "downsampling", "ds_option", "Downsampling Quality: Off, 2x, 4x" },
        { "api", "api_option", "Graphics API: Auto, D3D12, Vulkan" },
        { "screen-filter", "screen_filter", "None, CRT, Scanlines, LCD, xBR, xBRZ, HQ2x" },
        { "texture-filtering", "texture_filter", "Smooth, Sharp" },
        { "hybrid-dimming", "hybrid_dimming", "0 to 100 (percent)" },
        { "frame-generation", "frame_generation", "RT64, LosslessScaling" },
        { "lsfg-multiplier", "lsfg_multiplier", "2x, 3x, 4x" },
        { "lsfg-mode", "lsfg_quality", "Quality, Performance" },
        { "volume", "main_volume", "0 to 100 (percent)" },
        { "rumble", "rumble_strength", "0 to 100 (percent)" },
        { "deadzone", "joystick_deadzone", "0 to 100 (percent)" },
        { "background-input", "background_input_mode", "On, Off" },
    };

    std::string lowercase(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        return text;
    }

    void print_help() {
        printf(
            "Usage: DinoTale [options]\n"
            "\n"
            "Starting:\n"
            "  --rom <file>                   Pick the ROM (.z64, .n64, .v64, or inside a .zip or .7z), as the launcher does.\n"
            "  --play                         Skip the launcher and start the game (needs a ROM picked, now or before).\n"
            "\n"
            "Settings (for this run; with --save-settings, saved too):\n"
            "  --fullscreen                   Fullscreen.\n"
            "  --windowed                     Windowed.\n");
        for (const Alias &alias : aliases) {
            const std::string flag = std::string("--") + alias.name + " <value>";
            printf("  %-30s %s\n", flag.c_str(), alias.help);
        }
        printf(
            "  --set <option>=<value>         Any setting on the General, Graphics and Sound tabs, by its name\n"
            "                                 in the settings files (--list-settings shows them and their values).\n"
            "  --save-settings                Save the settings given on the command line.\n"
            "\n"
            "Other:\n"
            "  --list-settings                List every setting, its current value and the values it takes.\n"
            "  --help                         Show this.\n"
            "\n"
            "Example: DinoTale --rom \"Yoshi's Story.z64\" --aspect expand --fullscreen --play\n");
    }

    std::string value_text(const recomp::config::ConfigOption &option, const recomp::config::ConfigValueVariant &value) {
        if (const uint32_t *v = std::get_if<uint32_t>(&value)) {
            if (option.type == recomp::config::ConfigOptionType::Enum) {
                const auto &config = std::get<recomp::config::ConfigOptionEnum>(option.variant);
                auto it = config.find_option_from_value(*v);
                if (it != config.options.end()) {
                    return it->key;
                }
            }
            return std::to_string(*v);
        }
        if (const double *v = std::get_if<double>(&value)) {
            char text[32];
            snprintf(text, sizeof(text), "%g", *v);
            return text;
        }
        if (const bool *v = std::get_if<bool>(&value)) {
            return *v ? "true" : "false";
        }
        if (const std::string *v = std::get_if<std::string>(&value)) {
            return *v;
        }
        return "";
    }

    void list_settings() {
        for (const std::string &tab : setting_tabs) {
            recomp::config::Config &config = recompui::config::get_config(tab);
            const recomp::config::ConfigSchema &schema = config.get_config_schema();
            printf("%s:\n", config.name.c_str());
            for (const recomp::config::ConfigOption &option : schema.options) {
                if (option.hidden) {
                    continue;
                }
                std::string values;
                switch (option.type) {
                case recomp::config::ConfigOptionType::Enum:
                    for (const auto &enum_option : std::get<recomp::config::ConfigOptionEnum>(option.variant).options) {
                        values += (values.empty() ? "" : ", ") + enum_option.key;
                    }
                    break;
                case recomp::config::ConfigOptionType::Number: {
                    const auto &number = std::get<recomp::config::ConfigOptionNumber>(option.variant);
                    char text[64];
                    snprintf(text, sizeof(text), "%g to %g", number.min, number.max);
                    values = text;
                    break;
                }
                case recomp::config::ConfigOptionType::Bool:
                    values = "true, false";
                    break;
                default:
                    values = "text";
                    break;
                }
                const std::string name = option.name.empty() ? std::string() : " (" + option.name + ")";
                printf("  %-24s %-16s%s: %s\n", option.id.c_str(), value_text(option, config.get_option_value(option.id)).c_str(),
                    name.c_str(), values.c_str());
            }
        }
    }

    // The option's value from the text given, or monostate (with an error printed) if it doesn't fit.
    recomp::config::ConfigValueVariant parse_value(const recomp::config::ConfigOption &option, const std::string &text) {
        switch (option.type) {
        case recomp::config::ConfigOptionType::Enum: {
            const auto &config = std::get<recomp::config::ConfigOptionEnum>(option.variant);
            // The key (any case), the name shown in Settings, or a key that's the only one containing the text ("2x" for MSAA2X).
            auto it = config.find_option_from_string(text);
            if (it != config.options.end()) {
                return it->value;
            }
            const std::string wanted = lowercase(text);
            for (const auto &enum_option : config.options) {
                if (lowercase(enum_option.name) == wanted) {
                    return enum_option.value;
                }
            }
            const recomp::config::ConfigOptionEnumOption *match = nullptr;
            int matches = 0;
            for (const auto &enum_option : config.options) {
                if (lowercase(enum_option.key).find(wanted) != std::string::npos) {
                    match = &enum_option;
                    matches++;
                }
            }
            if ((matches == 1) && !wanted.empty()) {
                return match->value;
            }
            std::string keys;
            for (const auto &enum_option : config.options) {
                keys += (keys.empty() ? "" : ", ") + enum_option.key;
            }
            fprintf(stderr, "%s: \"%s\" isn't one of %s.\n", option.id.c_str(), text.c_str(), keys.c_str());
            return {};
        }
        case recomp::config::ConfigOptionType::Number: {
            const auto &config = std::get<recomp::config::ConfigOptionNumber>(option.variant);
            std::string number = text;
            if (!number.empty() && (number.back() == '%')) {
                number.pop_back();
            }
            char *end = nullptr;
            double value = std::strtod(number.c_str(), &end);
            if (number.empty() || (end == nullptr) || (*end != '\0')) {
                fprintf(stderr, "%s: \"%s\" isn't a number.\n", option.id.c_str(), text.c_str());
                return {};
            }
            if (config.max > config.min) {
                value = std::clamp(value, config.min, config.max);
            }
            return value;
        }
        case recomp::config::ConfigOptionType::Bool: {
            const std::string value = lowercase(text);
            if ((value == "true") || (value == "on") || (value == "yes") || (value == "1")) {
                return true;
            }
            if ((value == "false") || (value == "off") || (value == "no") || (value == "0")) {
                return false;
            }
            fprintf(stderr, "%s: \"%s\" isn't true or false.\n", option.id.c_str(), text.c_str());
            return {};
        }
        case recomp::config::ConfigOptionType::String:
            return text;
        default:
            return {};
        }
    }

    // Finds the tab holding an option, by its id.
    recomp::config::Config *find_option_tab(const std::string &option_id) {
        for (const std::string &tab : setting_tabs) {
            recomp::config::Config &config = recompui::config::get_config(tab);
            if (config.has_option(option_id)) {
                return &config;
            }
        }
        return nullptr;
    }

    // Sets an option as Apply does. True if it was set.
    bool set_option(const std::string &option_id, const std::string &text, std::vector<recomp::config::Config *> &changed_tabs) {
        recomp::config::Config *config = find_option_tab(option_id);
        if (config == nullptr) {
            fprintf(stderr, "There's no setting \"%s\" (--list-settings shows them).\n", option_id.c_str());
            return false;
        }
        recomp::config::ConfigValueVariant value = parse_value(config->get_option(option_id), text);
        if (std::holds_alternative<std::monostate>(value)) {
            return false;
        }
        config->set_option_value(option_id, value);
        if (config->requires_confirmation) {
            config->apply_option_value(option_id);
        }
        if (std::find(changed_tabs.begin(), changed_tabs.end(), config) == changed_tabs.end()) {
            changed_tabs.push_back(config);
        }
        return true;
    }

    // Shows an error that matters even without a console (Windows release builds have none).
    void show_error(const std::string &text) {
        fprintf(stderr, "%s\n", text.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DinoTale", text.c_str(), nullptr);
    }

    std::string path_text(const std::filesystem::path &path) {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char *>(text.c_str()), text.size());
    }

    std::filesystem::path executable_folder() {
        char *base = SDL_GetBasePath();
        if (base == nullptr) {
            return {};
        }
        std::filesystem::path folder = std::filesystem::path(reinterpret_cast<const char8_t *>(base));
        SDL_free(base);
        return folder;
    }
}

void dinotale_attach_console(int argc) {
#ifdef _WIN32
    // Release builds are GUI programs: print to the console they were started from, if any.
    if ((argc > 1) && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *file = nullptr;
        freopen_s(&file, "CONOUT$", "w", stdout);
        freopen_s(&file, "CONOUT$", "w", stderr);
        printf("\n");
    }
#else
    (void)(argc);
#endif
}

bool dinotale_parse_command_line(int argc, char **argv, DinoTaleCommandLine &command_line) {
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        auto next = [&](std::string &value) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s needs a value.\n", arg.c_str());
                return false;
            }
            value = argv[++i];
            return true;
        };

        std::string value;
        if ((arg == "--help") || (arg == "-h") || (arg == "/?")) {
            print_help();
            return false;
        }
        else if (arg == "--list-settings") {
            command_line.list_settings = true;
        }
        else if (arg == "--rom") {
            if (next(value)) {
                // Relative to where the game was started from, before it moves to its own folder.
                std::error_code ec;
                command_line.rom = std::filesystem::absolute(std::filesystem::path(value), ec);
            }
        }
        else if (arg == "--play") {
            command_line.play = true;
        }
        else if (arg == "--save-settings") {
            command_line.save_settings = true;
        }
        else if (arg == "--fullscreen") {
            command_line.settings.emplace_back(recompui::config::graphics::options::wm_option, "Fullscreen");
        }
        else if (arg == "--windowed") {
            command_line.settings.emplace_back(recompui::config::graphics::options::wm_option, "Windowed");
        }
        else if (arg == "--set") {
            if (next(value)) {
                const size_t equals = value.find('=');
                if ((equals == std::string::npos) || (equals == 0)) {
                    fprintf(stderr, "--set takes <option>=<value>, not \"%s\".\n", value.c_str());
                }
                else {
                    command_line.settings.emplace_back(value.substr(0, equals), value.substr(equals + 1));
                }
            }
        }
        else if ((arg == "--game") || (arg == "--game-mode")) {
            // The runtime's own options.
            command_line.runtime_args.push_back(arg);
            if (next(value)) {
                command_line.runtime_args.push_back(value);
            }
        }
        else {
            // --<alias> <value> or --<alias>=<value>.
            std::string name = (arg.rfind("--", 0) == 0) ? arg.substr(2) : std::string();
            std::string inline_value;
            bool has_inline_value = false;
            const size_t equals = name.find('=');
            if (equals != std::string::npos) {
                inline_value = name.substr(equals + 1);
                name = name.substr(0, equals);
                has_inline_value = true;
            }
            auto alias = std::find_if(aliases.begin(), aliases.end(), [&](const Alias &a) { return name == a.name; });
            if (alias == aliases.end()) {
                fprintf(stderr, "Unknown option \"%s\" (--help lists them).\n", arg.c_str());
                continue;
            }
            if (has_inline_value) {
                value = inline_value;
            }
            else if (!next(value)) {
                continue;
            }
            if (std::strcmp(alias->option_id, recompui::config::graphics::options::rr_manual_value.c_str()) == 0) {
                command_line.settings.emplace_back(recompui::config::graphics::options::rr_option, "Manual");
            }
            command_line.settings.emplace_back(alias->option_id, value);
        }
    }

    if (command_line.play) {
        command_line.runtime_args.push_back("--game");
        command_line.runtime_args.push_back("yoshisstory");
    }
    return true;
}

void dinotale_use_portable_mode_by_default() {
#if !defined(RECOMP_FLATPAK) && !defined(__APPLE__)
    const std::filesystem::path folder = executable_folder();
    if (folder.empty()) {
        return;
    }

    std::error_code ec;
    // Already portable: the app folder is found from the working directory, so use the game's own.
    if (std::filesystem::exists(folder / "portable.txt", ec)) {
        std::filesystem::current_path(folder, ec);
        return;
    }
    if (std::filesystem::exists("portable.txt", ec)) {
        return;
    }
    if (std::getenv("APP_FOLDER_PATH") != nullptr) {
        return;
    }

    // Settings, saves or mods already in the usual place: keep using it.
    const std::filesystem::path usual = recompui::file::get_app_folder_path();
    if (!usual.empty() && std::filesystem::exists(usual, ec) && !std::filesystem::is_empty(usual, ec)) {
        return;
    }

    // A new install: portable, if the game's folder can be written to.
    {
        std::ofstream marker(folder / "portable.txt");
        if (!marker.good()) {
            return;
        }
        marker << "Settings, saves and mods are kept in this folder. Delete this file to keep them in the usual place instead.\n";
    }
    std::filesystem::current_path(folder, ec);
#endif
}

bool dinotale_apply_command_line(const DinoTaleCommandLine &command_line) {
    std::vector<recomp::config::Config *> changed_tabs;
    for (const auto &[option_id, value] : command_line.settings) {
        set_option(option_id, value, changed_tabs);
    }
    for (recomp::config::Config *config : changed_tabs) {
        if (config->id == recompui::config::graphics::id) {
            recompui::config::graphics::apply_config();
        }
        if (command_line.save_settings) {
            config->save_config();
        }
    }

    if (!command_line.rom.empty()) {
        std::error_code ec;
        if (!std::filesystem::exists(command_line.rom, ec)) {
            show_error("--rom: " + path_text(command_line.rom) + " doesn't exist.");
        }
        else {
            switch (recomp::select_rom(command_line.rom, u8"yoshisstory.n64.us")) {
            case recomp::RomValidationError::Good:
                printf("ROM picked: %s\n", path_text(command_line.rom).c_str());
                break;
            case recomp::RomValidationError::FailedToOpen:
                show_error("--rom: couldn't open " + path_text(command_line.rom) + ".");
                break;
            case recomp::RomValidationError::NotARom:
                show_error("--rom: " + path_text(command_line.rom) + " isn't an N64 ROM.");
                break;
            case recomp::RomValidationError::IncorrectRom:
            case recomp::RomValidationError::IncorrectVersion:
            case recomp::RomValidationError::NotYet:
                show_error("--rom: " + path_text(command_line.rom) + " isn't Yoshi's Story (USA).");
                break;
            default:
                show_error("--rom: couldn't use " + path_text(command_line.rom) + ".");
                break;
            }
        }
    }

    if (command_line.list_settings) {
        list_settings();
        return false;
    }
    return true;
}
