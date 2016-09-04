/*
 * Mu config
 */

#ifndef MU_CONFIG_H
#define MU_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


// Determine if mu is 32-bit or 64-bit
#if !defined(MU32) && !defined(MU64)
#if UINT32_MAX == UINTPTR_MAX
#define MU32
#elif UINT64_MAX == UINTPTR_MAX
#define MU64
#else
#error "Unspecified word size for Mu"
#endif
#endif

// Definitions of the basic types used in Mu
// Default and half-sized integer types
#ifdef MU32
typedef int8_t   mintq_t;
typedef uint8_t  muintq_t;
typedef int16_t  minth_t;
typedef uint16_t muinth_t;
typedef int32_t  mint_t;
typedef uint32_t muint_t;
typedef float    mfloat_t;
#else
typedef int16_t  mintq_t;
typedef uint16_t muintq_t;
typedef int32_t  minth_t;
typedef uint32_t muinth_t;
typedef int64_t  mint_t;
typedef uint64_t muint_t;
typedef double   mfloat_t;
#endif

// Smallest addressable unit
typedef unsigned char mbyte_t;

// Length type for strings/tables
typedef muinth_t mlen_t;


// Flags for Mu options
//#define MU_DEBUG
#define MU_MALLOC
//#define MU_DISASSEMBLE
//#define MU_COMPUTED_GOTO


// Definition of macro-like inlined functions
#define mu_inline static inline __attribute__((always_inline))

// Definition of non-returning functions
#define mu_noreturn __attribute__((noreturn)) void

// Definition of pure functions
#define mu_pure __attribute__((const))

// Definition of unused variables
#define mu_unused __attribute__((unused))

// Definition of constructor functions
#define mu_constructor static __attribute__((constructor))

// Definition of alignment for Mu types
#define mu_aligned __attribute__((aligned(8)))

// Builtins for the likelyness of branches
#define mu_likely(x) __builtin_expect((x) != 0, 1)
#define mu_unlikely(x) __builtin_expect((x) != 0, 0)

// Builtin for an unreachable point in code
#define mu_unreachable __builtin_unreachable()

// Builtin for the next power of two
#ifdef MU32
#define mu_npw2(x) (32 - __builtin_clz((x)-1))
#else
#define mu_npw2(x) (64 - __builtin_clzl((x)-1))
#endif

// Definition of Mu specific assert function
#ifdef MU_DEBUG
#include <assert.h>
#define mu_assert(x) assert(x)
#else
#define mu_assert(x)
#endif


#endif
