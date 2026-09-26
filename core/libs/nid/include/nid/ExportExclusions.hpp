#ifndef NID_EXPORTEXCLUSIONS_HPP
#define NID_EXPORTEXCLUSIONS_HPP

#include <string>
#include <unordered_set>

namespace Nid {

std::string NormalizeExportName(const std::string& name);
std::unordered_set<std::string> ReadExportExclusions(const std::string& path);

}

#endif
