#ifndef CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_PARAMJSONPARSER_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APPMETADATA_INCLUDE_PARAMJSONPARSER_HPP

#include <filesystem>
#include <string>

struct ParsedParamJson {
    std::string title;
    std::string titleId;
};

ParsedParamJson parseParamJson(const std::filesystem::path& paramJsonPath);

#endif
