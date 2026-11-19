// Command-line options and portable mode by default (src/main/command_line.cpp).

#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

struct DinoTaleCommandLine {
    // <option id, value text>, in the order given.
    std::vector<std::pair<std::string, std::string>> settings;
    bool save_settings = false;
    bool list_settings = false;
    bool play = false;
    std::filesystem::path rom;
    // Passed on to the runtime (recomp::start): --game, --game-mode.
    std::vector<std::string> runtime_args;
};

// Windows release builds have no console of their own: print to the one the game was started from.
void dinotale_attach_console(int argc);

// Before anything else (paths are relative to where the game was started from). False: exit (--help).
bool dinotale_parse_command_line(int argc, char **argv, DinoTaleCommandLine &command_line);

// After the program id is set and before the app folder is used: a new install becomes portable.
void dinotale_use_portable_mode_by_default();

// After the settings tabs are finalized and the game registered. False: exit (--list-settings).
bool dinotale_apply_command_line(const DinoTaleCommandLine &command_line);
