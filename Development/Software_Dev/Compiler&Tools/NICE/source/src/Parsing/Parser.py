import tkinter, json

tcl = tkinter.Tcl()
root = {"type": "root", "children": []}
stack = [root]
FLAGS = {"-net", "-expose_as"}          # options that take no value

def opts(tokens):
    pos, o, i = [], {}, 0
    while i < len(tokens):
        t = tokens[i]
        if t.startswith("-"):
            if t in FLAGS or i + 1 >= len(tokens):
                o[t[1:]] = True
            else:
                o[t[1:]] = tokens[i + 1]; i += 1
        else:
            pos.append(t)
        i += 1
    return pos, o

def block(kind, *args):
    *head, body = args
    node = {"type": kind, "args": list(head), "children": []}
    stack[-1]["children"].append(node)
    stack.append(node)
    tcl.eval(body)                      # recursion into the body
    stack.pop()

def leaf(kind, *args):
    pos, o = opts(list(args))
    stack[-1]["children"].append({"type": kind, "args": pos, "opts": o})

def net_name(arg):                      # netName { -port x -netname y }
    leaf("netName", *tcl.splitlist(arg))

for k, n in [("module", "module"), ("Interface", "interface"),
             ("instance", "instance"), ("connectivity", "connectivity")]:
    tcl.createcommand(k, lambda *a, n=n: block(n, *a))
for k in ["port", "connect"]:
    tcl.createcommand(k, lambda *a, k=k: leaf(k, *a))
tcl.createcommand("netName", net_name)

tcl.eval(open("design.tcl").read())
print(json.dumps(root, indent=2))