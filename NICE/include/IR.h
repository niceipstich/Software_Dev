// C++17 port of the Python connectivity data model.
// Build: g++ -std=c++17 -Wall -Wextra -o design design.cpp

#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

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
    int width = 0;
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

// ---- Functions that operate on the data ----
const std::string& check_name(const std::string& name) {
    if (RESERVED_KEYWORDS.count(name)) {
        throw std::invalid_argument("'" + name +
            "' is a reserved keyword and cannot be used as a name");
    }
    return name;
}

// module <name> { ... } -- 'Design' is the one reserved name a module may take.
Module& module(Modules& modules, const std::string& name) {
    if (name != "Design") {
        check_name(name);
    }
    Module m;
    m.name = name;
    return modules[name] = std::move(m);  // std::map references stay valid
}

Interface& interface(Module& mod, const std::string& name) {
    Interface ifce;
    ifce.name = check_name(name);
    return mod.interfaces[ifce.name] = std::move(ifce);
}

void port(Interface& ifce, const std::string& name, const std::string& direction, int width) {
    ifce.ports.push_back(Port{ check_name(name), direction, width });
}

void connect(Module& mod, const std::string& src, const std::string& dst, ConnectType type) {
    mod.connectivity.push_back(Connect{ src, dst, type });
}

const Interface* find_interface(const Modules& modules, const std::string& name) {
    for (const auto& [mod_name, mod] : modules) {
        auto it = mod.interfaces.find(name);
        if (it != mod.interfaces.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

// Return a list of error strings; empty means valid.
std::vector<std::string> validate(const Modules& modules) {
    std::vector<std::string> errors;
    for (const auto& [mod_name, mod] : modules) {
        for (const Connect& c : mod.connectivity) {
            for (const std::string* ref : { &c.src, &c.dst }) {
                if (find_interface(modules, *ref) == nullptr) {
                    errors.push_back(mod.name + ": connect references unknown interface '" +
                        *ref + "'");
                }
            }
        }
    }
    return errors;
}

// ---- Equivalent of your Tcl ----
int main() {
    Modules modules;

    // module mod1 { Interface ifce1 { port a -direction input -width 4 } }
    Module& mod1 = module(modules, "mod1");
    port(interface(mod1, "ifce1"), "a", "input", 4);

    // module mod2 { Interface ifce2 { port b -direction output -width 4 } }
    Module& mod2 = module(modules, "mod2");
    port(interface(mod2, "ifce2"), "b", "output", 4);

    // module Design { connectivity { connect ifce1 ifce2 -net } }
    Module& design = module(modules, "Design");
    connect(design, "ifce1", "ifce2", ConnectType::Net);

    const auto errors = validate(modules);
    if (errors.empty()) {
        std::cout << "[]\n";
    }
    else {
        for (const auto& e : errors) {
            std::cout << e << "\n";
        }
    }
    return errors.empty() ? 0 : 1;
}