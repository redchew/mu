/*
 * Table variable type
 */

#ifdef MU_DEF
#ifndef MU_TBL_DEF
#define MU_TBL_DEF
#include "mu.h"


// Hash type definition
typedef uint_t hash_t;

// Definition of Mu's table type
typedef mu_aligned struct tbl tbl_t;


#endif
#else
#ifndef MU_TBL_H
#define MU_TBL_H
#include "var.h"
#include "err.h"


// Each table is composed of an array of values 
// with a stride for keys/values. If keys/values 
// is not stored in the array it is implicitely 
// stored as a range/offset based on the specified 
// offset and length.
struct tbl {
    ref_t ref; // reference count

    len_t len;  // count of non-nil entries
    len_t nils; // count of nil entries
    uintq_t npw2;   // log2 of capacity
    uintq_t stride; // type of table

    tbl_t *tail; // tail chain of tables

    union {
        uint_t offset; // offset for implicit ranges
        var_t *array;  // pointer to stored data
    };
};


// Table pointer manipulation with the ro flag
mu_inline tbl_t *tbl_ro(tbl_t *tbl) {
    return (tbl_t *)(MU_RO | (uint_t)tbl);
}

mu_inline bool tbl_isro(tbl_t *t) {
    return MU_RO & (uint_t)t;
}

mu_inline tbl_t *tbl_read(tbl_t *tbl) {
    return (tbl_t *)(~MU_RO & (uint_t)tbl);
}

mu_inline tbl_t *tbl_write(tbl_t *t, eh_t *eh) {
    if (tbl_isro(t))
        err_readonly(eh);
    else
        return t;
}


// Table creating functions and macros
tbl_t *tbl_create(len_t len, eh_t *eh);
void tbl_destroy(tbl_t *t);

mu_inline var_t vntbl(len_t l, eh_t *eh) { return vtbl(tbl_create(l, eh)); }

mu_inline len_t tbl_getlen(tbl_t *t) { return tbl_read(t)->len; }

// Table reference counting
mu_inline void tbl_inc(tbl_t *t) { ref_inc((void *)tbl_read(t)); }
mu_inline void tbl_dec(tbl_t *t) { ref_dec((void *)tbl_read(t), 
                                           (void *)tbl_destroy); }


// Recursively looks up a key in the table
// returns either that value or nil
var_t tbl_lookup(tbl_t *t, var_t key);

// Recursively looks up either a key or index
// if key is not found
var_t tbl_lookdn(tbl_t *t, var_t key, hash_t i);

// Recursively assigns a value in the table with the given key
// decends down the tail chain until its found
void tbl_assign(tbl_t *t, var_t key, var_t val, eh_t *eh);

// Inserts a value in the table with the given key
// without decending down the tail chain
void tbl_insert(tbl_t *t, var_t key, var_t val, eh_t *eh);

// Sets the next index in the table with the value
void tbl_append(tbl_t *t, var_t val, eh_t *eh);

// Performs iteration on a table
fn_t *tbl_iter(tbl_t *t, eh_t *eh);

// Table representation
str_t *tbl_repr(tbl_t *t, eh_t *eh);


// Macro for iterating through a table in c
// Assign names for k and v, and pass in the 
// block to execute for each pair in tbl
#define tbl_for_begin(k, v, tbl) {                  \
    var_t k;                                        \
    var_t v;                                        \
    tbl_t *_t = tbl_read(tbl);                      \
    uint_t _i, _c = _t->len;                        \
                                                    \
    for (_i=0; _c; _i++) {                          \
        switch (_t->stride) {                       \
            case 0:                                 \
                k = vuint(_i);                      \
                v = vuint(_t->offset + _i);         \
                break;                              \
            case 1:                                 \
                k = vuint(_i);                      \
                v = _t->array[_i];                  \
                break;                              \
            case 2:                                 \
                k = _t->array[2*_i  ];              \
                v = _t->array[2*_i+1];              \
                if (isnil(k) || isnil(v))           \
                    continue;                       \
                break;                              \
        }                                           \
{
#define tbl_for_end                                 \
}                                                   \
        _c--;                                       \
    }                                               \
}


#endif
#endif
