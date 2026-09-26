#include <stdexcept>
#include <string>
#include <filesystem>
#include <mutex>
#include <cerrno>
#include <cstring>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestHeap.hpp"

namespace {
struct WorkingDirectory {
    std::mutex mutex;
    const std::filesystem::path root = std::filesystem::canonical(std::filesystem::current_path());
    std::filesystem::path current = root;
};
WorkingDirectory& Directories() { static WorkingDirectory state; return state; }
std::filesystem::path Resolve(WorkingDirectory& state, const char* path) {
    std::string text(path);
    for (auto& character : text) if (character == '\\') character = '/';
    std::filesystem::path input(text);
#ifdef _WIN32
    // Preserve the existing ability to pass explicit native drive paths.
    if (input.has_root_name()) return input;
#endif
    auto guest = (std::filesystem::path("/") / state.current.lexically_relative(state.root));
    guest = (input.is_absolute() ? input : guest / input).lexically_normal();
    return (state.root / guest.relative_path()).make_preferred();
}
int DirectoryFailure(const std::error_code& error) {
    if (error == std::errc::permission_denied) return 13;
    if (error == std::errc::not_a_directory) return 20;
    if (error == std::errc::no_such_file_or_directory) return 2;
    if (error == std::errc::filename_too_long) return 63;
    if (error == std::errc::too_many_symbolic_link_levels) return 62;
    return 5;
}
}

extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path) {
    if (!path) { APS5_INVALID_ARG_EX; }
    auto& state = Directories();
    std::lock_guard lock(state.mutex);
    return Resolve(state, path);
}

extern "C" int APS5_VABI chdir_nid_postfix(const char* path) {
    if (!path) { errno = 14; return -1; }
    if (!*path) { errno = 2; return -1; }
    try {
        auto& state = Directories();
        std::lock_guard lock(state.mutex);
        std::error_code error;
        const auto resolved = std::filesystem::canonical(Resolve(state, path), error);
        if (error) { errno = DirectoryFailure(error); return -1; }
        if (!std::filesystem::is_directory(resolved, error)) {
            errno = error ? DirectoryFailure(error) : 20; return -1;
        }
        const auto relative = resolved.lexically_relative(state.root);
        if (relative.empty() || *relative.begin() == "..") { errno = 45; return -1; }
        state.current = resolved;
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) { errno = DirectoryFailure(error.code()); return -1; }
}

extern "C" char* APS5_VABI getcwd_nid_postfix(char* buffer, std::size_t size) {
    if (buffer && size == 0) { errno = 22; return nullptr; }
    try {
        auto& state = Directories();
        std::lock_guard lock(state.mutex);
        std::error_code error;
        if (!std::filesystem::is_directory(state.current, error)) {
            errno = error ? DirectoryFailure(error) : 2; return nullptr;
        }
        const auto relative = state.current.lexically_relative(state.root);
        const auto path = relative == "." ? std::string("/") : "/" + relative.generic_string();
        const auto required = path.size() + 1;
        if ((buffer || size) && size < required) { errno = 34; return nullptr; }
        if (!buffer) buffer = static_cast<char*>(GuestHeap::GuestHeapAllocate_nid_postfix(size ? size : required));
        std::memcpy(buffer, path.c_str(), required);
        return buffer;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
      catch (const std::filesystem::filesystem_error& error) { errno = DirectoryFailure(error.code()); return nullptr; }
}

extern "C" void NotImplemented_nid_no_patch(const char* funcName) {
    throw std::runtime_error(std::string(funcName) + " not implemented");
}
