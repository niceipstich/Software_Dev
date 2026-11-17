"""Mini compiler: source -> tokens -> AST -> folded AST -> stack code -> peephole -> VM.

Patterns used (tag in comments):
  Singleton, Flyweight, State (lexer), Composite (AST), Visitor, Interpreter,
  Builder, Command, Chain of Responsibility, Template Method, Decorator, Facade
"""
import re, time

# ---------- Singleton: diagnostics ----------
class Diagnostics:
    _inst = None
    def __new__(cls):
        if cls._inst is None:
            cls._inst = super().__new__(cls); cls._inst.msgs = []
        return cls._inst
    def error(self, m): self.msgs.append(m); raise SyntaxError(m)

# ---------- Flyweight: interned identifiers ----------
class Symbol:
    _pool = {}
    def __new__(cls, name):
        if name not in cls._pool:
            o = super().__new__(cls); o.name = name; cls._pool[name] = o
        return cls._pool[name]
    def __repr__(self): return self.name

# ---------- State: lexer ----------
class LexState:
    def feed(self, lx, ch): raise NotImplementedError
class Start(LexState):
    def feed(self, lx, ch):
        if ch.isdigit(): lx.buf = ch; lx.state = InNumber()
        elif ch.isalpha(): lx.buf = ch; lx.state = InIdent()
        elif ch in "+-*/()=;": lx.out.append(("OP", ch))
        elif not ch.isspace(): Diagnostics().error(f"bad char {ch!r}")
class InNumber(LexState):
    def feed(self, lx, ch):
        if ch.isdigit(): lx.buf += ch
        else: lx.out.append(("NUM", int(lx.buf))); lx.state = Start(); lx.state.feed(lx, ch)
class InIdent(LexState):
    def feed(self, lx, ch):
        if ch.isalnum(): lx.buf += ch
        else:
            kind = "KW" if lx.buf in ("let", "print") else "ID"
            lx.out.append((kind, lx.buf if kind == "KW" else Symbol(lx.buf)))
            lx.state = Start(); lx.state.feed(lx, ch)
class Lexer:
    def tokenize(self, src):
        self.out, self.state, self.buf = [], Start(), ""
        for ch in src + " ": self.state.feed(self, ch)
        return self.out

# ---------- Composite: AST ----------
class Node:
    def accept(self, v): return getattr(v, "visit_" + type(self).__name__)(self)
class Num(Node):
    def __init__(self, v): self.v = v
class Var(Node):
    def __init__(self, s): self.s = s
class BinOp(Node):
    def __init__(self, op, l, r): self.op, self.l, self.r = op, l, r
class Let(Node):
    def __init__(self, s, e): self.s, self.e = s, e
class Print(Node):
    def __init__(self, e): self.e = e

class Parser:
    def parse(self, toks):
        self.t, self.i = toks, 0
        prog = []
        while self.i < len(self.t): prog.append(self.stmt())
        return prog
    def peek(self): return self.t[self.i] if self.i < len(self.t) else (None, None)
    def eat(self, kind=None, val=None):
        k, v = self.peek()
        if (kind and k != kind) or (val and v != val): Diagnostics().error(f"expected {val or kind}, got {v}")
        self.i += 1; return v
    def stmt(self):
        if self.peek() == ("KW", "let"):
            self.eat(); s = self.eat("ID"); self.eat("OP", "="); e = self.expr(); self.eat("OP", ";"); return Let(s, e)
        self.eat("KW", "print"); e = self.expr(); self.eat("OP", ";"); return Print(e)
    def expr(self):
        n = self.term()
        while self.peek()[1] in ("+", "-") and self.peek()[0] == "OP":
            n = BinOp(self.eat(), n, self.term())
        return n
    def term(self):
        n = self.atom()
        while self.peek()[1] in ("*", "/") and self.peek()[0] == "OP":
            n = BinOp(self.eat(), n, self.atom())
        return n
    def atom(self):
        k, v = self.peek()
        if k == "NUM": self.eat(); return Num(v)
        if k == "ID": self.eat(); return Var(v)
        self.eat("OP", "("); n = self.expr(); self.eat("OP", ")"); return n

# ---------- Visitors ----------
class PrinterV:
    def visit_Num(self, n): return str(n.v)
    def visit_Var(self, n): return repr(n.s)
    def visit_BinOp(self, n): return f"({n.l.accept(self)} {n.op} {n.r.accept(self)})"
    def visit_Let(self, n): return f"let {n.s} = {n.e.accept(self)}"
    def visit_Print(self, n): return f"print {n.e.accept(self)}"

OPS = {"+": lambda a, b: a + b, "-": lambda a, b: a - b, "*": lambda a, b: a * b, "/": lambda a, b: a // b}

class ConstFoldV:                       # Visitor that returns a new tree
    def visit_Num(self, n): return n
    def visit_Var(self, n): return n
    def visit_BinOp(self, n):
        l, r = n.l.accept(self), n.r.accept(self)
        if isinstance(l, Num) and isinstance(r, Num): return Num(OPS[n.op](l.v, r.v))
        return BinOp(n.op, l, r)
    def visit_Let(self, n): return Let(n.s, n.e.accept(self))
    def visit_Print(self, n): return Print(n.e.accept(self))

# ---------- Interpreter: evaluate AST directly (reference semantics) ----------
class InterpV:
    def __init__(self): self.env, self.out = {}, []
    def visit_Num(self, n): return n.v
    def visit_Var(self, n): return self.env[n.s]
    def visit_BinOp(self, n): return OPS[n.op](n.l.accept(self), n.r.accept(self))
    def visit_Let(self, n): self.env[n.s] = n.e.accept(self)
    def visit_Print(self, n): self.out.append(n.e.accept(self))

# ---------- Command: VM instructions ----------
class Instr:
    def __init__(self, op, arg=None): self.op, self.arg = op, arg
    def execute(self, vm): getattr(vm, "do_" + self.op)(self.arg)
    def __repr__(self): return f"{self.op}" + (f" {self.arg}" if self.arg is not None else "")

# ---------- Builder: code emission ----------
class CodeBuilder:
    def __init__(self): self.code = []
    def push(self, v): self.code.append(Instr("PUSH", v)); return self
    def load(self, s): self.code.append(Instr("LOAD", s)); return self
    def store(self, s): self.code.append(Instr("STORE", s)); return self
    def binop(self, op): self.code.append(Instr({"+": "ADD", "-": "SUB", "*": "MUL", "/": "DIV"}[op])); return self
    def print_(self): self.code.append(Instr("PRINT")); return self
    def build(self): return self.code

class CodeGenV:                         # Visitor + Builder
    def __init__(self): self.b = CodeBuilder()
    def visit_Num(self, n): self.b.push(n.v)
    def visit_Var(self, n): self.b.load(n.s)
    def visit_BinOp(self, n): n.l.accept(self); n.r.accept(self); self.b.binop(n.op)
    def visit_Let(self, n): n.e.accept(self); self.b.store(n.s)
    def visit_Print(self, n): n.e.accept(self); self.b.print_()

# ---------- Chain of Responsibility: peephole ----------
class PeepHandler:
    def __init__(self, nxt=None): self.next = nxt
    def try_rewrite(self, code, i):
        r = self.match(code, i)
        if r is not None: return r
        return self.next.try_rewrite(code, i) if self.next else None
class AddZero(PeepHandler):
    def match(self, c, i):
        if i+1 < len(c) and c[i].op == "PUSH" and c[i].arg == 0 and c[i+1].op in ("ADD", "SUB"): return (2, [])
class MulOne(PeepHandler):
    def match(self, c, i):
        if i+1 < len(c) and c[i].op == "PUSH" and c[i].arg == 1 and c[i+1].op in ("MUL", "DIV"): return (2, [])
class MulTwo(PeepHandler):
    def match(self, c, i):
        if i+1 < len(c) and c[i].op == "PUSH" and c[i].arg == 2 and c[i+1].op == "MUL":
            return (2, [Instr("PUSH", 1), Instr("SHL")])

# ---------- Template Method: Pass ----------
class Pass:
    name = "pass"
    def run(self, unit):
        before = self.measure(unit); out = self.transform(unit); after = self.measure(out)
        print(f"  [{self.name}] size {before} -> {after}")
        return out
    def measure(self, unit): return len(unit)
    def transform(self, unit): raise NotImplementedError

class FoldPass(Pass):
    name = "const-fold"
    def transform(self, prog): return [s.accept(ConstFoldV()) for s in prog]
class PeepholePass(Pass):
    name = "peephole"
    chain = AddZero(MulOne(MulTwo()))
    def transform(self, code):
        out, i = [], 0
        while i < len(code):
            r = self.chain.try_rewrite(code, i)
            if r: i += r[0]; out += r[1]
            else: out.append(code[i]); i += 1
        return out

# ---------- Decorator: timing around any pass ----------
class TimedPass(Pass):
    def __init__(self, inner): self.inner, self.name = inner, inner.name
    def run(self, unit):
        t = time.perf_counter(); r = self.inner.run(unit)
        print(f"  [{self.name}] {1e6*(time.perf_counter()-t):.0f} us"); return r

# ---------- VM ----------
class VM:
    def __init__(self): self.stack, self.env, self.out = [], {}, []
    def do_PUSH(self, a): self.stack.append(a)
    def do_LOAD(self, a): self.stack.append(self.env[a])
    def do_STORE(self, a): self.env[a] = self.stack.pop()
    def do_PRINT(self, a): self.out.append(self.stack.pop())
    def do_SHL(self, a): n = self.stack.pop(); self.stack[-1] <<= n
    def _bin(self, op): b, a = self.stack.pop(), self.stack.pop(); self.stack.append(OPS[op](a, b))
    def do_ADD(self, a): self._bin("+")
    def do_SUB(self, a): self._bin("-")
    def do_MUL(self, a): self._bin("*")
    def do_DIV(self, a): self._bin("/")
    def run(self, code):
        for ins in code: ins.execute(self)
        return self.out

# ---------- Facade ----------
class Compiler:
    def compile(self, src, verbose=True):
        toks = Lexer().tokenize(src)
        ast = Parser().parse(toks)
        if verbose: print("AST      :", "; ".join(s.accept(PrinterV()) for s in ast))
        ast = TimedPass(FoldPass()).run(ast)
        if verbose: print("Folded   :", "; ".join(s.accept(PrinterV()) for s in ast))
        cg = CodeGenV()
        for s in ast: s.accept(cg)
        code = TimedPass(PeepholePass()).run(cg.b.build())
        if verbose: print("Code     :", code)
        return ast, code

if __name__ == "__main__":
    src = "let x = 2 * 3 + 4; let y = x * 1 + 0; print y * 2; print (x + 5) * 2;"
    ast, code = Compiler().compile(src)
    ref = InterpV()
    for s in ast: s.accept(ref)
    print("Interp   :", ref.out)
    print("VM       :", VM().run(code))
    assert ref.out == VM().run(code)
    assert Symbol("x") is Symbol("x")
    print("OK: interpreter and VM agree")
