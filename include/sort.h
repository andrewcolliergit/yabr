#ifndef SORT_H
#define SORT_H

#include <vector>

#include "tasks.h"

namespace yabr::task::sort {
void depth_first(std::vector<Task>& tasks);
std::vector<Task> topo_graph(std::vector<Task> tasks);
int natural_compare(const std::string& a, const std::string& b);
}  // namespace yabr::task::sort
#endif
