// Settings > Cheats and Settings > Hacks.
//
// Cheats: the game's secrets without earning them (Black and White Yoshi),
// and a few helpers (infinite health, any course on a page, every Trial
// course).
//
// Hacks: GameShark codes. The codes listed come from the community code
// lists; any other code can be typed into Custom Codes. Codes are applied
// once per frame on the game thread (dinotale_cheats_frame, hooked before
// the framebuffer swap in yoshisstory.us.toml), as a GameShark applies them
// every frame. Supported: 80/A0 (8-bit write), 81/A1 (16-bit write), 88/89
// (written when the GameShark button is pressed), D0/D1/D2/D3 (if equal /
// not equal, for the next code), 50 (repeat the next code). DE, EE, F0, F1
// and FF (cartridge and boot codes) need nothing here and are skipped.
//
// A GameShark changes the game's memory, including its machine code. The game
// here is recompiled: its code runs on the PC, not from N64 memory, so codes
// that change code (anything inside the game's own code, 0x80000400 to
// 0x80082000) do nothing. Those are skipped, and Custom Codes says so.

#include <atomic>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <string>
#include <variant>
#include <vector>

#include "librecomp/config.hpp"
#include "recompui/config.h"
#include "recompui/recompui.h"

namespace {
    const std::string cheats_tab_id = "cheats";
    const std::string hacks_tab_id = "hacks";
    const std::string custom_codes_id = "custom_codes";

    // The game's own code in N64 memory: codes that write here change nothing.
    constexpr uint32_t code_start = 0x80000400;
    constexpr uint32_t code_end = 0x80082000;
    constexpr uint32_t rdram_end = 0x80800000;

    struct Code {
        uint32_t address; // With its type in the top byte, as written.
        uint16_t value;
    };

    struct CodeList {
        std::string id;
        std::string name;
        std::string description;
        std::vector<Code> codes;
    };

    // Settings > Cheats. Each is a list of codes too (the addresses are the game's own variables).
    const std::vector<CodeList> cheat_lists = {
        { "cheat_black_white_yoshi", "Black and White Yoshi",
          "Black Yoshi and White Yoshi are always there to pick, without finding their eggs in 2-1 and 3-2 (or "
          "losing them when they lose all their health). They join on the next Select Yoshi screen.",
          // The byte at 0x800F8DF7 holds one bit per Yoshi that's still around: green to pink are the low six
          // (0x3F on a new game), White and Black the top two. The Infinite Lives code sets all eight.
          { {0x010F8DF7, 0x00C0} } },
        { "cheat_infinite_health", "Infinite Health",
          "Every Yoshi's health flower stays full. (The pause-screen code \"hold L, A, B, A, C-Up, Left\" that "
          "lists give for invincibility isn't in the game.)",
          { {0x800F8DB7, 0x0008}, {0x800F8DBB, 0x0008}, {0x800F8DBF, 0x0008}, {0x800F8DC3, 0x0008},
            {0x800F8DC7, 0x0008}, {0x800F8DCB, 0x0008}, {0x800F8DCF, 0x0008}, {0x800F8DD3, 0x0008} } },
        { "cheat_any_course", "Choose Any Course",
          "Story mode lets you pick any of the four courses on each page, not just the ones you've opened.",
          { {0x800F895B, 0x0003} } },
        { "cheat_all_trial_courses", "Open All Trial Courses",
          "Every course is open in Trial mode. The game saves this with your progress, so the courses stay open "
          "after turning it off.",
          { {0x810F8E5C, 0x7FFF}, {0x810F8E5E, 0xFFFF} } },
    };

    // Settings > Hacks: the GameShark codes.
    const std::vector<CodeList> hack_lists = {
        { "gs_infinite_lives", "Infinite Lives (All Yoshis)",
          "800F8DF7 00FF. Every Yoshi stays, Black and White Yoshi included.", { {0x800F8DF7, 0x00FF} } },
        { "gs_health_green", "Infinite Health: Green Yoshi", "800F8DB7 0008", { {0x800F8DB7, 0x0008} } },
        { "gs_health_red", "Infinite Health: Red Yoshi", "800F8DBB 0008", { {0x800F8DBB, 0x0008} } },
        { "gs_health_yellow", "Infinite Health: Yellow Yoshi", "800F8DBF 0008", { {0x800F8DBF, 0x0008} } },
        { "gs_health_dark_blue", "Infinite Health: Dark Blue Yoshi", "800F8DC3 0008", { {0x800F8DC3, 0x0008} } },
        { "gs_health_light_blue", "Infinite Health: Light Blue Yoshi", "800F8DC7 0008", { {0x800F8DC7, 0x0008} } },
        { "gs_health_pink", "Infinite Health: Pink Yoshi", "800F8DCB 0008", { {0x800F8DCB, 0x0008} } },
        { "gs_health_white", "Infinite Health: White Yoshi", "800F8DCF 0008", { {0x800F8DCF, 0x0008} } },
        { "gs_health_black", "Infinite Health: Black Yoshi", "800F8DD3 0008", { {0x800F8DD3, 0x0008} } },
        { "gs_all_fruit", "All Fruit (GameShark Button)",
          "880F89D3 001E. Fills the fruit count (30, the end of a level) when you press the GameShark Button "
          "under Custom Codes.", { {0x880F89D3, 0x001E} } },
        { "gs_no_sound", "No Sound", "801DCB9E 0001", { {0x801DCB9E, 0x0001} } },
        { "gs_low_jump", "Yoshi Can't Jump Very High", "810F8994 0100", { {0x810F8994, 0x0100} } },
        { "gs_main_menu_line", "Small Line Across Screen at Main Menu", "80144892 FFFF", { {0x80144892, 0xFFFF} } },
    };

    std::mutex codes_mutex;
    std::vector<Code> active_codes;          // Applied every frame.
    std::vector<Code> custom_codes;          // Parsed from Custom Codes.
    std::vector<bool> cheat_enabled(cheat_lists.size());
    std::vector<bool> hack_enabled(hack_lists.size());
    std::atomic<bool> gameshark_button = false;

    // Settings > Cheats > Yoshi Colour: 0 is off, else the Yoshi's number + 1. The game's current Yoshi is the
    // byte at 0x800F8DB3 (the Yoshi Modifier code): green, red, yellow, dark blue, light blue, pink, white,
    // black. The Trial-mode trick for a coloured Yoshi in Practice works because Practice keeps whatever is there.
    const std::string yoshi_colour_id = "cheat_yoshi_colour";
    uint32_t yoshi_colour = 0;

    void rebuild_active_codes() {
        std::lock_guard lock(codes_mutex);
        active_codes.clear();
        if (yoshi_colour != 0) {
            active_codes.push_back({ 0x800F8DB3, uint16_t(yoshi_colour - 1) });
        }

        for (size_t i = 0; i < cheat_lists.size(); i++) {
            if (cheat_enabled[i]) {
                active_codes.insert(active_codes.end(), cheat_lists[i].codes.begin(), cheat_lists[i].codes.end());
            }
        }

        for (size_t i = 0; i < hack_lists.size(); i++) {
            if (hack_enabled[i]) {
                active_codes.insert(active_codes.end(), hack_lists[i].codes.begin(), hack_lists[i].codes.end());
            }
        }

        active_codes.insert(active_codes.end(), custom_codes.begin(), custom_codes.end());
    }

    bool is_hex(char c) {
        return std::isxdigit(static_cast<unsigned char>(c)) != 0;
    }

    // Codes are an 8-digit address and a 4-digit value, anywhere in the text: one per line, or separated by
    // commas, semicolons or spaces. Anything else (names, notes) is ignored. "??" can't be used: type the value.
    std::vector<Code> parse_codes(const std::string &text, std::vector<std::string> &skipped) {
        std::vector<Code> codes;
        std::vector<std::string> words;
        std::string word;
        for (char c : text) {
            if (is_hex(c) || (c == '?')) {
                word += c;
            }
            else if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }

        if (!word.empty()) {
            words.push_back(word);
        }

        for (size_t i = 0; i + 1 < words.size(); i++) {
            if ((words[i].size() != 8) || (words[i + 1].size() != 4)) {
                continue;
            }

            if ((words[i].find('?') != std::string::npos) || (words[i + 1].find('?') != std::string::npos)) {
                skipped.push_back(words[i] + " " + words[i + 1] + " (replace ?? with a value)");
                i++;
                continue;
            }

            Code code{ uint32_t(std::stoul(words[i], nullptr, 16)), uint16_t(std::stoul(words[i + 1], nullptr, 16)) };
            const uint8_t type = uint8_t(code.address >> 24);
            const uint32_t address = 0x80000000 | (code.address & 0x00FFFFFF);
            const bool writes = (type == 0x80) || (type == 0x81) || (type == 0xA0) || (type == 0xA1) || (type == 0x88) || (type == 0x89);
            if (writes && (address >= code_start) && (address < code_end)) {
                skipped.push_back(words[i] + " " + words[i + 1] + " (changes the game's code)");
            }

            codes.push_back(code);
            i++;
        }

        return codes;
    }

    uint8_t read8(uint8_t *rdram, uint32_t address) {
        return (address < rdram_end) ? rdram[(address ^ 3) - 0x80000000] : 0;
    }

    uint16_t read16(uint8_t *rdram, uint32_t address) {
        address &= ~1u;
        return (address < rdram_end) ? *reinterpret_cast<uint16_t *>(rdram + ((address ^ 2) - 0x80000000)) : 0;
    }

    void write8(uint8_t *rdram, uint32_t address, uint8_t value) {
        if ((address >= code_end) && (address < rdram_end)) {
            rdram[(address ^ 3) - 0x80000000] = value;
        }
    }

    void write16(uint8_t *rdram, uint32_t address, uint16_t value) {
        address &= ~1u;
        if ((address >= code_end) && (address < rdram_end)) {
            *reinterpret_cast<uint16_t *>(rdram + ((address ^ 2) - 0x80000000)) = value;
        }
    }

    void apply_code(uint8_t *rdram, const Code &code, bool button) {
        const uint8_t type = uint8_t(code.address >> 24);
        const uint32_t address = 0x80000000 | (code.address & 0x00FFFFFF);
        switch (type) {
        case 0x80:
        case 0xA0:
            write8(rdram, address, uint8_t(code.value));
            break;
        // Not a GameShark type: sets bits in a byte (Settings > Cheats).
        case 0x01:
            write8(rdram, address, read8(rdram, address) | uint8_t(code.value));
            break;
        case 0x81:
        case 0xA1:
            write16(rdram, address, code.value);
            break;
        case 0x88:
            if (button) {
                write8(rdram, address, uint8_t(code.value));
            }
            break;
        case 0x89:
            if (button) {
                write16(rdram, address, code.value);
            }
            break;
        default:
            break;
        }
    }

    void apply_codes(uint8_t *rdram, const std::vector<Code> &codes, bool button) {
        for (size_t i = 0; i < codes.size(); i++) {
            const Code &code = codes[i];
            const uint8_t type = uint8_t(code.address >> 24);
            const uint32_t address = 0x80000000 | (code.address & 0x00FFFFFF);
            switch (type) {
            // Conditions: the next code only if the byte or halfword is (D0, D1) or isn't (D2, D3) the value.
            case 0xD0:
            case 0xD1:
            case 0xD2:
            case 0xD3: {
                bool equal = (type & 1) ? (read16(rdram, address) == code.value) : (read8(rdram, address) == uint8_t(code.value));
                bool pass = (type < 0xD2) ? equal : !equal;
                if (!pass) {
                    i++;
                }
                break;
            }
            // Serial: 5000NNSS VVVV then the code; NN times, the address moving by SS and the value by VVVV.
            case 0x50: {
                if (i + 1 >= codes.size()) {
                    break;
                }

                const uint32_t count = (code.address >> 8) & 0xFF;
                const uint32_t step = code.address & 0xFF;
                Code next = codes[++i];
                for (uint32_t n = 0; n < count; n++) {
                    apply_code(rdram, next, button);
                    next.address += step;
                    next.value = uint16_t(next.value + code.value);
                }
                break;
            }
            default:
                apply_code(rdram, code, button);
                break;
            }
        }
    }

    void add_code_lists(recomp::config::Config &config, const std::vector<CodeList> &lists, std::vector<bool> &enabled) {
        for (size_t i = 0; i < lists.size(); i++) {
            config.add_bool_option(lists[i].id, lists[i].name, lists[i].description, false);
            config.add_option_change_callback(lists[i].id,
                [i, &enabled](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
                    enabled[i] = std::get<bool>(value);
                    rebuild_active_codes();
                });
        }
    }
}

void dinotale_add_cheat_tabs() {
    recomp::config::Config &cheats = recompui::config::create_config_tab("Cheats", cheats_tab_id, false);
    add_code_lists(cheats, cheat_lists, cheat_enabled);
    cheats.add_enum_option(yoshi_colour_id, "Yoshi Colour",
        "Play as this Yoshi in every mode. In Practice it's the colour the game only gives you with a trick (pick "
        "the Yoshi in Trial mode, quit the course, then start Practice). In Story and Trial mode it replaces the "
        "Yoshi you pick.",
        {
            {0u, "Off", "Off"}, {1u, "Green", "Green"}, {2u, "Red", "Red"}, {3u, "Yellow", "Yellow"},
            {4u, "DarkBlue", "Dark Blue"}, {5u, "LightBlue", "Light Blue"}, {6u, "Pink", "Pink"},
            {7u, "White", "White"}, {8u, "Black", "Black"},
        },
        0u);
    cheats.add_option_change_callback(yoshi_colour_id,
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext) {
            yoshi_colour = std::get<uint32_t>(value);
            rebuild_active_codes();
        });

    recomp::config::Config &hacks = recompui::config::create_config_tab("Hacks", hacks_tab_id, false);
    add_code_lists(hacks, hack_lists, hack_enabled);
    hacks.add_string_option(custom_codes_id, "Custom Codes",
        "GameShark codes of your own: an 8-digit address and a 4-digit value each, separated by spaces, commas or "
        "semicolons (for example <recomp-color primary>800F895F 0063; 810F8994 0100</recomp-color>). Codes are "
        "applied every frame, as a GameShark does. Codes that change the game's own code (most \"infinite eggs\" "
        "or \"walk through enemies\" codes) can't work in this version and are skipped. 88 and 89 codes are "
        "written when you press the GameShark Button.",
        "");
    hacks.add_option_change_callback(custom_codes_id,
        [](recomp::config::ConfigValueVariant value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            std::vector<std::string> skipped;
            std::vector<Code> codes = parse_codes(std::get<std::string>(value), skipped);
            {
                std::lock_guard lock(codes_mutex);
                custom_codes = std::move(codes);
            }
            rebuild_active_codes();
            if (!skipped.empty() && (context != recomp::config::OptionChangeContext::Load)) {
                std::string text = "These codes can't be used:";
                for (const std::string &code : skipped) {
                    text += "<br/>" + code;
                }
                recompui::open_info_prompt("Custom Codes", text, "OK", [] {}, recompui::ButtonStyle::Primary);
            }
        });
    recompui::config::add_option_button(custom_codes_id, "GameShark Button", [] {
        gameshark_button = true;
    });
}

// Once per frame, on the game thread (see yoshisstory.us.toml).
extern "C" void dinotale_cheats_frame(uint8_t *rdram) {
    const bool button = gameshark_button.exchange(false);
    std::lock_guard lock(codes_mutex);
    if (!active_codes.empty()) {
        apply_codes(rdram, active_codes, button);
    }
}
