#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];

char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

void add_history(char *unused) {}

#else
#include <editline/readline.h>
#endif

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

// add SYM and sexpr as possible lval types
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR, };

typedef lval *(*lbuiltin)(lenv *, lval *);

// declare l value struct
struct lval {
  int type;
  
  long num;
  char *err;
  char *sym;
  lbuiltin fun;

  int count;
  lval **cell;
};

// create a new number type lval
lval *lval_num(long x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

// create a new error type lval
lval *lval_err(char *fmt, ...) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  v->err = malloc(512);

  vsnprintf(v->err, 511, fmt, va);

  v->err = realloc(v->err, strlen(v->err)+1);

  va_end(va);
  
  return v;
}

lval *lval_sym(char *s) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval *lval_sexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval *lval_qexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
         lval_num(x) : lval_err("Invalid number");
}

lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval *lval_read(mpc_ast_t *t) {

  // if symbol or number return conversion to that type
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  // if root (>) or sexpr then create empty list
  lval *x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }
  
  // fill this list with any valid expression contained within
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

lval *lval_fun(lbuiltin func) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = func;
  return v;
}

lval *lval_copy(lval *v) {
  lval *x = malloc(sizeof(lval));
  x->type = v->type;
  
  switch (v->type) {
    // copy functions and numbers directly
  case LVAL_FUN: x->fun = v->fun; break;
  case LVAL_NUM: x->num = v->num; break;

    // copy string using malloc and strcpy
  case LVAL_ERR:
    x->err = malloc(strlen(v->err) + 1);
    strcpy(x->err, v->err); break;

  case LVAL_SYM:
    x->sym = malloc(strlen(v->sym) + 1);
    strcpy(x->sym, v->sym); break;

    // copy lists by copying each sub-expression
  case LVAL_SEXPR:
  case LVAL_QEXPR:
    x->count = v->count;
    x->cell = malloc(sizeof(lval *) * x->count);
    for (int i = 0; i < x->count; i++) {
      x->cell[i] = lval_copy(v->cell[i]);
    }
    break;
  }
  return x;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM: break;
  case LVAL_FUN: break;
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;

  case LVAL_QEXPR:
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      lval_del(v->cell[i]);
    }

    free(v->cell);
    break;
  }
  free(v);
}

struct lenv {
  int count;
  char **syms;
  lval **vals;
};

lenv *lenv_new(void) {
  lenv *e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv *e) {
  for (int i = 0; i < e->count; i++) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval *lenv_get(lenv *e, lval *k) {
  // iterate over all items in environment
  for (int i = 0; i < e->count; i++) {
    // check if the stored string matches the symbol string
    // if it does return a copy of the value
    if (strcmp(e->syms[i], k->sym) == 0) {
      return lval_copy(e->vals[i]);
    }
  }
  // if no symbol found return error
  return lval_err("Unbound symbol", k->sym);
}

void lenv_put(lenv *e, lval *k, lval *v) {
  // iterate over all items in environment
  // this is to see if variable allready exists
  for (int i = 0; i < e->count; i++) {
    // if variable is found delete item at that position
    // and replace with variable supplied by user
    if (strcmp(e->syms[i], k->sym) == 0) {
      lval_del(e->vals[i]);
      e->vals[i] = lval_copy(v);
      return;
    }
  }

  // if no existing entry found allocate space for new entry
  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  // copy contents of lval and symbol string into new location
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count-1], k->sym);
}

lval *lval_eval_sexpr(lenv *e, lval *v);

lval *lval_eval(lenv *e, lval *v);

lval *lval_take(lval *v, int i);

lval *lval_pop(lval *v, int i);

void lval_print(lval *v);

void lval_expr_print(lval *v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {

    // print value contained within
    lval_print(v->cell[i]);

    // dont't print trailing space if last element
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval *v) {
  switch (v->type) {
  case LVAL_FUN: printf("<funtion>"); break;
  case LVAL_NUM: printf("%li", v->num); break;
  case LVAL_ERR: printf("Error: %s", v->err); break;
  case LVAL_SYM: printf("%s", v->sym); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

lval *lval_pop(lval *v, int i) {
  // dint the item at i
  lval *x = v->cell[i];

  // shift memory after the item at "i" over the top
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));

  // decrese the count of items in the list
  v->count--;

  // reallocate the memory used
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  return x;
}

lval *lval_take(lval *v, int i) {
  lval *x = lval_pop(v, i);
  lval_del(v);
  return x;
}

char *ltype_names(int t) {
  switch (t) {
  case LVAL_FUN: return "Function";
  case LVAL_NUM: return "Number";
  case LVAL_ERR: return "Error";
  case LVAL_SYM: return "Symbol";
  case LVAL_SEXPR: return "S-Expression";
  case LVAL_QEXPR: return "Q-Expression";
  default: return "Unknown";
  }
}

#define LASSERT(args, cond, fmt, ...)                                          \
  if (!(cond)) {                                                               \
    lval *err = lval_err(fmt, ##__VA_ARGS__);                                  \
    lval_del(args);                                                            \
    return err;                                                                \
  }

#define LASSERT_TYPE(func, args, index, expect)                                \
LASSERT(args, args->cell[index]->type == expect, \
"Function '%s' passed incorrect type for argument %i. Got %s, expected %s.",     \
func, index, ltype_names(args->cell[index]->type), ltype_names(expect))

#define LASSERT_NUM(func, args, num)                                           \
LASSERT(args, args->count == num, \
  "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
func, args->count, num)

#define LASSERT_NOT_EMPTY(func, args, index)                                   \
LASSERT(args, args->cell[index]->count != 0,                                   \
"Function '%s' passed {} for argument %i.", func, index)

lval *builtin_op(lenv *e, lval *a, char *op) {

  // ensure all arguments are numbers
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE(op, a, i, LVAL_NUM);
  }

  // pop the first element
  lval *x = lval_pop(a, 0);

  // if non arguments and sub then perform unary negation
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }

  // while there is still elements remaining
  while (a->count > 0) {

    // pop the next element
    lval *y = lval_pop(a, 0);

    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("Division by zero!"); break;
      }
      x->num /= y->num;
    }
    lval_del(y);
  }
  lval_del(a); return x;
}

lval *builtin_add(lenv *e, lval *a) {
  return builtin_op(e, a, "+");
}

lval *builtin_sub(lenv *e, lval *a) {
  return builtin_op(e, a, "-");
}

lval *builtin_mul(lenv *e, lval *a) {
  return builtin_op(e, a, "*");
}

lval *builtin_div(lenv *e, lval *a) {
  return builtin_op(e, a, "/");
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func) {
  lval *k = lval_sym(name);
  lval *v = lval_fun(func);
  lenv_put(e, k, v);
  lval_del(k); lval_del(v);
}

lval *builtin_head(lenv *e, lval *a) {
  // check error conditions
  LASSERT_NUM("head", a, 1);
  
  LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
  
  LASSERT_NOT_EMPTY("head", a, 0);
  // otherwise take first argument
  lval *v = lval_take(a, 0);

  // Delete all elements that are not head and return
  while (v->count > 1) { lval_del(lval_pop(v, 1));}
  return v;
}

lval *builtin_tail(lenv *e, lval *a) {
  // check error conditions
  LASSERT_NUM("tail", a, 1);
  
  LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
  
  LASSERT_NOT_EMPTY("tail", a, 0);
  
  // take first argument
  lval *v = lval_take(a, 0);

  // delete first element and return
  lval_del(lval_pop(v, 0));
  return v;
}

lval *builtin_list(lenv *e, lval *a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval *builtin_eval(lenv *e,lval *a) {
  LASSERT_NUM("eval", a, 1);
  
  LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);
  
  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval *lval_join(lval *x, lval *y);

lval *builtin_join(lenv *e, lval *a) {
  for (int i = 0; i < a->count; i++) {
    LASSERT_TYPE("join", a, i, LVAL_QEXPR);
  }

  lval *x = lval_pop(a, 0);

  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval *lval_join(lval *x, lval *y) {
  // for each cell in 'y' add it to 'x'
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  // delete the empty 'y' and return 'x'
  lval_del(y);
  return x;
}

lval *builtin_def(lenv *e, lval *a) {
  LASSERT_TYPE("def", a, 0, LVAL_QEXPR);
  
  // first argument is symbol list
  lval *syms = a->cell[0];

  // ensure all elements of first list are symbols
  for (int i = 0; i < syms->count; i++) {
    LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
            "Function 'def' cannot define non-symbol"
            "GOT %s, Expected %s",
            ltype_names(syms->cell[i]->type), ltype_names(LVAL_SYM));
  }

  // check correct number of symbols and values
  LASSERT(a, syms->count == a->count - 1,
          "Function 'def' passed too many arguments for symbol.  "
          "Got %i Expected %i.",
          syms->count, a->count-1);

  // assign copies of value to symbols
  for (int i = 0; i < syms->count; i++) {
    lenv_put(e, syms->cell[i], a->cell[i+1]);
  }

  lval_del(a);
  return lval_sexpr();
}

void lenv_add_builtins(lenv *e) {
  // variable fuctions
  lenv_add_builtin(e, "def",  builtin_def);
  
  // list functions
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_head);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  
  // mathemathical functions
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_sub);
}

lval *lval_eval_sexpr(lenv *e, lval *v) {

  // evaluate children
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e ,v->cell[i]);
  }

  // error checking
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  // empty expression
  if (v->count == 0) { return v; }

  // single expression
  if (v->count == 1) { return lval_take(v, 0); }

  // TODO!!!
  // ensure first element is symbol
  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval *err = lval_err(
                         "S-expression starts with incorrect type. "
                         "Got %s, Expected %s",
                         ltype_names(f->type), ltype_names(LVAL_FUN));
    lval_del(f);
    lval_del(f);
    return err;
  }

  // call builtin with operator
  lval *result = f->fun(e, v); //builtin(e, v, f->sym);
  lval_del(f);
  return result;
}

 lval *lval_eval(lenv *e, lval *v) { 
   if (v->type == LVAL_SYM) {
     lval *x = lenv_get(e, v);
     lval_del(v);
     return x;
   } 
   if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
   // all other lval types remain the smae
   return v;
 }

// print an "lval" followed by a new line
void lval_println(lval *v) { lval_print(v); putchar('\n'); }

int main(int argc, char *argv[]) {

  // create some parsers
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  // define them with the following language
  mpca_lang(MPCA_LANG_DEFAULT,
            "                                                \
      number   : /-?[0-9]+/ ;                                \
      symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;          \
      sexpr    : '(' <expr>* ')' ;                           \
      qexpr    : '{' <expr>* '}' ;                           \
      expr     : <number> | <symbol> | <sexpr> | <qexpr> ;   \
      lispy    : /^/ <expr>* /$/ ;                           \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  // print version and exit info
  puts("Lispy version 0.0.0.0.7");
  puts("Press ctrl+c to exit");

  lenv *e = lenv_new();
  lenv_add_builtins(e);

  while (1) {

    // output the prompt
    char *input = readline("Lispy >");

    // add input to history
    add_history(input);

    // attempt to parse the user input
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval *x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }   
    
    // free input // memory dump
    free(input);
  }

  lenv_del(e);
  
  // undefine and clean up the parsers
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  return 0;
}
