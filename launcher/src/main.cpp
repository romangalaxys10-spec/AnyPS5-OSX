#include <Launcher.hpp>
#include <iostream>

int main(const int argc, char* argv[]) {
    Launcher::Options options;
    try {
        options = Launcher::ParseArgs(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 2;
    }

    if (options.helpRequested) return 0;

    try {
        return Launcher::Run(options);
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 2;
    }
}
