// patterns_demo.cpp - a compilable skeleton of the AgentPnR C++ kernel layer.
// Every pattern named in the design document appears here in miniature:
//   Flyweight (LibCell), Strong typedef IDs, Memento (Snapshot), Command + RAII Transaction,
//   Strategy (IPlacer), Factory registry, Template Method (Stage), Observer (EventBus),
//   Facade (PnRFlow), Policy/Decorator-style cost (WeightedCost).
// Build:  g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 patterns_demo.cpp -o patterns_demo
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace pnr {

// ---- strong typed ids: an InstId can never be passed where a NetId is expected ------------
template <class Tag> struct StrongId {
    std::uint32_t v{};
    bool operator==(StrongId o) const { return v == o.v; }
};
using InstId = StrongId<struct InstTag>;
using NetId = StrongId<struct NetTag>;

// ---- FLYWEIGHT: one immutable master per cell type, shared by every instance ---------------
struct LibCell { std::string name; int width; double delay_ns; };

class CellLibrary {
public:
    const LibCell& add(LibCell c) {
        auto name = c.name;
        auto [it, ok] = cells_.emplace(name, std::make_unique<LibCell>(std::move(c)));
        if (!ok) throw std::runtime_error("duplicate cell " + name);
        return *it->second;
    }
    const LibCell& get(const std::string& n) const { return *cells_.at(n); }
private:
    std::unordered_map<std::string, std::unique_ptr<LibCell>> cells_;  // stable addresses
};

// ---- domain model ---------------------------------------------------------------------------
struct Instance { std::string name; const LibCell* master; int x{0}, y{0}; };
struct Net { std::string name; std::vector<InstId> pins; };

class Design {
public:
    InstId add_instance(std::string name, const LibCell& m) {
        insts_.push_back({std::move(name), &m});
        return {static_cast<std::uint32_t>(insts_.size() - 1)};
    }
    NetId add_net(std::string name, std::vector<InstId> pins) {
        nets_.push_back({std::move(name), std::move(pins)});
        return {static_cast<std::uint32_t>(nets_.size() - 1)};
    }
    Instance& inst(InstId i) { return insts_.at(i.v); }
    const Instance& inst(InstId i) const { return insts_.at(i.v); }
    std::size_t num_instances() const { return insts_.size(); }
    const std::vector<Net>& nets() const { return nets_; }

    // MEMENTO: a value-type snapshot of the mutable part of the design
    struct Snapshot { std::vector<std::pair<int, int>> pos; };
    Snapshot save() const {
        Snapshot s;
        for (auto& i : insts_) s.pos.emplace_back(i.x, i.y);
        return s;
    }
    void restore(const Snapshot& s) {
        for (std::size_t k = 0; k < insts_.size(); ++k) { insts_[k].x = s.pos[k].first; insts_[k].y = s.pos[k].second; }
    }
private:
    std::vector<Instance> insts_;
    std::vector<Net> nets_;
};

// ---- cost functions: POLICY objects combined by a weighted sum (Decorator/Composite flavour) ----
struct CostTerm { virtual ~CostTerm() = default; virtual double eval(const Design&) const = 0; };
struct Hpwl final : CostTerm {
    double eval(const Design& d) const override {
        double tot = 0;
        for (auto& n : d.nets()) {
            int x0 = 1 << 30, x1 = -x0, y0 = x0, y1 = x1;
            for (auto p : n.pins) { auto& i = d.inst(p); x0 = std::min(x0, i.x); x1 = std::max(x1, i.x); y0 = std::min(y0, i.y); y1 = std::max(y1, i.y); }
            tot += (x1 - x0) + (y1 - y0);
        }
        return tot;
    }
};
struct Overlap final : CostTerm {          // penalise cells sharing a spot
    double eval(const Design& d) const override {
        double pen = 0;
        for (std::size_t a = 0; a < d.num_instances(); ++a)
            for (std::size_t b = a + 1; b < d.num_instances(); ++b) {
                auto& A = d.inst({static_cast<std::uint32_t>(a)}); auto& B = d.inst({static_cast<std::uint32_t>(b)});
                if (A.y == B.y && A.x < B.x + B.master->width && B.x < A.x + A.master->width) pen += 100;
            }
        return pen;
    }
};
class WeightedCost final : public CostTerm {
public:
    WeightedCost& add(double w, std::unique_ptr<CostTerm> t) { terms_.emplace_back(w, std::move(t)); return *this; }
    double eval(const Design& d) const override {
        double s = 0; for (auto& [w, t] : terms_) s += w * t->eval(d); return s;
    }
private:
    std::vector<std::pair<double, std::unique_ptr<CostTerm>>> terms_;
};

// ---- STRATEGY: interchangeable placement algorithms ---------------------------------------------
struct PlaceParams { int row_pitch{6}; int die_width{40}; };
struct IPlacer {
    virtual ~IPlacer() = default;
    virtual std::string name() const = 0;
    virtual void place(Design& d, const PlaceParams& p) = 0;
};
class RowPackPlacer final : public IPlacer {              // pack cells left-to-right in row 0
public:
    std::string name() const override { return "row-pack"; }
    void place(Design& d, const PlaceParams&) override {
        int x = 0;
        for (std::size_t k = 0; k < d.num_instances(); ++k) { auto& i = d.inst({static_cast<std::uint32_t>(k)}); i.x = x; i.y = 0; x += i.master->width; }
    }
};
class TwoRowPlacer final : public IPlacer {               // alternate cells between two rows
public:
    std::string name() const override { return "two-row"; }
    void place(Design& d, const PlaceParams& p) override {
        int x[2] = {0, 0};
        for (std::size_t k = 0; k < d.num_instances(); ++k) {
            auto& i = d.inst({static_cast<std::uint32_t>(k)}); int r = static_cast<int>(k % 2);
            i.x = x[r]; i.y = r * p.row_pitch; x[r] += i.master->width;
        }
    }
};

// ---- FACTORY REGISTRY: strategies are created by name, so agents can choose them from JSON -------
template <class Base> class Registry {
public:
    using Maker = std::function<std::unique_ptr<Base>()>;
    void add(std::string key, Maker m) { makers_[std::move(key)] = std::move(m); }
    std::unique_ptr<Base> make(const std::string& key) const {
        auto it = makers_.find(key);
        if (it == makers_.end()) throw std::invalid_argument("unknown kernel '" + key + "'");
        return it->second();
    }
private:
    std::unordered_map<std::string, Maker> makers_;
};

// ---- OBSERVER: stages publish metrics; the agent runtime, logger and checkpointer subscribe -------
struct MetricEvent { std::string stage; std::string key; double value; };
class EventBus {
public:
    using Handler = std::function<void(const MetricEvent&)>;
    void subscribe(Handler h) { handlers_.push_back(std::move(h)); }
    void publish(const MetricEvent& e) const { for (auto& h : handlers_) h(e); }
private:
    std::vector<Handler> handlers_;
};

// ---- COMMAND + RAII TRANSACTION: every ECO is undoable; a failed transaction rolls itself back ------
struct EcoCommand {
    virtual ~EcoCommand() = default;
    virtual void apply(Design&) = 0;
    virtual void undo(Design&) = 0;
    virtual std::string describe() const = 0;
};
class MoveInstance final : public EcoCommand {
public:
    MoveInstance(InstId i, int nx, int ny) : id_(i), nx_(nx), ny_(ny) {}
    void apply(Design& d) override { auto& i = d.inst(id_); ox_ = i.x; oy_ = i.y; i.x = nx_; i.y = ny_; }
    void undo(Design& d) override { auto& i = d.inst(id_); i.x = ox_; i.y = oy_; }
    std::string describe() const override { return "move inst " + std::to_string(id_.v) + " to (" + std::to_string(nx_) + "," + std::to_string(ny_) + ")"; }
private:
    InstId id_; int nx_, ny_, ox_{0}, oy_{0};
};
class Transaction {
public:
    explicit Transaction(Design& d) : d_(d) {}
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction() { if (!committed_) rollback(); }       // RAII: leaving scope without commit() undoes everything
    void exec(std::unique_ptr<EcoCommand> c) { c->apply(d_); done_.push_back(std::move(c)); }
    void commit() { committed_ = true; }
    std::size_t size() const { return done_.size(); }
private:
    void rollback() { for (auto it = done_.rbegin(); it != done_.rend(); ++it) (*it)->undo(d_); done_.clear(); }
    Design& d_;
    std::vector<std::unique_ptr<EcoCommand>> done_;
    bool committed_{false};
};

// ---- TEMPLATE METHOD: the fixed skeleton of every flow stage; subclasses fill in execute() -------
class Stage {
public:
    Stage(std::string name, const EventBus& bus, const CostTerm& cost) : name_(std::move(name)), bus_(bus), cost_(cost) {}
    virtual ~Stage() = default;
    bool run(Design& d) {                                  // non-virtual: the invariant skeleton
        auto before = d.save();                            // Memento: checkpoint
        double c0 = cost_.eval(d);
        execute(d);
        double c1 = cost_.eval(d);
        bus_.publish({name_, "cost_before", c0});
        bus_.publish({name_, "cost_after", c1});
        if (c1 > c0 && !allow_regression()) { d.restore(before); bus_.publish({name_, "rolled_back", 1}); return false; }
        return true;
    }
protected:
    virtual void execute(Design& d) = 0;                   // the one step that varies
    virtual bool allow_regression() const { return false; }
    std::string name_;
private:
    const EventBus& bus_;
    const CostTerm& cost_;
};
class PlaceStage final : public Stage {
public:
    PlaceStage(const EventBus& b, const CostTerm& c, std::unique_ptr<IPlacer> p) : Stage("place", b, c), placer_(std::move(p)) {}
protected:
    void execute(Design& d) override { placer_->place(d, {}); }
private:
    std::unique_ptr<IPlacer> placer_;
};

// ---- FACADE: the single entry point the Python agents (via pybind11) talk to ---------------------
class PnRFlow {
public:
    PnRFlow() {
        placers_.add("row-pack", [] { return std::make_unique<RowPackPlacer>(); });
        placers_.add("two-row", [] { return std::make_unique<TwoRowPlacer>(); });
        cost_.add(1.0, std::make_unique<Hpwl>()).add(1.0, std::make_unique<Overlap>());
    }
    EventBus& bus() { return bus_; }
    double run_place(Design& d, const std::string& strategy) {
        PlaceStage s(bus_, cost_, placers_.make(strategy));   // Factory + Strategy + Template Method
        s.run(d);
        return cost_.eval(d);
    }
    double cost(const Design& d) const { return cost_.eval(d); }
private:
    EventBus bus_;
    Registry<IPlacer> placers_;
    WeightedCost cost_;
};

}  // namespace pnr

int main() {
    using namespace pnr;
    CellLibrary lib;
    const auto& inv = lib.add({"INV", 2, 0.04});
    const auto& nand = lib.add({"NAND2", 3, 0.06});
    const auto& dff = lib.add({"DFF", 6, 0.16});

    Design d;
    auto a = d.add_instance("U1", nand), b = d.add_instance("U2", inv), c = d.add_instance("U3", dff), e = d.add_instance("U4", nand);
    d.add_net("n1", {a, b}); d.add_net("n2", {b, c}); d.add_net("n3", {c, e, a});

    PnRFlow flow;
    flow.bus().subscribe([](const MetricEvent& m) { std::cout << "  [event] " << m.stage << "." << m.key << " = " << m.value << "\n"; });

    for (auto* s : {"row-pack", "two-row"}) {
        std::cout << "strategy " << s << ":\n";
        double c = flow.run_place(d, s);                     // events print while the stage runs
        std::cout << "  total cost after place = " << c << "\n";
    }

    std::cout << "\nECO transaction that fails half-way (rolls back automatically):\n";
    auto before = d.save();
    try {
        Transaction tx(d);
        tx.exec(std::make_unique<MoveInstance>(a, 30, 0));
        std::cout << "  applied: move U1 -> cost " << flow.cost(d) << "\n";
        tx.exec(std::make_unique<MoveInstance>(InstId{99}, 0, 0));   // invalid id -> throws
        tx.commit();
    } catch (const std::exception& ex) { std::cout << "  error: " << ex.what() << "\n"; }
    auto after = d.save();
    std::cout << "  positions restored: " << (before.pos == after.pos ? "yes" : "NO") << "\n";

    try { flow.run_place(d, "does-not-exist"); } catch (const std::exception& ex) { std::cout << "\nfactory error: " << ex.what() << "\n"; }
    return 0;
}
