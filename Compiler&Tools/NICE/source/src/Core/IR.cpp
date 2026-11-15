/ module <name> { ... } -- 'Design' is the one reserved name a module may take.
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