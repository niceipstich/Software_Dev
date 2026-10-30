#An RTL module, which is a collection of functions, globals, and structs
# This module will have only ports information
from enum import Enum

class PortDirection(Enum):
    INPUT = "input"
    OUTPUT = "output"
    INOUT = "inout" # currently not supported

class Module():
    def __init__(self, name):
        self.name = name
        self.instances = []
        self.ports = []

# This module will have instance ports and full connectivity information
class Design(Module):
    def __init__(self, name):
        super().__init__(name)
        self.connectivity = []
    isDesign = True
    connectivity = []  # List of tuples (source_port, destination_port) representing connections
    a = -1
    b = -1
    connection_map = {"a": a , "b": b } 

#An object of a class Modle.
class Instance():
    def __init__(self, name, type):
        self.name = name
        self.type = type

# A boundary port for a module, which can be connected to other ports or nets.
class Port():
    def __init__(self, name, type):
        self.name = name
        self.type = type
        irection = None  # Direction can be 'input', 'output', or 'inout'

class PortNet():
    def __init__(self, name, type):
        self.name = name
        self.type = type                            

class Interface():
    def __init__(self, name):
        self.name = name
        self.ports = []
        self.portnets = []

class InterfaceNet():
    def __init__(self, name, type):
        self.name = name
        self.type = type

class Spec():
    _count = 0
    MAX_SPECS = 2

    def __init__(self, name):
        if Spec._count >= Spec.MAX_SPECS:
            raise RuntimeError(f"Only {Spec.MAX_SPECS} Spec objects are allowed")
        Spec._count += 1

        self.name = name
        self.modules = []
        self.interfaces = []
        self.interface_nets = []

    def add_module(self, name):
        module = Module(name)
        self.modules.append(module)
        return module
    
def make_module(spec, name):
    module = Module(name)
    spec.modules.append(module)
    return module 