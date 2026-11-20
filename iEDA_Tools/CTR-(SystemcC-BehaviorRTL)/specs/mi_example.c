/*
 * mi_example.c - multiple inheritance lowered to C, with this-adjusting thunks.
 *
 *   struct A { virtual void f(); int a; };
 *   struct B { virtual void g(); int b; };
 *   struct C : A, B {
 *       void f() override;       // overrides A::f
 *       void g() override;       // overrides B::g
 *       virtual void h();        // new virtual
 *       int c;
 *   };
 *
 * Build:  cc -std=c11 -Wall -Wextra -fsanitize=address,undefined mi_example.c
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------- A ---------- */
struct A;
typedef struct A_vtable {
    ptrdiff_t offset_to_top;
    void (*f)(struct A *);
} A_vtable;
typedef struct A { const A_vtable *vptr; int a; } A;

static void A_f(A *this_) { (void)this_; puts("A::f"); }
static const A_vtable A_vt = { 0, A_f };
static void A_ctor(A *this_, int a) { this_->vptr = &A_vt; this_->a = a; }

/* ---------- B ---------- */
struct B;
typedef struct B_vtable {
    ptrdiff_t offset_to_top;
    void (*g)(struct B *);
} B_vtable;
typedef struct B { const B_vtable *vptr; int b; } B;

static void B_g(B *this_) { (void)this_; puts("B::g"); }
static const B_vtable B_vt = { 0, B_g };
static void B_ctor(B *this_, int b) { this_->vptr = &B_vt; this_->b = b; }

/* ---------- C ---------- */
struct C;
typedef struct C_vtable {
    A_vtable a_part;                /* A's slots first (f overridden)  */
    void (*h)(struct C *);          /* then C's own new virtuals       */
} C_vtable;

typedef struct C { A base_A; B base_B; int c; } C;

/* the real implementations take C* */
static void C_f(C *this_) { printf("C::f a=%d c=%d\n", this_->base_A.a, this_->c); }
static void C_g(C *this_) { printf("C::g b=%d c=%d\n", this_->base_B.b, this_->c); }
static void C_h(C *this_) { (void)this_; puts("C::h"); }

/* adapters / thunks: what the vtable slots actually point to */
static void C_f_via_A(A *p) {                   /* adjustment is 0          */
    C_f((C *)p);
}
static void C_g_via_B(B *p) {                   /* adjustment is -offsetof  */
    C_g((C *)((char *)p - offsetof(C, base_B)));
}

static const C_vtable C_vt_primary = {
    { 0, C_f_via_A },
    C_h
};
static const B_vtable C_vt_B = {
    -(ptrdiff_t)offsetof(C, base_B),            /* offset_to_top = -16      */
    C_g_via_B
};

static void C_ctor(C *this_, int a, int b, int c) {
    A_ctor(&this_->base_A, a);                  /* sets vptr = &A_vt        */
    B_ctor(&this_->base_B, b);                  /* sets vptr = &B_vt        */
    this_->base_A.vptr = &C_vt_primary.a_part;  /* overwrite with C's       */
    this_->base_B.vptr = &C_vt_B;
    this_->c = c;
}

/* upcast / downcast rules (null must stay null) */
static B *C_to_B(C *p) { return p ? &p->base_B : NULL; }
static C *B_to_C(B *p) { return p ? (C *)((char *)p - offsetof(C, base_B)) : NULL; }

int main(void) {
    C *obj = malloc(sizeof *obj);
    C_ctor(obj, 1, 2, 3);

    A *pa = &obj->base_A;                       /* upcast: +0               */
    B *pb = C_to_B(obj);                        /* upcast: +16              */

    pa->vptr->f(pa);                            /* C_f_via_A -> C_f         */
    pb->vptr->g(pb);                            /* C_g_via_B -> C_g, -16    */
    ((const C_vtable *)obj->base_A.vptr)->h(obj);   /* static type C        */

    printf("sizeof(C)=%zu, B-part offset=%zu\n", sizeof(C), offsetof(C, base_B));
    printf("downcast B*->C* round-trips: %s\n", B_to_C(pb) == obj ? "yes" : "NO");
    printf("null stays null: %s\n", C_to_B(NULL) == NULL ? "yes" : "NO");
    printf("offset_to_top of B's vtable in C: %td\n", pb->vptr->offset_to_top);

    free(obj);
    return 0;
}
