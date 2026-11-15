# NICE: Python or Tcl Input Design, Module Table, and AI Scope

Nov 8, 2026 · @Ausaf

## Summary

NICE reads one input file, either Python or Tcl and never both. It describes modules with interfaces and ports, a Design with interface-level connectivity, and groups. The tool builds a flat design first, then processes the groups into the exact RTL hierarchy, and checks every connection as it is made, in a single pass.

- **Pipeline:** interface-level IR (a small graph) feeding the IP-XACT dump, port-level IR feeding the JSON debug dump and the RTL printer, port-by-port connectivity at instance level and parent-module level (the huge graphs), group processing, then a pretty printer that writes RTL.
- **Checks:** inline, during connectivity, so the huge port graphs are walked once. A flatten-and-compare run stays available for tests.
- **AI:** outside the trusted path and only proposing. LLMs first; torch only for port-name classification.
- **Open points:** how two interfaces pair their ports, the group syntax and the meaning of -net are listed under Input.

## Architecture

&#91;embedded content: NICE flow: one input file to RTL · interface graph, port IR, inline checks, groups\]

The input is read once. The small interface graph goes to IP-XACT, each interface then becomes ports, and every port link is checked as it is made. Groups turn the flat design into the RTL hierarchy, and the printer only writes what the IR holds. Nothing above the dashed grey line is trusted, and the dashed red line returns a rejection to the AI proposer.

## Input: one Python or one Tcl file

The tool takes exactly one input file, in Python or in Tcl, never a mix. Either way it describes four things: modules with interfaces and ports, the Design with its instances, interface-level connectivity, and groups (a group name plus its child instances). Groups are not in the example below because they are applied later, after the flat design exists.

```tcl
module mod1 {
    Interface ifce1 {
        port a -direction input -width 4
    }
}

module mod2 {
    Interface ifce2 {
        port b -direction output -width 4
    }
}

module Design {
    connectivity {
        connect ifce1 ifce2 -net
    }
}
```

That syntax is already valid Tcl: `module`, `Interface`, `port`, `connectivity` and `connect` can be commands registered by the tool, with the braces as script bodies. So the Tcl reader runs the file instead of parsing it, and loops over buses come for free.

**How the Tcl runs**

- The core is C++. It links libtcl, creates an interpreter with `Tcl_CreateInterp`, makes it safe with `Tcl_MakeSafe` (file, process and network commands are hidden; check the hidden list against your Tcl version), and registers `module`, `Interface`, `port`, `connectivity` and `connect` with `Tcl_CreateObjCommand`.
- Each command validates its arguments and records an object. A script that runs too long is stopped by a Tcl resource limit (time or command count) or by running the interpreter in a worker process with a wall-clock limit.
- Python input runs in a child process with time and memory limits, because Python cannot be sandboxed reliably in-process. The child hands the core the same Design description, and the core builds the same Design model from it.
- The acceptance test is the same design written in Python and in Tcl (in separate runs) giving identical IR.

**To confirm**

- The example connects `ifce1` and `ifce2` without naming instances. I assume real designs write `instance.interface`, so two instances of one module stay distinct.
- Port a (input) and port b (output) have different names, so connecting two interfaces needs a pairing rule. I assume ports pair by declaration order, with opposite directions and equal widths.
- `-net` is taken to mean: create a net between the two interfaces.
- The group command is not shown yet. This doc treats it as a group name plus child instances.
- The two levels of connectivity are read as instance level and parent-module level (see the pipeline section).

## Module-wise table

Thirteen modules, in flow order. Only ai/propose ever calls a model. The connectivity checks are inside modules 6 to 8, not a separate pass.

| # | Module | Job | In → out | Rejects with | AI hook |
| --- | --- | --- | --- | --- | --- |
| 1 | frontend/read (tcl or py) | Run the single input file; commands or calls build modules, interfaces, ports, instances, connectivity, groups | File → Design model | E\_TCL (new, Tcl only), E\_SCHEMA, E\_PORT\_TYPE, duplicate names | #2, #3, #4 propose only; user confirms |
| 2 | model/validate | Resolve references, unique names, direction and clk rules | Design model → validated model | E\_UNKNOWN\_REF, E\_DUP\_CONN, E\_DIRECTION, E\_CLK\_RENAME | None |
| 3 | ir/interface\_ir | Interface-level IR: a small graph of instance interfaces and connect edges; pair the ports of each edge | Validated model → interface IR | E\_IFACE\_PAIR (new) | None |
| 4 | emit/ipxact | Dump IP-XACT from the interface IR in two forms: bus and abstraction definitions per interface type, and ad hoc port-level connections | Interface IR → XML | None | None |
| 5 | ir/ports | Expand each interface into ports with direction and width: the port-level IR | Interface IR → port IR | None | None |
| 6 | engine/connect\_instance | Port-by-port links between instance ports, flat at top, checked as each link is made | Port IR → instance-level links | E\_WIDTH, E\_SLICE\_RANGE, E\_MULTI\_DRIVER | None |
| 7 | engine/connect\_parent | Links between a module's own ports and its children's ports; end-of-pass undriven count | Port IR → parent-level links | E\_UNDRIVEN, plus the codes of #6 | None |
| 8 | engine/groups | Module per group, children moved in, boundary ports, wires at the lowest common group | Flat IR + groups → hierarchical IR | E\_GROUP (new), E\_NAME\_COLLISION | None |
| 9 | emit/rtl\_printer | Pretty printer: write RTL from the IR in sorted order, no connectivity logic | Hierarchical IR → .v | None | None; AI never emits |
| 10 | ai/propose | Build the prompt from the skeleton view; the only module that calls a model | Skeleton view + request + errors → JSON | None itself; its JSON goes to #11 | #1, #5, #6 |
| 11 | parse/strict\_ai | One strict schema for AI output; stamps origin = ai | JSON → connect objects | E\_SCHEMA | None |
| 12 | report/review | Plain-language summary of every origin = ai connection | IR with origin tags → report | None | #7; changes nothing |
| 13 | emit/json\_dump | Write-only debug dump of the port IR, sorted keys, never re-read | Port IR → .json | None | None |

Three additions to the first-lap error catalogue: E\_GROUP (a group child that does not exist, an instance in two groups, or a cycle), E\_TCL (Tcl syntax error, forbidden command, or time limit), and E\_IFACE\_PAIR (the two interfaces of a connect cannot be paired: different port counts or the same directions).

## Pipeline: input file to RTL

Each stage reads only the output of the one before it, so each can be tested alone. Graph sizes differ: the interface graph is small, the port graphs are huge.

1. **Read and validate.** Run the file, build the Design model, resolve every reference.
2. **Interface IR (small).** Nodes are instance interfaces, edges are connect statements. Port pairing and direction are checked here.
3. **IP-XACT dump** from the interface IR, in both forms so the IP-XACT is complete: bus and abstraction definitions per interface type, and ad hoc port-level connections.
4. **Port IR.** Each interface expands into ports with direction and width. This level feeds the JSON debug dump and the RTL printer.
5. **Flat connectivity, port by port (huge graphs).** Two levels: instance level (instance port to instance port) and parent-module level (a module's own ports to its children's ports; in the flat design the parent is the Design top). Checks run as each link is made.
6. **Group processing.** Takes the flat design to the exact RTL hierarchy.
7. **Outputs from the port IR.** A JSON debug dump (write-only, sorted keys, never re-read) and the pretty printer, which writes RTL and holds no connectivity logic.

### Group processing

A group is a name plus child instances. Process groups bottom-up in sorted order. For each group, create its module, move the children in, and classify every net that touches them:

- all endpoints inside: an internal wire of the group
- driver inside, a load outside: an output boundary port
- driver outside, a load inside: an input boundary port
- clk: one pinned-name input per group, fanning out inside

A boundary port keeps the IP's real port name. Two nets wanting the same name at one boundary are E\_NAME\_COLLISION, never renamed silently. A finished group module then counts as one instance at the next level up, and each wire is declared at the lowest common group of its driver and loads. Every group becomes its own Verilog module with an instance statement in its parent, and independent subtrees can be processed in parallel.

## Inline checking and graph scale

There is no separate checker pass. Checks run while connectivity is made, so the huge port graphs are walked once.

| Check | When it runs | Code |
| --- | --- | --- |
| Port pairing and direction | Interface graph build | E\_IFACE\_PAIR, E\_DIRECTION |
| Equal widths | As each port link is made | E\_WIDTH |
| Slice inside the port | As each port link is made | E\_SLICE\_RANGE |
| Single driver | As each link is made, by updating a driven-bit map per port | E\_MULTI\_DRIVER |
| Undriven inputs | One counter per port compared with its width at the end of the pass; no re-scan | E\_UNDRIVEN |
| Boundary names and group rules | During group processing, when a boundary port is created | E\_NAME\_COLLISION, E\_GROUP |

The trade-off: inline checks share code with the builder, so a builder bug could hide in both. Keep an independent flatten-and-compare run (flatten the hierarchy and diff it with the flat IR, then build twice and compare hashes) for tests and CI, not for the normal single pass.

**Can a GNN process the connection graph?** Not to build it: a learned model would approximate connectivity, and the design rests on connectivity being exact. A GNN can sit beside the flow as an advisory ranker that scores finished connections by how unlikely they look, for a person to review. That needs labelled wrong connections, and it never changes the RTL. A graph this large needs neighbour sampling or partitioning, which PyTorch Geometric and DGL support. For port-name problems (#2, #3) the graphs are per module and small.

**Can an accelerator help?** The interface graph is small. The port graphs are close to linear work (lookups, a driven-bit map, a walk up the group tree), so profile first. In order: keep slices as intervals instead of single bits; shard the nets, since every check above is local to one net or one port, and process group subtrees in parallel; move hot loops to numpy or a compiled language; only then a GPU graph library, for bulk array steps. Union-find on a CPU is already fast, so a GPU pays off only if memory bandwidth becomes the limit.

## Where AI fits

AI can be useful at eight points, all of them proposals that a deterministic module accepts or rejects. Hooks #1 to #7 are from the first-lap design; #8 is new, for the Tcl and Python inputs.

| Hook | Attaches to | AI proposes | Checked by | First lap |
| --- | --- | --- | --- | --- |
| #1 | ai/propose → conn\_parse | Prose request into a Conn list in strict JSON | Strict parser, then the full gatekeeper | Yes |
| #2 | frontend/read | Vendor port names mapped to standard protocol signals, with confidence | Direction, width and completeness checks; user confirms | Yes |
| #3 | frontend/read | Loose ports grouped into interfaces | Same as #2 | Later |
| #4 | frontend/read | Warning about missing mandatory signals | A deterministic protocol table decides | Later |
| #5 | Grouping | Suggested groups for instances | E\_GROUP and collision checks; user confirms | Later |
| #6 | After any rejection | A corrected Conn list, from the error codes | Gatekeeper again, 3 tries, then a person reads the errors | Yes |
| #7 | report/review | Review summary of origin = ai connections | Changes nothing | Yes |
| #8 | Offline helper | Draft Tcl or Python from the other language or from prose | The draft runs through its own frontend; the sorted Conn list is diffed against the source; user reviews the script | Optional |

Hook #8 bends the rule that AI emits no code. It is allowed because its output is a draft script that a person reviews and then runs as ordinary user input through the sandbox; it never goes straight into the engine.

What the gatekeeper cannot catch is a connection that is well-formed but not what the person meant, for example the right widths on the wrong pair of ports. That is why every AI connection carries origin = ai and shows up in the review report, and why #1 results should be confirmed by a person before they are trusted.

**AI must never:** decide width, driver or direction correctness; create nets, route through groups or name wires; rename clk; edit the skeleton, the groups or the engine's output; emit Verilog or IP-XACT.

## LLM scope

An LLM fits wherever the input is words and the output is a small, checkable structure: prose to Conn JSON, odd vendor names to protocol signals, error codes to a corrected list. It does not fit anywhere a wrong answer would pass the gatekeeper unnoticed.

**Good fits, in order of payoff**

1. **Repair after rejection (#6).** Input is the failed list plus structured errors; output is a new list. It is the easiest loop to evaluate because the gatekeeper is the judge.
2. **Prose to Conn JSON (#1).** The most useful for users and the riskiest, because a valid but wrong connection passes the gate. Keep a person in the confirmation step.
3. **Vendor name mapping (#2).** Few-shot, with confidence. This is also the baseline the torch classifier has to beat.
4. **Review report (#7).** Build the table of origin = ai connections deterministically; let the LLM add only a one-line rationale per row.
5. **Script drafts (#8).** Tcl to Python and back, or prose to a script, always reviewed by a person.

**Contract for every call**

- Input is the skeleton view (names, directions, widths) plus the request. For large designs, pre-filter to the IPs the request names, found by plain string lookup, so the prompt stays small.
- Output is JSON in a fixed schema, enforced with the model's structured-output or JSON-schema mode where it exists, otherwise by the strict parser rejecting it. Temperature zero.
- Every instance, interface and port name must already exist in the skeleton, so a hallucinated name is caught by lookup.
- Retry budget of three, then stop and show the person the errors.

**Evaluation, before trusting any of it**

Build a gold set from open-source designs: a request in words and the expected Conn set. Measure the share of lists that pass the gate on the first try, tries needed by the repair loop, hallucinated-name rate, and the rate of valid-but-wrong connections (found by diffing against the gold set). The numbers in the earlier paper are targets, not baselines; re-measure on your own designs.

**Hosted or local.** Port and interface names can be sensitive for proprietary IP. Options are a hosted API for non-sensitive skeletons only, or a local model for everything. This is still an open decision (see the last section).

## Torch scope

Torch earns its place only where there is a labelled classification problem that rules and an LLM do not solve well enough. In this tool that is one problem for sure (vendor port names) and one maybe (interface grouping). Everything else is a lookup, a rule or an LLM task.

| Problem | Hook | Try first | Torch model, if the baseline falls short | Data needed |
| --- | --- | --- | --- | --- |
| Vendor port name to protocol signal | #2 | Regex and token rules against the protocol table, then LLM few-shot | Small character-level or token-level classifier over name, direction, width and sibling names | Labelled (name, signal) pairs from open-source IPs, plus synthetic variants |
| Group loose ports into interfaces | #3 | Prefix clustering, then LLM | Graph attention network over ports in a module, as in the earlier paper | Modules with known interface membership |
| Missing mandatory signals | #4 | Protocol table lookup (no model) | None; the spec already lists what is mandatory per protocol version | None |
| Group suggestions for instances | #5 | LLM from prose intent | None | None |
| Connectivity, width, driver and routing checks | Core | Deterministic engine | Never | None |

Two points differ from the earlier paper. The link-prediction head for missing signals duplicates a table lookup, so drop it. The GNN is worth building only if prefix clustering and the LLM both underperform on #3.

**How to train and judge the classifier**

- Split by IP or vendor, not by random port; a random split leaks naming habits and overstates accuracy.
- Mask labels a port cannot have: the direction and width from the skeleton remove illegal signals before the argmax.
- Let it abstain below a confidence threshold, and send abstentions to the person; the user confirms #2 output anyway.
- Augment with prefixes (s\_axi\_), suffixes (\_i, \_o), case changes and bus renames.
- Compare against the rule baseline and the LLM on the same held-out set; keep the model only if it wins on precision at the same coverage.

**Practical points.** A name classifier of this size will likely train and run on a CPU. Put it behind the same function as the rule and LLM versions, for example `propose_map(skeleton) -> ProposedMap`, so the three are swappable and compared on one eval. Fix seeds, run in eval mode, and record the model version in the review report so a run can be reproduced.

## C++ implementation stack

| Area | Tool | Notes |
| --- | --- | --- |
| Language and build | C++17 or C++20 (clang or gcc), CMake, Ninja | clang-format and clang-tidy for style and static checks |
| Dependencies | vcpkg or Conan | Pin versions; vendor the single header json.hpp |
| Tcl input | libtcl 8.6 or newer (C API) | Tcl\_CreateInterp, Tcl\_MakeSafe, Tcl\_CreateObjCommand |
| Python input | Child process with time and memory limits | Or embed with pybind11 if no sandbox is needed |
| JSON | nlohmann/json | Debug dump: keys are sorted by default, so output is deterministic; write in chunks (per group or instance) for large IR. AI JSON: parse with exceptions off (`json::parse(text, nullptr, false)`, then `is_discarded()`) and map failure to E\_SCHEMA; add json-schema-validator for the schema |
| IP-XACT | pugixml or libxml2 | libxml2 can validate against the IEEE 1685 XSD schemas |
| Data structures | Integer IDs with a name-interning table; a flat hash map or CSR arrays | Matters for the huge port graphs |
| Parallelism | std::thread or oneTBB | Shard nets and group subtrees |
| Tests | GoogleTest or Catch2, RapidCheck | Python hypothesis can generate random designs that drive the binary |
| Safety | ASan, UBSan, TSan; valgrind or heaptrack |  |
| RTL verification | Verilator (lint only), Icarus Verilog, Yosys | Yosys can flatten the printed RTL and compare it independently |
| Profiling | perf with flamegraphs, VTune, Tracy |  |

## AI and the C++ API

AI stays outside the C++ core and reaches it through a small, narrow interface. These are the changes to the hook table above:

| Hook | Use | Change |
| --- | --- | --- |
| #5 Group suggestions | Propose group names and child instances | Higher priority now that groups are core; E\_GROUP, E\_NAME\_COLLISION and the user decide |
| #2, #3, #4 | Port-name mapping, grouping, missing signals | Lower priority; needed only for modules whose ports are not already in Interface blocks |
| New: bus mapping | Propose which standard bus definition an interface matches, for the bus-definition form of IP-XACT | The bus definition's port list decides; the user confirms |
| New: debug assistant | An LLM reads the JSON debug dump and explains why a net exists | Read-only, changes nothing |

What the core must provide so AI can call it:

1. **Machine-readable errors:** code, objects involved, connection index, and file and line in the input. The inline pass collects errors up to a limit instead of stopping at the first, so the repair loop sees more than one problem.
2. **A dry-run mode** that checks without writing outputs.
3. **A skeleton-view function** returning only names, directions and widths, never RTL.
4. **A stable boundary:** start with a CLI (input in, JSON errors out). Later add a C ABI (`extern "C"`, opaque handles, no exceptions across it) with pybind11 or nanobind bindings; the C++ ABI is not stable between compilers.
5. **A narrow AI-facing API:** get skeleton view, dry-run check, get errors. Nothing in it can emit RTL or IP-XACT.
6. **An origin = ai field** on input connections, kept through to the review report.
7. **No global state, thread-safe, deterministic**, so evaluation and repair retries can run in parallel.
8. **Process isolation:** the AI layer runs as a separate Python service and calls the core through the CLI or bindings.

If a model ever has to run inside a C++ step, use ONNX Runtime or LibTorch. For now keep the core model-free.

## Build order and open decisions

Build the deterministic path first; each AI step then has something to be measured against.

1. Tcl reader with the safe interpreter (the syntax already is Tcl), validation, interface graph and IP-XACT dump.
2. Interface IR, flat port-by-port connectivity with inline checks, and the RTL printer for the flat design. Acceptance: small designs checked by hand.
3. Group processing and hierarchical RTL. Acceptance: flatten-and-compare in tests.
4. Python reader producing the same Design model. Acceptance: one design in Python and in Tcl, identical IR (separate runs).
5. LLM hooks #6, #1 and #7 with the gold set and metrics above.
6. Hook #2 rule baseline, then LLM, then a torch classifier only if the gap justifies it.
7. Profile at scale; shard by net and by group subtree; compiled hot loops.

| Decision | Why it matters | Options |
| --- | --- | --- |
| Interface names in connect | The example names interfaces without instances | instance.interface paths (assumed), or unique interface names only |
| Port pairing between two interfaces | Ports a and b have different names | Declaration order (assumed), an explicit name map, or by role |
| Meaning of -net | Decides what the connect creates | A net between the two interfaces (assumed), or other modes |
| Group syntax | Not shown yet | Group name + child instances (assumed), child groups allowed or not |
| Name collision at a group boundary | Two children can expose the same port name | Hard error (current), or a user-given prefix rule |
| Tcl runtime | Sets sandbox strength and installability | libtcl embedded in the C++ core with Tcl\_MakeSafe (suggested), or a tclsh subprocess |
| Hosted or local model | Port names can be sensitive | Hosted for non-sensitive skeletons only, or local for everything |
| Test designs | Keeps the work independent of any employer's code and data | Open-source IPs and your own files only |
