#include "vparse.h"

#include "vlex.h"
#include "tbl.h"

#include <assert.h>
#include <stdio.h>


// Macro for accepting the current token and continuing
static inline vstate_t *vnext(vstate_t *vs) {
    vs->tok = vlex(vs);
    return vs;
}

// Parsing error handling
#define vunexpected(vs) _vunexpected(vs, __FUNCTION__)
__attribute__((noreturn))
static void _vunexpected(vstate_t *vs, const char *fn) {
    //printf("unexpected '%c' (%d) in %s\n", vs->tok, vs->tok, fn);
    printf("unexpected (%d) in %s\n", vs->tok, fn);
    assert(false); // TODO make this throw actual messages
}

// Expect a token or fail
static void vexpect(vstate_t *vs, vtok_t tok) {
    if (vs->tok != tok) {
        //printf("expected '%c' (%d) not '%c' (%d)\n", tok, tok, vs->tok, vs->tok);
        printf("expected (%d) not (%d)\n", tok, vs->tok);
        assert(false); // TODO
    }

    vnext(vs);
}


// Different encoding calls
static varg_t vaccvar(vstate_t *vs) {
    varg_t arg;
    var_t index = tbl_lookup(vs->vars, vs->val);

    if (var_isnil(index)) {
        arg = vs->vars->len;
        tbl_assign(vs->vars, vs->val, vnum(arg));
    } else {
        arg = (varg_t)var_num(index);
    }

    return arg;
}



static void venc(vstate_t *vs, vop_t op) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op, 0);
}

static void vencarg(vstate_t *vs, vop_t op, varg_t arg) {
    vs->ins += vs->encode(&vs->bcode[vs->ins], op | VOP_ARG, arg);
}

static void vencvar(vstate_t *vs) {
    vencarg(vs, VVAR, vaccvar(vs));
}


static vins_t vsized(vstate_t *vs, vop_t op) {
    return vcount(0, op, 0);
}

static vins_t vsizearg(vstate_t *vs, vop_t op, varg_t arg) {
    return vcount(0, op | VOP_ARG, arg);
}

static vins_t vsizevar(vstate_t *vs) {
    return vsizearg(vs, VVAR, vaccvar(vs));
}


static vins_t vinsert(vstate_t *vs, vop_t op, vins_t in) {
    return vs->encode(&vs->bcode[in], op, 0);
}

static vins_t vinsertarg(vstate_t *vs, vop_t op, varg_t arg, vins_t in) {
    return vs->encode(&vs->bcode[in], op | VOP_ARG, arg);
}

static vins_t vinsertvar(vstate_t *vs, vins_t in) {
    return vinsertarg(vs, VVAR, vaccvar(vs), in);
}



static void vpatch(vstate_t *vs) {
    tbl_for(k, v, vs->ltbl, {
        vinsertarg(vs, VJUMP,
            vs->ins - (v.data+vsizearg(vs, VJUMP, 0)), v.data);
    });

    tbl_dec(vs->ltbl);
    vs->ltbl = 0;
}

static void venlarge(vstate_t *vs, vins_t in, vins_t count) {
    if (vs->bcode) {
        memmove(&vs->bcode[in] + count,
                &vs->bcode[in], 
                vs->ins - in);
    }

    vs->ins += count;
}




// Parser for V's grammar
static void vp_value(vstate_t *vs);
static void vp_primarydot(vstate_t *vs);
static void vp_primaryop(vstate_t *vs);
static void vp_primary(vstate_t *vs);
static void vp_table(vstate_t *vs);
static void vp_tabassign(vstate_t *vs);
static void vp_tabident(vstate_t *vs);
static void vp_tabfollow(vstate_t *vs);
static void vp_tabentry(vstate_t *vs);
static void vp_explist(vstate_t *vs);
static void vp_explet(vstate_t *vs);
static void vp_expassign(vstate_t *vs);
static void vp_expfollow(vstate_t *vs);
static void vp_expression(vstate_t *vs);


static void vp_value(vstate_t *vs) {
    vp_primary(vs);

    if (vs->indirect)
        venc(vs, VLOOKUP);
}


static void vp_primaryif(vstate_t *vs) {
    vins_t ifins = vs->ins;
    vs->ins += vsizearg(vs, VJFALSE, 0);
    vp_value(vs);

    switch (vs->tok) {
        case VT_ELSE:   {   vins_t elins = vs->ins;
                            vs->ins += vsizearg(vs, VJUMP, 0);
                            vp_value(vnext(vs));
                            vinsertarg(vs, VJFALSE,
                                (elins+vsizearg(vs, VJUMP, 0)) -
                                (ifins+vsizearg(vs, VJFALSE, 0)), ifins);
                            vinsertarg(vs, VJUMP,
                                vs->ins - (elins+vsizearg(vs, VJUMP, 0)), elins);
                        }
                        return;

        default:        vencarg(vs, VJUMP, vsized(vs, VNIL));
                        vinsertarg(vs, VJFALSE,
                            vs->ins - (ifins+vsizearg(vs, VJFALSE, 0)), ifins);
                        venc(vs, VNIL);
                        return;
    }
}

static void vp_primarydot(vstate_t *vs) {
    switch (vs->tok) {
        case VT_IDENT:  vencvar(vs);
                        vs->indirect = true;
                        return vp_primaryop(vnext(vs));

        default:        vunexpected(vs);
    }
}

static void vp_primaryop(vstate_t *vs) {
    switch (vs->tok) {
        case VT_DOT:    if (vs->indirect) venc(vs, VLOOKUP);
                        return vp_primarydot(vnext(vs));

        case '[':       if (vs->indirect) venc(vs, VLOOKUP);
                        vs->paren++;
                        vp_value(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ']');
                        vs->indirect = true;
                        return vp_primaryop(vs);

        case '(':       if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VTBL);
                        vs->paren++;
                        vp_table(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ')');
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_primaryop(vs);

        case VT_OP:     if (vs->prec <= vs->nprec) return;
                        if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VADD);
                        {   uint32_t opstate = vs->opstate;
                            venlarge(vs, vs->opins, vsizevar(vs)
                                                  + vsized(vs, VTBL));
                            vs->opins += vinsertvar(vs, vs->opins);
                            vs->opins += vinsert(vs, VTBL, vs->opins);
                            vs->prec = vs->nprec;
                            vp_value(vnext(vs));
                            vs->opstate = opstate;
                        }
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_primaryop(vs);

        default:        return;
    }
}

static void vp_primary(vstate_t *vs) {
    vs->opins = vs->ins;

    switch (vs->tok) {
        case VT_IDENT:  venc(vs, VSCOPE);
                        vencvar(vs);
                        vs->indirect = true;
                        return vp_primaryop(vnext(vs));

        case VT_NIL:    venc(vs, VNIL);
                        vs->indirect = false;
                        return vp_primaryop(vnext(vs));

        case VT_NUM:
        case VT_STR:    vencvar(vs);
                        vs->indirect = false;
                        return vp_primaryop(vnext(vs));

        case '[':       venc(vs, VTBL);
                        vs->paren++;
                        vp_table(vnext(vs));
                        vs->paren--;
                        vexpect(vs, ']');
                        vs->indirect = false;
                        return vp_primaryop(vs);

        case '(':       vs->paren++;
                        {   uint32_t opstate = vs->opstate;
                            vs->prec = -1;
                            vp_primary(vnext(vs));
                            vs->opstate = opstate;
                        }
                        vs->paren--;
                        vexpect(vs, ')');
                        return vp_primaryop(vs);

        case VT_OP:     vencvar(vs);
                        venc(vs, VTBL);
                        {   uint32_t opstate = vs->opstate;
                            vs->prec = vs->nprec;
                            vp_value(vnext(vs));
                            vs->opstate = opstate;
                        }
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        vs->indirect = false;
                        return vp_primaryop(vs);

        case VT_IF:     vexpect(vnext(vs), '(');
                        vp_value(vs);
                        vexpect(vs, ')');
                        vp_primaryif(vs);
                        vs->indirect = false;
                        return vp_primaryop(vs);

        default:        vunexpected(vs);
    }
}


static void vp_table(vstate_t *vs) {
    vp_tabentry(vs);
    return vp_tabfollow(vs);
}

static void vp_tabassign(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SET:    vp_value(vnext(vs));
                        venc(vs, VINSERT);
                        return;

        default:        venc(vs, VADD);
                        return;
    }
}

static void vp_tabident(vstate_t *vs) {
    int ins = vs->ins;
    vencvar(vs);
    vnext(vs);

    switch (vs->tok) {
        case VT_SET:    return vp_tabassign(vs);

        default:        venlarge(vs, ins, vsized(vs, VSCOPE));
                        vinsert(vs, VSCOPE, ins);
                        vs->indirect = true;
                        vp_primaryop(vs);
                        if (vs->indirect) venc(vs, VLOOKUP);
                        return vp_tabassign(vs);
    }
}

static void vp_tabfollow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    return vp_table(vnext(vs));

        default:        return;
    }
}

static void vp_tabentry(vstate_t *vs) {
    switch (vs->tok) {
        case VT_IDENT:  return vp_tabident(vs);

        case VT_NUM:
        case VT_STR:
        case '[':
        case '(':       
        case VT_OP:     vp_value(vs);
                        return vp_tabassign(vs);

        default:        return;
    }
}


static void vp_explist(vstate_t *vs) {
    vp_expression(vs);
    return vp_expfollow(vs);
}

static void vp_explet(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SET:    if (!vs->indirect) vunexpected(vs);
                        vp_value(vnext(vs));
                        venc(vs, VINSERT);
                        venc(vs, VDROP);
                        return;

        default:        vunexpected(vs);
    }
}

static void vp_expassign(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SET:    if (!vs->indirect) vunexpected(vs);
                        vp_value(vnext(vs));
                        venc(vs, VASSIGN);
                        return;

        case VT_OPSET:  if (!vs->indirect) vunexpected(vs);
                        vencvar(vs);
                        venc(vs, VTBL);
                        vencarg(vs, VDUP, 3);
                        vencarg(vs, VDUP, 3);
                        venc(vs, VLOOKUP);
                        venc(vs, VADD);
                        vp_value(vnext(vs));
                        venc(vs, VADD);
                        venc(vs, VCALL);
                        venc(vs, VASSIGN);
                        return;
                        
        default:        if (vs->indirect) venc(vs, VLOOKUP);
                        venc(vs, VDROP);
                        return;
    }
}

static void vp_expif(vstate_t *vs) {
    vins_t ifins = vs->ins;
    vs->ins += vsizearg(vs, VJFALSE, 0);
    vp_expression(vs);

    switch (vs->tok) {
        case VT_ELSE:   {   vins_t elins = vs->ins;
                            vs->ins += vsizearg(vs, VJUMP, 0);
                            vp_expression(vnext(vs));
                            vinsertarg(vs, VJFALSE, 
                                (elins+vsizearg(vs, VJUMP, 0)) -
                                (ifins+vsizearg(vs, VJFALSE, 0)), ifins);
                            vinsertarg(vs, VJUMP,
                                vs->ins - (elins+vsizearg(vs, VJUMP, 0)), elins);
                        }
                        return;
                                

        default:        vinsertarg(vs, VJFALSE, 
                            vs->ins - (ifins+vsizearg(vs, VJFALSE, 0)), ifins);
                        return;
    }
}

static void vp_expwhile(vstate_t *vs) {
    vins_t whins = vs->ins;
    vs->ins += vsizearg(vs, VJFALSE, 0);
    vp_expression(vs);

    vinsertarg(vs, VJFALSE,
        (vs->ins+vsizearg(vs, VJUMP, 0)) -
        (whins+vsizearg(vs, VJFALSE, 0)), whins);
    vencarg(vs, VJUMP,
        vs->lins - (vs->ins+vsizearg(vs, VJUMP, 0)));

    switch (vs->tok) {
        case VT_ELSE:   return vp_expression(vnext(vs));
                            
        default:        return;
    }
}

static void vp_expfollow(vstate_t *vs) {
    switch (vs->tok) {
        case VT_SEP:    return vp_explist(vnext(vs));

        default:        return;
    }
}

static void vp_expression(vstate_t *vs) {
    switch (vs->tok) {
        case '{':       vp_explist(vnext(vs));
                        vexpect(vs, '}');
                        return;

        case VT_RETURN: vp_value(vnext(vs));
                        venc(vs, VRET);
                        return;

        case VT_LET:    vp_primary(vnext(vs));
                        return vp_explet(vs);

        case VT_IF:     vexpect(vnext(vs), '(');
                        vp_value(vs);
                        vexpect(vs, ')');
                        return vp_expif(vs);

        case VT_WHILE:  {   uint64_t lstate = vs->lstate;
                            vs->lins = vs->ins;
                            vs->ltbl = tbl_create(0);
                            vexpect(vnext(vs), '(');
                            vp_value(vs);
                            vexpect(vs, ')');
                            vp_expwhile(vs);
                            vpatch(vs);
                            vs->lstate = lstate;
                        }
                        return;

        case VT_CONT:   if (!vs->ltbl) vunexpected(vs);
                        vnext(vs);
                        vencarg(vs, VJUMP,
                            vs->lins - (vs->ins+vsizearg(vs, VJUMP, 0)));
                        return;

        case VT_BREAK:  if (!vs->ltbl) vunexpected(vs);
                        vnext(vs);
                        tbl_add(vs->ltbl, vraw(vs->ins));
                        vs->ins += vsizearg(vs, VJUMP, 0);
                        return;

        case VT_IDENT:
        case VT_NUM:
        case VT_STR:
        case '[':  
        case '(': 
        case VT_OP:     vp_primary(vs);
                        return vp_expassign(vs);

        default:        return;
    }
}



// Parses V source code and evaluates the result
vins_t vparse(vstate_t *vs) {
    vs->ins = 0;
    vs->paren = 0;
    vs->prec = -1;
    vs->ltbl = 0;
    vs->tok = vlex(vs);

    vp_explist(vs);

    if (vs->tok != 0)
        vunexpected(vs);

    venc(vs, VRETN | VOP_END);
    return vs->ins;
}

