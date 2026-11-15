// C++17 port of the Python connectivity data model.
// Build: g++ -std=c++17 -Wall -Wextra -o design design.cpp

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

typedef int UNEVALUATED_WIDTH = -1;  // used to indicate that a port width has not been evaluated yet

// ---- Reserved keywords (add the rest here) ----
static const std::set<std::string> RESERVED_KEYWORDS = {
    "module",
    "Design",
};

enum class ConnectType {
    Net,
    Expose,
    Module,
    Design,
    Port,
    Interface,
    InterfaceNet,
};

class Port {
    std::string name;
    std::string direction;  // "input" or "output"
    int width = UNEVALUATED_WIDTH;
};

class PortNet {
    std::string name;
    std::vector<Port> ports;
};

class Interface {
    std::string name;
    std::vector<Port> ports;
};

class InterfaceNet {
    std::string name;
    std::vector<Port> ports;
};

class Connect {
    std::string src;  // interface name
    std::string dst;  // interface name
    ConnectType type;
};

// 'Design' is just a Module named "Design" (as in the Python, where
// module() builds a plain Module for it), so no separate subclass is needed.
class Module {
    std::string name;
    std::string con_name;
    std::map<std::string, Interface> interfaces;
    std::vector<Connect> connectivity;  // used by Design
};

using Modules = std::map<std::string, Module>;

