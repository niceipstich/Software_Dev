"""Mini EDA tool: build netlist -> static timing -> ECO with undo -> checkpoint -> flow.

Patterns used (tag in comments):
  Singleton, Flyweight, Prototype (deepcopy), Builder, Composite, Visitor, Strategy, Decorator,
  Observer, Command, Memento, Template Method, State, Facade, Iterator
"""
import copy

# ---------- Singleton: logger ----------
class Log:
    _inst = None
    def __new__(cls):
        if cls._inst is None: cls._inst = super().__new__(cls); cls._inst.lines = []
        return cls._inst
    def info(self, m): self.lines.append(m); print("  |", m)

# ---------- Flyweight: library cell masters ----------
class CellMaster:
    _cache = {}
    def __new__(cls, name, area, delay):
        if name not in cls._cache:
            o = super().__new__(cls); o.name, o.area, o.delay = name, area, delay
            cls._cache[name] = o
        return cls._cache[name]
    def __deepcopy__(self, memo): return self      # Prototype clones must keep sharing flyweights
LIB = {n: CellMaster(n, a, d) for n, a, d in
       [("INV_X1", 1, 1.0), ("INV_X4", 3, 0.5), ("NAND2_X1", 2, 1.5), ("NAND2_X4", 5, 0.8), ("BUF_X1", 2, 1.2)]}

# ---------- Observer base ----------
class Subject:
    def __init__(self): self._obs = []
    def attach(self, cb): self._obs.append(cb)
    def notify(self, what): [cb(what) for cb in self._obs]

# ---------- Composite: Gate (leaf) and Module (container) ----------
class Component:
    def accept(self, v): return getattr(v, "visit_" + type(self).__name__)(self)
class Gate(Component):
    def __init__(self, name, master, inputs): self.name, self.master, self.inputs = name, master, inputs
class Module(Component, Subject):
    def __init__(self, name):
        Subject.__init__(self); self.name, self.children, self.inputs, self.outputs = name, [], [], []
    def add(self, c): self.children.append(c)
    def gate(self, name):
        return next(g for g in self if g.name == name)
    def __iter__(self):                 # Iterator: flattened walk of hierarchy
        for c in self.children:
            if isinstance(c, Module): yield from c
            else: yield c
    # Memento
    def save(self): return copy.deepcopy([(g.name, g.master.name) for g in self])
    def restore(self, snap):
        for name, m in snap: self.gate(name).master = LIB[m]
        self.notify("restore")
    def resize(self, gname, master): self.gate(gname).master = LIB[master]; self.notify(gname)

# ---------- Builder ----------
class NetlistBuilder:
    def __init__(self, name): self.m = Module(name)
    def input(self, *n): self.m.inputs += n; return self
    def output(self, *n): self.m.outputs += n; return self
    def cell(self, name, master, *ins): self.m.add(Gate(name, LIB[master], list(ins))); return self
    def build(self): return self.m

# ---------- Visitors ----------
class AreaV:
    def visit_Gate(self, g): return g.master.area
    def visit_Module(self, m): return sum(c.accept(self) for c in m.children)
class VerilogV:
    def visit_Gate(self, g): return f"  {g.master.name} {g.name} ({', '.join(g.inputs)}, {g.name}_o);"
    def visit_Module(self, m):
        body = "\n".join(c.accept(self) for c in m.children)
        return f"module {m.name} ({', '.join(m.inputs + m.outputs)});\n{body}\nendmodule"

# ---------- Strategy + Decorator: delay models ----------
class LibraryDelay:
    def delay(self, g): return g.master.delay
class PessimisticDelay:
    def __init__(self, k=1.2): self.k = k
    def delay(self, g): return g.master.delay * self.k
class CachedDelay:                      # Decorator over any delay model
    def __init__(self, inner): self.inner, self.cache, self.calls = inner, {}, 0
    def delay(self, g):
        key = (g.name, g.master.name)
        if key not in self.cache: self.calls += 1; self.cache[key] = self.inner.delay(g)
        return self.cache[key]

class STA:
    """Incremental-ish STA: results are invalidated by Observer callbacks."""
    def __init__(self, module, model):
        self.m, self.model, self.arrival = module, model, {}
        module.attach(lambda what: self.invalidate(what))
    def invalidate(self, what): Log().info(f"STA: invalidate ({what})"); self.arrival.clear()
    def at(self, net):
        if net in self.m.inputs: return 0.0
        if net not in self.arrival:
            g = self.m.gate(net[:-2]) if net.endswith("_o") else self.m.gate(net)
            self.arrival[net] = max(self.at(i) for i in g.inputs) + self.model.delay(g)
        return self.arrival[net]
    def worst(self): return max(self.at(o) for o in self.m.outputs)

# ---------- Command: ECO with undo ----------
class ResizeCmd:
    def __init__(self, m, gname, new): self.m, self.g, self.new, self.old = m, gname, new, m.gate(gname).master.name
    def execute(self): self.m.resize(self.g, self.new)
    def undo(self): self.m.resize(self.g, self.old)
class ECO:
    def __init__(self): self.hist = []
    def do(self, c): c.execute(); self.hist.append(c)
    def undo(self): self.hist.pop().undo()

# ---------- Template Method + State: flow stages ----------
class Stage:
    def run(self, ctx):
        Log().info(f"--> {self.name}"); self.execute(ctx); self.report(ctx)
    def execute(self, ctx): raise NotImplementedError
    def report(self, ctx): pass
class TimingStage(Stage):
    name = "timing"
    def execute(self, ctx): ctx["wns"] = ctx["sta"].worst()
    def report(self, ctx): Log().info(f"worst arrival = {ctx['wns']:.2f}")
class OptimizeStage(Stage):
    name = "optimize"
    def execute(self, ctx):
        m, sta, eco = ctx["m"], ctx["sta"], ctx["eco"]
        snap = m.save(); base = sta.worst()          # Memento checkpoint
        for g in list(m):
            up = g.master.name.replace("_X1", "_X4")
            if up in LIB and up != g.master.name:
                eco.do(ResizeCmd(m, g.name, up))
                if sta.worst() >= base: eco.undo()   # not helping -> undo
                else: base = sta.worst()
        ctx["snap"] = snap
class SignoffStage(Stage):
    name = "signoff"
    def execute(self, ctx): ctx["ok"] = ctx["sta"].worst() <= ctx["target"]
    def report(self, ctx): Log().info(f"target {ctx['target']} -> {'PASS' if ctx['ok'] else 'FAIL'}")

# ---------- Facade ----------
class Flow:
    def __init__(self, module, model, target):
        self.ctx = dict(m=module, sta=STA(module, model), eco=ECO(), target=target)
        self.stages = [TimingStage(), OptimizeStage(), TimingStage(), SignoffStage()]  # State sequence
    def run(self):
        for s in self.stages: s.run(self.ctx)
        return self.ctx

if __name__ == "__main__":
    top = (NetlistBuilder("top").input("a", "b").output("U5_o")
           .cell("U1", "NAND2_X1", "a", "b")
           .cell("U2", "INV_X1", "U1_o")
           .cell("U3", "BUF_X1", "U2_o")
           .cell("U4", "NAND2_X1", "U3_o", "b")
           .cell("U5", "INV_X1", "U4_o")
           .build())
    print("Area:", top.accept(AreaV()), "| unique masters in use:", len({id(g.master) for g in top}))
    print(top.accept(VerilogV()))
    for name, model in [("library", LibraryDelay()), ("pessimistic", PessimisticDelay(1.2))]:
        print(f"\nModel = {name}")
        top2 = copy.deepcopy(top)
        ctx = Flow(top2, CachedDelay(model), target=5.0).run()
        print("  final area:", top2.accept(AreaV()))
    assert LIB["INV_X1"] is CellMaster("INV_X1", 1, 1.0)
