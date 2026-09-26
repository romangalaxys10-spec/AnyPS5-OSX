#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstdio>
extern "C" {
int APS5_VABI remove_nid_postfix(const char*);
int APS5_VABI rename_nid_postfix(const char*, const char*);
int* APS5_VABI __error_nid_postfix();
}
static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Filesystem check failed at line %d (guest errno %d)\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)
int main() {
    const auto root = std::filesystem::path("anyps5-filesystem-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(root));
    const auto file = root / "file.txt";
    { std::ofstream stream(file); stream << "retained until removal"; }
    Require(remove_nid_postfix(root.string().c_str()) == -1);
    Require(*__error_nid_postfix() == 66);
    Require(std::filesystem::is_regular_file(file));
    Require(remove_nid_postfix((file / "invalid").string().c_str()) == -1);
    Require(remove_nid_postfix("") == -1 && *__error_nid_postfix() == 2);
    Require(remove_nid_postfix(nullptr) == -1 && *__error_nid_postfix() == 14);
    const auto renamed = root / "renamed.txt";
    { std::ofstream stream(renamed); stream << "old contents"; }
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == 0);
    Require(!std::filesystem::exists(file));
    { std::ifstream stream(renamed); std::string contents; std::getline(stream, contents);
      Require(contents == "retained until removal"); }
    Require(rename_nid_postfix(renamed.string().c_str(), renamed.string().c_str()) == 0);
    Require(rename_nid_postfix(file.string().c_str(), renamed.string().c_str()) == -1);
    Require(*__error_nid_postfix() == 2);
    Require(rename_nid_postfix(renamed.string().c_str(), file.string().c_str()) == 0);
    Require(remove_nid_postfix(file.string().c_str()) == 0);
    Require(!std::filesystem::exists(file));
    Require(remove_nid_postfix(file.string().c_str()) == -1 && *__error_nid_postfix() == 2);
    Require(remove_nid_postfix(root.string().c_str()) == 0);
    Require(!std::filesystem::exists(root));
}
