#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <chrono>
extern "C" {
FileStream* APS5_VABI freopen_nid_postfix(const char*, const char*, FileStream*);
int APS5_VABI fseeko_nid_postfix(FileStream*, std::int64_t, int);
std::int64_t APS5_VABI ftello_nid_postfix(FileStream*);
int APS5_VABI fseek_nid_postfix(FileStream*, std::int64_t, int);
std::int64_t APS5_VABI ftell_nid_postfix(FileStream*);
int* APS5_VABI __error_nid_postfix();
extern FileStream* __stdinp_nid_postfix;
extern FileStream* __stdoutp_nid_postfix;
extern FileStream* __stderrp_nid_postfix;
extern int __isthreaded_nid_postfix;
int APS5_VABI fprintf_nid_postfix(FileStream*, const char*, ...);
int APS5_VABI vfprintf_nid_postfix(FileStream*, const char*, void*);
int APS5_VABI vsprintf_nid_postfix(char*, const char*, void*);
int APS5_VABI fgetc_nid_postfix(FileStream*);
int APS5_VABI fputc_nid_postfix(int, FileStream*);
int APS5_VABI __srget_nid_postfix(FileStream*);
int APS5_VABI __swbuf_nid_postfix(int, FileStream*);
int APS5_VABI ungetc_nid_postfix(int, FileStream*);
char* APS5_VABI fgets_nid_postfix(char*, int, FileStream*);
int APS5_VABI feof_nid_postfix(FileStream*);
int APS5_VABI fileno_nid_postfix(FileStream*);
void APS5_VABI clearerr_nid_postfix(FileStream*);
int APS5_VABI setvbuf_nid_postfix(FileStream*, char*, int, std::size_t);
}
static void Require(bool value) { if (!value) std::abort(); }
static int APS5_VABI WriteFormatted(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfprintf_nid_postfix(stream, format, args);
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}
static int APS5_VABI FormatString(char* buffer, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vsprintf_nid_postfix(buffer, format, args);
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}
int main() {
    char stringOutput[256];
    std::memset(stringOutput, '!', sizeof(stringOutput));
    std::int64_t count = -1;
    const char expectedString[] = "guest:4294967297:  3.50:1,2,3,4,5,6,7,8:%";
    const int written = FormatString(stringOutput, "%s:%ld:%*.*f:%d,%d,%d,%d,%d,%d,%d,%d:%%%ln",
        "guest", std::int64_t{4294967297}, 6, 2, 3.5, 1, 2, 3, 4, 5, 6, 7, 8, &count);
    Require(written == sizeof(expectedString) - 1 && count == written);
    Require(std::strcmp(stringOutput, expectedString) == 0 && stringOutput[written + 1] == '!');
    Require(FormatString(stringOutput, "%.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.2Lf",
        1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 1.25L) == 25);
    Require(std::strcmp(stringOutput, "1 2 3 4 5 6 7 8 9 10 1.25") == 0);
    Require(FormatString(stringOutput, "") == 0 && stringOutput[0] == '\0');
    Require(__isthreaded_nid_postfix == 1);
    Require(__stdoutp_nid_postfix == &_Stdout_nid_postfix);
    Require(__stderrp_nid_postfix == &_Stderr_nid_postfix);
    Require(fileno_nid_postfix(__stdinp_nid_postfix) == 0);
    Require(fileno_nid_postfix(__stdoutp_nid_postfix) == 1);
    Require(fileno_nid_postfix(__stderrp_nid_postfix) == 2);
    FileStream stream(std::tmpfile());
    auto& guest = *reinterpret_cast<GuestFilePrefix*>(&stream);
    Require(&guest == &stream.GuestState());
    Require(guest.position == nullptr && guest.readRemaining == 0 && guest.writeRemaining == 0);
    Require(guest.descriptor == fileno_nid_postfix(&stream));
    Require(setvbuf_nid_postfix(&stream, nullptr, 2, 0) == 0);
    Require(fputc_nid_postfix('A', &stream) == 'A');
    Require(--guest.writeRemaining < 0 && __swbuf_nid_postfix('\n', &stream) == '\n');
    std::rewind(stream.GetHandle());
    Require(--guest.readRemaining < 0 && __srget_nid_postfix(&stream) == 'A');
    Require(ungetc_nid_postfix('B', &stream) == 'B');
    char text[8]{};
    Require(fgets_nid_postfix(text, sizeof(text), &stream) == text);
    Require(std::strcmp(text, "B\n") == 0);
    Require(fgetc_nid_postfix(&stream) == EOF);
    Require(feof_nid_postfix(&stream) && (guest.flags & 0x20));
    clearerr_nid_postfix(&stream);
    Require(!feof_nid_postfix(&stream) && !(guest.flags & 0x20));
    stream.Close();
    Require(guest.flags == 0 && guest.descriptor == -1);

    FileStream formatted(std::tmpfile());
    const char expected[] = "guest 4294967297 1.25 1 2 3 4 5 6 7 8\n";
    Require(fprintf_nid_postfix(&formatted, "%s %ld %.2f %d %d %d %d %d %d %d %d\n",
        "guest", std::int64_t{4294967297}, 1.25, 1, 2, 3, 4, 5, 6, 7, 8) == sizeof(expected) - 1);
    Require(WriteFormatted(&formatted, "%*.*f:%s", 6, 2, 3.5, "end") == 10);
    std::rewind(formatted.GetHandle());
    char output[128]{};
    Require(fgets_nid_postfix(output, sizeof(output), &formatted) == output);
    Require(std::strcmp(output, expected) == 0);
    Require(fgets_nid_postfix(output, sizeof(output), &formatted) == output);
    Require(std::strcmp(output, "  3.50:end") == 0);
    formatted.Close();

    FileStream positioned(std::tmpfile());
    constexpr std::int64_t largeOffset = INT64_C(4294967313);
    Require(fseeko_nid_postfix(&positioned, largeOffset, SEEK_SET) == 0);
    Require(ftello_nid_postfix(&positioned) == largeOffset);
    Require(ftell_nid_postfix(&positioned) == largeOffset);
    Require(fseek_nid_postfix(&positioned, -9, SEEK_CUR) == 0);
    Require(ftello_nid_postfix(&positioned) == largeOffset - 9);
    Require(fseek_nid_postfix(&positioned, largeOffset, SEEK_SET) == 0);
    Require(ftell_nid_postfix(&positioned) == largeOffset);
    Require(fseeko_nid_postfix(&positioned, 0, 12345) == -1 && *__error_nid_postfix() == 22);
    Require(ftello_nid_postfix(&positioned) == largeOffset);
    Require(fseeko_nid_postfix(&positioned, 0, SEEK_END) == 0);
    Require(ftello_nid_postfix(&positioned) == 0); // Seeking alone did not extend the file.
    Require(fgetc_nid_postfix(&positioned) == EOF && feof_nid_postfix(&positioned));
    Require(fseeko_nid_postfix(&positioned, 0, SEEK_SET) == 0);
    Require(!feof_nid_postfix(&positioned));
    positioned.Close();

    const auto filename = "anyps5-reopen-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    FileStream redirected(std::tmpfile());
    Require(fgetc_nid_postfix(&redirected) == EOF && feof_nid_postfix(&redirected));
    Require(freopen_nid_postfix(filename.c_str(), "w+b", &redirected) == &redirected);
    Require(!feof_nid_postfix(&redirected));
    Require(fputc_nid_postfix('R', &redirected) == 'R');
    Require(fseeko_nid_postfix(&redirected, 0, SEEK_SET) == 0);
    Require(fgetc_nid_postfix(&redirected) == 'R');
    Require(freopen_nid_postfix(filename.c_str(), "ab", &redirected) == &redirected);
    Require(fputc_nid_postfix('S', &redirected) == 'S');
    Require(freopen_nid_postfix(nullptr, "r", &redirected) == nullptr && *__error_nid_postfix() == 45);
    redirected.Close();
    { std::ifstream input(filename, std::ios::binary); std::string contents; std::getline(input, contents);
      Require(contents == "RS"); }
    Require(std::filesystem::remove(filename));
    FileStream failed(std::tmpfile());
    Require(freopen_nid_postfix(filename.c_str(), "rb", &failed) == nullptr);
    Require(*__error_nid_postfix() == 2);
    Require(failed.GuestState().flags == 0 && failed.GuestState().descriptor == -1);
}
