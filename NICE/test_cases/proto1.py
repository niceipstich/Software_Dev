from dataclasses import dataclass, field
from enum import Enum

# ---- Reserved keywords (add the rest here) ----
RESERVED_KEYWORDS = {
    "module",
    "Design",
}


class ConnectType(Enum):
    NET = "net"
    EXPOSE = "expose"
    Module = "module"
    Design = "design"
    Port = "port"
    Interface = "interface"
    InterfaceNet = "interface_net"


class Port:
    name: str
    direction: str  # "input" or "output"
    width: int

class PortNet:
    name: str
    ports: list[Port] = field(default_factory=list)


class Interface:
    name: str
    ports: list[Port] = field(default_factory=list)

class InterfaceNet:
    name: str
    ports: list[Port] = field(default_factory=list)

class Connect:
    src: str  # interface name
    dst: str  # interface name
    type: ConnectType


@dataclass
class Module:
    name: str
    con_name: str
    interfaces: dict[str, Interface] = field(default_factory=dict)
    connectivity: list[Connect] = field(default_factory=list)  # used by Design

@dataclass
class Design(Module):
    name: str
    interfaces: dict[str, Interface] = field(default_factory=dict)
    connectivity: list[Connect] = field(default_factory=list)  # used by Design

# ---- Functions that operate on the data ----
def check_name(name):
    if name in RESERVED_KEYWORDS:
        raise ValueError(f"'{name}' is a reserved keyword and cannot be used as a name")
    return name


def module(modules, name):
    """module <name> { ... } -- 'Design' is the one reserved name a module may take."""
    if name != "Design":
        check_name(name)
    modules[name] = Module(name)
    return modules[name]


def interface(mod, name):
    ifce = Interface(check_name(name))
    mod.interfaces[ifce.name] = ifce
    return ifce


def port(ifce, name, *, direction, width):
    ifce.ports.append(Port(check_name(name), direction, width))


def connect(mod, src, dst, *, type):
    mod.connectivity.append(Connect(src, dst, type))


def find_interface(modules, name):
    for mod in modules.values():
        if name in mod.interfaces:
            return mod.interfaces[name]
    return None


def validate(modules):
    """Return a list of error strings; empty means valid."""
    errors = []
    for mod in modules.values():
        for c in mod.connectivity:
            for ref in (c.src, c.dst):
                if find_interface(modules, ref) is None:
                    errors.append(f"{mod.name}: connect references unknown interface '{ref}'")
    return errors


# ---- Equivalent of your Tcl ----
modules = {}

# module mod1 { Interface ifce1 { port a -direction input -width 4 } }
mod1 = module(modules, "mod1")
port(interface(mod1, "ifce1"), "a", direction="input", width=4)

# module mod2 { Interface ifce2 { port b -direction output -width 4 } }
mod2 = module(modules, "mod2")
port(interface(mod2, "ifce2"), "b", direction="output", width=4)

# module Design { connectivity { connect ifce1 ifce2 -net } }
design = module(modules, "Design")
connect(design, "ifce1", "ifce2", type=ConnectType.NET)

print(validate(modules))  # []