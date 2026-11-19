int main() {
    Modules mods;

    Module mod1;
    mod1.name = "mod1";
    Interface ifce1;
    ifce1.name = "ifce1";
    ifce1.ports = {make_port("a", Direction::Input, 4), make_port("b", Direction::Output, 4)};
    mod1.interfaces.push_back(ifce1);
    mods.add(mod1);

    Module mod2;
    mod2.name = "mod2";
    Interface ifce2;
    ifce2.name = "ifce2";
    ifce2.ports = {make_port("b", Direction::Output, 4), make_port("a", Direction::Input, 4)};
    Interface ifce3;
    ifce3.name = "ifce3";
    ifce3.ports = {make_port("c", Direction::Output, 4)};
    ifce3.ports[0].default_value = 0;
    mod2.interfaces = {ifce2, ifce3};
    mods.add(mod2);

    Module design;
    design.name = TOP_MODULE_NAME;
    design.interfaces.push_back(ifce3);
    design.net_names.push_back({"mod1.ifce1.a", "mod2.ifce2.AWWR", {}});

    Instance a;
    a.name = "A";
    a.module = "mod1";
    a.connectivity.push_back({"ifce1", "ifce2", ConnectKind::Net, {}});
    Instance b;
    b.name = "B";
    b.module = "mod2";
    b.connectivity.push_back({"ifce1", "ifce2", ConnectKind::Net, {}});
    b.connectivity.push_back({"ifce3", "", ConnectKind::Expose, {}});
    design.instances = {a, b};
    mods.add(design);

    // A second "mod1" must be rejected.
    Module dup;
    dup.name = "mod1";
    std::cout << "duplicate module rejected: " << (mods.add(dup) == nullptr ? "yes" : "NO") << "\n\n";

    for (const auto& m : mods.all()) {
        std::cout << "module " << m.name << "\n";
        for (const auto& i : m.interfaces) {
            std::cout << "  interface " << i.name << "\n";
            for (const auto& p : i.ports)
                std::cout << "    port " << p.name << " "
                          << (p.direction == Direction::Input ? "input" : "output")
                          << " width " << p.width
                          << (p.default_value ? " (has default)" : "") << "\n";
        }
        for (const auto& n : m.net_names)
            std::cout << "  netName " << n.port << " -> " << n.netname << "\n";
        for (const auto& inst : m.instances) {
            std::cout << "  instance " << inst.module << " " << inst.name << "\n";
            for (const auto& c : inst.connectivity)
                std::cout << "    connect " << c.src
                          << (c.kind == ConnectKind::Net ? " " + c.dst + " -net" : " -expose_as")
                          << "\n";
        }
    }
    return 0;
}