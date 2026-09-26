#ifndef LAUNCHER_SRC_LAUNCHER_HPP
#define LAUNCHER_SRC_LAUNCHER_HPP

#include <string>
#include <vector>

namespace Launcher {

enum class Mode {
    Auto,
    Ps5,
    Switch
};

struct Options {
    Mode mode = Mode::Auto;
    std::string titlePath;
    std::string enginePath;
    std::vector<std::string> extraArgs;
    bool verbose = false;
    bool optiscaler = false;
    bool helpRequested = false;
    std::string executableDir;
};

Options ParseArgs(int argc, char* argv[]);

int Run(const Options& options);

}

#endif
