#include <Launcher.hpp>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Launcher {

namespace {

std::string Lowercase(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

bool FileExists(const std::filesystem::path& path) {
    std::error_code ec;
    const bool present = std::filesystem::exists(path, ec);
    return !ec && present;
}

std::string ResolveExecutableDir(const char* argv0) {
    std::error_code ec;
    std::filesystem::path resolved;
    if (argv0 != nullptr) resolved = std::filesystem::weakly_canonical(std::filesystem::path(argv0), ec);
    if (ec || resolved.empty()) {
        ec.clear();
        if (argv0 != nullptr) resolved = std::filesystem::absolute(std::filesystem::path(argv0), ec);
    }
    if (ec || resolved.empty()) return std::filesystem::current_path().string();
    return resolved.parent_path().string();
}

std::string ModeName(const Mode mode) {
    switch (mode) {
        case Mode::Ps5: return "ps5";
        case Mode::Switch: return "switch";
        case Mode::Auto: break;
    }
    return "auto";
}

void LogVerbose(const Options& options, const std::string& message) {
    if (options.verbose) std::cerr << "[launcher] " << message << "\n";
}

std::string QuoteArgument(const std::string& value) {
    std::string quoted = "\"";
    for (const char c : value) {
        if (c == '"') quoted += '\\';
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

Mode DetectMode(const std::string& titlePath) {
    const std::filesystem::path path(titlePath);
    const std::string extension = Lowercase(path.extension().string());

    if (extension == ".elf" || extension == ".self" || extension == ".bin") return Mode::Ps5;
    if (extension == ".nsp" || extension == ".xci" || extension == ".nca" || extension == ".nro") return Mode::Switch;

    std::error_code ec;
    const bool isDirectory = std::filesystem::is_directory(path, ec);
    const std::filesystem::path markerDir = (!ec && isDirectory) ? path : path.parent_path();
    if (!markerDir.empty()) {
        if (FileExists(markerDir / "sce_sys" / "param.sfo")) return Mode::Ps5;
        if (FileExists(markerDir / "eboot.bin")) return Mode::Ps5;
    }

    throw std::runtime_error(
        "cannot detect engine mode for \"" + titlePath + "\"; use --mode ps5 or --mode switch");
}

int InvokeEngine(const Options& options, const std::filesystem::path& enginePath, const std::vector<std::string>& arguments) {
    std::string command = QuoteArgument(enginePath.string());
    for (const std::string& argument : arguments) {
        command += ' ';
        command += QuoteArgument(argument);
    }

    LogVerbose(options, "command: " + command);
    const int rawCode = std::system(command.c_str());
    if (rawCode == -1) throw std::runtime_error("failed to spawn engine: " + enginePath.string());

#if defined(_WIN32)
    return rawCode;
#else
    return rawCode >> 8;
#endif
}

std::filesystem::path LocatePs5Engine(const Options& options) {
    if (!options.enginePath.empty()) {
        if (!FileExists(std::filesystem::path(options.enginePath)))
            throw std::runtime_error("engine override not found: " + options.enginePath);
        return std::filesystem::path(options.enginePath);
    }

#if defined(_WIN32)
    const std::filesystem::path sibling = std::filesystem::path(options.executableDir) / "relinker.exe";
#else
    const std::filesystem::path sibling = std::filesystem::path(options.executableDir) / "relinker";
#endif
    if (FileExists(sibling)) return sibling;

    throw std::runtime_error(
        "relinker engine not found at \"" + sibling.string() + "\"; build the relinker target or pass --engine <path>");
}

int RunPs5(const Options& options) {
    const std::filesystem::path enginePath = LocatePs5Engine(options);
    LogVerbose(options, "relinker engine: " + enginePath.string());

    std::error_code ec;
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) throw std::runtime_error("cannot resolve temporary directory: " + ec.message());

    const std::string stem = std::filesystem::path(options.titlePath).stem().string();
    const std::filesystem::path outputPath = tempDir / (stem + ".out");

    std::vector<std::string> arguments;
    arguments.push_back("--input");
    arguments.push_back(options.titlePath);
    arguments.push_back("--output");
    arguments.push_back(outputPath.string());
    arguments.insert(arguments.end(), options.extraArgs.begin(), options.extraArgs.end());

    std::cout << "Mode: PS5; relinker: " << enginePath.string() << '\n';
    const int exitCode = InvokeEngine(options, enginePath, arguments);
    if (exitCode != 0) {
        std::cerr << "relinker failed with exit code " << exitCode << "\n";
        return exitCode;
    }

    std::cout << "OK: relinked image written to " << outputPath.string() << '\n';
    return 0;
}

void PrintSwitchEngineInstructions() {
    std::cerr << "Switch engine (Ryujinx) not found.\n"
              << "Searched, in order:\n"
              << "  1. --engine <path> launcher option\n"
              << "  2. ANYPS5_SWITCH_ENGINE environment variable\n"
              << "  3. <launcher dir>/switch/publish/{osx|linux|win}/Ryujinx[.exe]\n"
              << "To build the Switch engine, run a dotnet build/publish of the switch Ryujinx project so the\n"
              << "artifacts land in switch/publish/<rid>/Ryujinx next to the launcher; see docs/ for details.\n";
}

std::optional<std::filesystem::path> LocateSwitchEngine(const Options& options) {
    if (!options.enginePath.empty()) {
        if (!FileExists(std::filesystem::path(options.enginePath))) {
            std::cerr << "Switch engine override not found: " << options.enginePath << "\n";
            return std::nullopt;
        }
        return std::filesystem::path(options.enginePath);
    }

    const char* envEngine = std::getenv("ANYPS5_SWITCH_ENGINE");
    if (envEngine != nullptr && *envEngine != '\0') {
        if (!FileExists(std::filesystem::path(envEngine))) {
            std::cerr << "ANYPS5_SWITCH_ENGINE points to a missing binary: " << envEngine << "\n";
            return std::nullopt;
        }
        return std::filesystem::path(envEngine);
    }

#if defined(__APPLE__)
    const std::filesystem::path artifact = std::filesystem::path(options.executableDir) / "switch" / "publish" / "osx" / "Ryujinx";
#elif defined(_WIN32)
    const std::filesystem::path artifact = std::filesystem::path(options.executableDir) / "switch" / "publish" / "win" / "Ryujinx.exe";
#else
    const std::filesystem::path artifact = std::filesystem::path(options.executableDir) / "switch" / "publish" / "linux" / "Ryujinx";
#endif
    if (FileExists(artifact)) return artifact;

    return std::nullopt;
}

#if defined(_WIN32)

void CopyOptiScalerFile(const Options& options, const std::filesystem::path& componentDir, const std::filesystem::path& engineDir, const std::string& fileName) {
    std::error_code ec;
    const std::filesystem::path source = componentDir / fileName;
    const std::filesystem::path target = engineDir / fileName;
    std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "OptiScaler: cannot copy " << source.string() << " -> " << target.string() << ": " << ec.message() << "\n";
        return;
    }
    LogVerbose(options, "OptiScaler: copied " + target.string());
}

#endif

void ProcessOptiScaler(const Options& options, const std::filesystem::path& engineDir) {
#if defined(_WIN32)
    if (!options.optiscaler) {
        std::cout << "OptiScaler: pass --optiscaler to stage OptiScaler.ini and nvngx.dll from the optiscaler/ component next to the engine\n";
        return;
    }
    const std::filesystem::path componentDir = engineDir / "optiscaler";
    if (!FileExists(componentDir)) {
        std::cerr << "OptiScaler: component directory not found: " << componentDir.string() << "\n";
        return;
    }
    CopyOptiScalerFile(options, componentDir, engineDir, "OptiScaler.ini");
    CopyOptiScalerFile(options, componentDir, engineDir, "nvngx.dll");
#else
    if (options.optiscaler)
        std::cerr << "OptiScaler: Windows-only component; ignoring --optiscaler (engine dir: " << engineDir.string() << ")\n";
#endif
}

int RunSwitch(const Options& options) {
    const std::optional<std::filesystem::path> enginePath = LocateSwitchEngine(options);
    if (!enginePath.has_value()) {
        PrintSwitchEngineInstructions();
        return 3;
    }

    LogVerbose(options, "switch engine: " + enginePath->string());
    ProcessOptiScaler(options, enginePath->parent_path());

    std::vector<std::string> arguments;
    arguments.push_back(options.titlePath);
    arguments.insert(arguments.end(), options.extraArgs.begin(), options.extraArgs.end());

    std::cout << "Mode: Switch; engine: " << enginePath->string() << '\n';
    return InvokeEngine(options, *enginePath, arguments);
}

void PrintHelp() {
    std::cout <<
        "AnyPS5 OSX Edition - hybrid game launcher (PS5 + Nintendo Switch)\n"
        "\n"
        "Usage:\n"
        "  anyps5-launcher [options] <titlePath> [-- <engine args...>]\n"
        "\n"
        "Options:\n"
        "  --mode auto|ps5|switch  Engine selection; default auto-detects from the title\n"
        "  --engine <path>         Engine binary override (relinker for PS5, Ryujinx for Switch)\n"
        "  --optiscaler            Stage OptiScaler.ini + nvngx.dll from the optiscaler/ component (Windows only)\n"
        "  --verbose               Verbose logging to stderr\n"
        "  -h, --help              Show this help\n"
        "  --                      Everything after -- is passed to the engine verbatim\n"
        "\n"
        "Mode detection:\n"
        "  PS5     .elf, .self or .bin extension; or a folder layout with sce_sys/param.sfo or eboot.bin next to it\n"
        "  Switch  .nsp, .xci, .nca or .nro extension\n"
        "\n"
        "Switch engine search order:\n"
        "  1. --engine <path>\n"
        "  2. ANYPS5_SWITCH_ENGINE environment variable\n"
        "  3. <launcher dir>/switch/publish/{osx|linux|win}/Ryujinx[.exe]\n"
        "\n"
        "Examples:\n"
        "  anyps5-launcher /games/MyGame/eboot.bin\n"
        "  anyps5-launcher --mode switch /games/MyGame/game.nsp -- --profile 1\n"
        "  anyps5-launcher --engine /usr/local/bin/relinker --verbose MyGame.self\n";
}

}

Options ParseArgs(int argc, char* argv[]) {
    Options options;
    options.executableDir = ResolveExecutableDir(argc > 0 ? argv[0] : nullptr);

    bool passThrough = false;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (passThrough) {
            options.extraArgs.push_back(arg);
        } else if (arg == "--") {
            passThrough = true;
        } else if (arg == "-h" || arg == "--help") {
            options.helpRequested = true;
            PrintHelp();
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "--optiscaler") {
            options.optiscaler = true;
        } else if (arg == "--mode") {
            if (i + 1 >= argc)
                throw std::runtime_error("--mode requires a value (auto, ps5 or switch)");
            const std::string value = argv[++i];
            if (value == "auto") options.mode = Mode::Auto;
            else if (value == "ps5") options.mode = Mode::Ps5;
            else if (value == "switch") options.mode = Mode::Switch;
            else throw std::runtime_error("unknown mode: " + value + " (expected auto, ps5 or switch)");
        } else if (arg == "--engine") {
            if (i + 1 >= argc)
                throw std::runtime_error("--engine requires a value");
            options.enginePath = argv[++i];
        } else if (arg.rfind("--", 0) == 0) {
            throw std::runtime_error("unknown option: " + arg + " (use \"--\" to pass arguments through to the engine)");
        } else {
            positional.push_back(arg);
        }
    }

    if (!positional.empty()) {
        options.titlePath = positional.front();
        if (positional.size() > 1)
            throw std::runtime_error("unexpected argument: " + positional[1]);
    }

    if (!options.helpRequested && options.titlePath.empty())
        throw std::runtime_error(
            "Usage: anyps5-launcher [--mode auto|ps5|switch] [--engine <path>] [--optiscaler] [--verbose] [-- <engine args...>] <titlePath>\n"
            "Example: anyps5-launcher /games/MyGame/eboot.bin"
        );

    return options;
}

int Run(const Options& options) {
    LogVerbose(options, "mode: " + ModeName(options.mode));
    LogVerbose(options, "title: " + options.titlePath);
    LogVerbose(options, "executable dir: " + options.executableDir);
    if (!options.enginePath.empty()) LogVerbose(options, "engine override: " + options.enginePath);
    if (!options.extraArgs.empty()) {
        std::string joined;
        for (const std::string& extra : options.extraArgs) {
            if (!joined.empty()) joined += ' ';
            joined += extra;
        }
        LogVerbose(options, "extra args: " + joined);
    }

    if (options.mode == Mode::Auto) {
        const Mode detected = DetectMode(options.titlePath);
        LogVerbose(options, "detected mode: " + ModeName(detected));
        return detected == Mode::Ps5 ? RunPs5(options) : RunSwitch(options);
    }

    return options.mode == Mode::Ps5 ? RunPs5(options) : RunSwitch(options);
}

}
