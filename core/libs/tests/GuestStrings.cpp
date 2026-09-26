#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>

extern "C" {
char* APS5_VABI basename_nid_postfix(const char*);
int* APS5_VABI __error_nid_postfix();
std::size_t APS5_VABI strnlen_nid_postfix(const char*, std::size_t);
char* APS5_VABI strncat_nid_postfix(char*, const char*, std::size_t);
char* APS5_VABI strpbrk_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strcspn_nid_postfix(const char*, const char*);
std::size_t APS5_VABI strlcat_nid_postfix(char*, const char*, std::size_t);
char* APS5_VABI strtok_r_nid_postfix(char*, const char*, char**);
char* APS5_VABI strtok_nid_postfix(char*, const char*);
char* APS5_VABI strcasestr_nid_postfix(const char*, const char*);
}

static void Require(bool condition) {
    if (!condition) {
        std::fputs("Guest string check failed\n", stderr);
        std::abort();
    }
}

int main() {
    Require(std::strcmp(basename_nid_postfix(nullptr), ".") == 0);
    Require(std::strcmp(basename_nid_postfix(""), ".") == 0);
    Require(std::strcmp(basename_nid_postfix("////"), "/") == 0);
    const char path[] = "/one/two///";
    Require(std::strcmp(basename_nid_postfix(path), "two") == 0);
    Require(std::strcmp(path, "/one/two///") == 0);
    Require(std::strcmp(basename_nid_postfix("one\\two"), "one\\two") == 0);
    const std::string longName(1024, 'x');
    Require(basename_nid_postfix(longName.c_str()) == nullptr && *__error_nid_postfix() == 63);
    const char bounded[] = {'a', 'b', 'c'};
    Require(strnlen_nid_postfix(bounded, 0) == 0);
    Require(strnlen_nid_postfix(bounded, sizeof(bounded)) == 3);
    Require(strnlen_nid_postfix("a", 8) == 1);
    char truncated[] = "abXX";
    Require(strlcat_nid_postfix(truncated, "cd", 2) == 4);
    Require(std::strcmp(truncated, "abXX") == 0);
    char buffer[8] = "ab";
    Require(strlcat_nid_postfix(buffer, "cdefgh", sizeof(buffer)) == 8);
    Require(std::strcmp(buffer, "abcdefg") == 0);
    Require(strlcat_nid_postfix(buffer, "xyz", 0) == 3);
    buffer[0] = '\0';
    Require(strlcat_nid_postfix(buffer, "x", 1) == 1 && buffer[0] == '\0');
    Require(strncat_nid_postfix(buffer, "xyz", 2) == buffer);
    Require(std::strcmp(buffer, "xy") == 0);
    Require(strpbrk_nid_postfix(buffer, "ay") == buffer + 1);
    Require(strpbrk_nid_postfix(buffer, "") == nullptr);
    Require(strcspn_nid_postfix(buffer, "y") == 1);
    char first[] = ",a,,b,";
    char second[] = "x:y";
    char* firstState = nullptr;
    char* secondState = nullptr;
    Require(std::strcmp(strtok_r_nid_postfix(first, ",", &firstState), "a") == 0);
    Require(std::strcmp(strtok_r_nid_postfix(second, ":", &secondState), "x") == 0);
    Require(std::strcmp(strtok_r_nid_postfix(nullptr, ",", &firstState), "b") == 0);
    Require(strtok_r_nid_postfix(nullptr, ",", &firstState) == nullptr);
    Require(strtok_r_nid_postfix(nullptr, ",", &firstState) == nullptr);
    Require(std::strcmp(strtok_r_nid_postfix(nullptr, "", &secondState), "y") == 0);
    char hostTokens[] = "host:next";
    Require(std::strcmp(std::strtok(hostTokens, ":"), "host") == 0);
    char guestTokens[] = ",one,,two:three";
    Require(std::strcmp(strtok_nid_postfix(guestTokens, ","), "one") == 0);
    Require(std::strcmp(strtok_nid_postfix(nullptr, ":,"), "two") == 0);
    Require(std::strcmp(strtok_nid_postfix(nullptr, ""), "three") == 0);
    Require(strtok_nid_postfix(nullptr, ",") == nullptr);
    Require(strtok_nid_postfix(nullptr, ",") == nullptr);
    Require(std::strcmp(std::strtok(nullptr, ":"), "next") == 0);
    const char text[] = "aABAbC";
    Require(strcasestr_nid_postfix(text, "ababc") == text + 1);
    Require(strcasestr_nid_postfix(text, "") == text);
    Require(strcasestr_nid_postfix(text, "abcdef") == nullptr);
    Require(strcasestr_nid_postfix("", "a") == nullptr);
    const char highBytes[] = {static_cast<char>(0xff), 'A', 0};
    Require(strcasestr_nid_postfix(highBytes, "a") == highBytes + 1);
}
