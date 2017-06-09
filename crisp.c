

#include "mpc.h"

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

/* Forward declaration*/
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Add SYM and SEXPR as possible lval types */
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM,
		LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };

typedef lval*(*lbuiltin)(lenv*, lval*);


struct lval {
  int type;
  
  //Basic types
  long num;
  char* err;
  char* sym;
  
  
  //
  lbuiltin builtin;
  lenv* env;
  lval* formals;
  lval* body;
  
  /* Count and Pointer to a list of "lval*"; */
  int count;
  struct lval** cell;
};

lval* lval_lambda(lval* formals, lval* body) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	
	v->builtin = NULL;
	
	v->env = lenv_new();
	
	v->formals = formals;
	v->body = body;
	return v;
}

lval* lval_fun(lbuiltin func){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->fun = func;
	return v;
}

/* Construct a pointer to a new Number lval */ 
lval* lval_num(long x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* Construct a pointer to a new Error lval */ 
lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  
  va_list va;
  va_start(va, fmt);
  
  //Allocate 512 bytes of space.
  v->err = malloc(512);
  
  //Print the errorstring with a maxiumum of 511 characters.
  vsnprintf(v->err, 511, fmt, va);
  
  //Reallocate to number of bytes actually used.
  v->err = realloc(v->err, strlen(v->err)+1);
  
  //Cleanup
  va_end(va);
  
  return v;
}

/* Construct a pointer to a new Symbol lval */ 
lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* A pointer to a new empty Sexpr lval */
lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

//Construct pointer to empty Qexpr lval
lval* lval_qexpr(void){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v-> count = 0;
	v-> cell = NULL;
	return v;
}


void lval_del(lval* v) {

  switch (v->type) {
    /* Do nothing special for number type */
    case LVAL_NUM: break;
    
    /* For Err or Sym free the string data */
    case LVAL_ERR: free(v->err); break;
    case LVAL_SYM: free(v->sym); break;
	
	case LVAL_FUN:
		if(!v->builtin) {
			lenv_del(v->env);
			lval_del(v->formals);
			lval_del(v->body);
		}
	break;
	
	/* For function type */
	case LVAL_FUN: break;
    
    /* If Sexpr then delete all elements inside */
    case LVAL_SEXPR:
      for (int i = 0; i < v->count; i++) {
        lval_del(v->cell[i]);
      }
      /* Also free the memory allocated to contain the pointers */
      free(v->cell);
    break;
	
	case LVAL_QEXPR:
		for (int i = 0; i < v->count; i++) {
			lval_del(v->cell[i]);
		}
		free(v->cell);
  }
  
  /* Free the memory allocated for the "lval" struct itself */
  free(v);
}

lval* lval_add(lval* v, lval* x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

lval* lval_pop(lval* v, int i) {
  /* Find the item at "i" */
  lval* x = v->cell[i];
  
  /* Shift memory after the item at "i" over the top */
  memmove(&v->cell[i], &v->cell[i+1],
    sizeof(lval*) * (v->count-i-1));
  
  /* Decrease the count of items in the list */
  v->count--;
  
  /* Reallocate the memory used */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

/* Some pre definitions. */
lval* lval_eval(lenv* e, lval* v);

void lval_print(lval* v);


void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    
    /* Print Value contained within */
    lval_print(v->cell[i]);
    
    /* Don't print trailing space if last element */
    if (i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:   printf("%li", v->num); break;
    case LVAL_ERR:   printf("Error: %s", v->err); break;
    case LVAL_SYM:   printf("%s", v->sym); break;
    case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
	case LVAL_QEXPR: lval_expr_print(v,'{', '}'); break;
	case LVAL_FUN: 
		if (v->builtin) {
			printf("<function>");
		} else {
			printf("(\\ "); lval_print(v->formals);
			putchar(' '); lval_print(v->body); putchar(')');}
		}
	break;
  }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_copy(lval* v) {
	lval* x = malloc(sizeof(lval));
	x->type = v->type;
	
	switch (v->type){
		
		case LVAL_FUN: x->fun = v->fun; break;
		case LVAL_NUM: x->num = v->num; break;
		
		case LVAL_FUN:
			if(v->builtin) {
				x->builtin = v->builtin;
			} else {
				x->builtin = NULL;
				x->env = lenv_copy(v->env);
				x->formals = lval_copy(v->formals);
				x->body = lval_copy(v->body);
			}
		break;
		
		case LVAL_ERR:
			x->err = malloc(strlen(v->err) + 1);
			strcpy(x->err, v->err); break;
			
		case LVAL_SYM:
			x->sym = malloc(strlen(v->sym) + 1);
			strcpy(x->err, v->err); break;
			
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			x-> count = v->count;
			x->cell = malloc(sizeof(lval*) * x->count);
			for(int i = 0; i < x-> count; i++){
				x->cell[i] = lval_copy(v-> cell[i]);
			}
		break;
	}
	return x;	
}

/* ENVIRONMENT PART */

//Environment struct
struct lenv{
	int count;
	char** syms;
	lval** vals;
};

/*Create environment*/
lenv* lenv_new(void) {
	lenv* e = malloc(sizeof(lenv));
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

//Delete environment
void lenv_del(lenv* e) {
	for (int i = 0; i < e->count; i++){
	free(e->syms[i]);
	lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

//Get a variable from an environment
lval* lenv_get(lenv* e, lval* k) {
	
	//iterate over all items in an environment
	for(int i = 0; i < e-> count; i++){
		/* Check if stored string matches the symbol string
		If it does, return a copy of the value*/
		if (strcmp(e->syms[i],k->sym) == 0) {
			return lval_copy(e->vals[i]);
		}
	}
	//If no copy found
	return lval_err("Unbound Symbol! '%s'", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v){
		//Iterate over all items in environment
		//to see if variable already exists
		for(int i = 0; i < e-> count; i++){
			/* If variable is already found
			delete it at that position and replace
			with the new variable*/
			if(strcmp(e->syms[i],k->sym) == 0) {
				lval_del(e->vals[i]);
				e->vals[i] = lval_copy(v);
				return;
			}
		}
		
		
		/* If no variable is found allocate space for new memory*/
		e->count++;
		e->vals = realloc(e->vals,sizeof(lval*) * e->count);
		e->syms = realloc(e->syms,sizeof(char*) * e->count);
		
		/* Copy contents of lval and symbol string into new location */
		e->vals[e->count-1] = lval_copy(v);
		e->syms[e->count-1] = malloc(strlen(k->sym)+1);
		strcpy(e->syms[e->count-1],k->sym);
}

/* A pre declaration */
lval* lval_eval_sexpr(lenv* e, lval* v);

/* Variable evaluation */
lval* lval_eval(lenv* e, lval* v){
	if(v->type == LVAL_SYM){
		lval* x = lenv_get(e,v);
		lval_del(v);
		return x;
	}
	if(v->type == LVAL_SEXPR) {return lval_eval_sexpr(e,v);}
	return v;
}

lval* builtin_lambda(lenv* e, lval* a) {
  /* Check Two arguments, each of which are Q-Expressions */
  LASSERT_NUM("\\", a, 2);
  LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
  LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

  /* Check first Q-Expression contains only Symbols */
  for (int i = 0; i < a->cell[0]->count; i++) {
	  if(a->cell[0]->cell[i]->type != LVAL_SYM){
		  return lval_err("Cannot define non-symbol. Got %s, Expected %s.",
      ltype_name(a->cell[0]->cell[i]->type),ltype_name(LVAL_SYM))
	  }
  }

  /* Pop first two arguments and pass them to lval_lambda */
  lval* formals = lval_pop(a, 0);
  lval* body = lval_pop(a, 0);
  lval_del(a);

  return lval_lambda(formals, body);
}

lval* builtin_def(lenv* e, lval* a) {
	if(a->cell[0]->type != LVAL_QEXPR){
		return lval_err("Function 'def' passed incorrect type!");
	}
	
	lval* syms = a->cell[0];
	
	for(int i = 0; i < syms->count; i++){
		if(syms->cell[i]->type != LVAL_SYM){
			return lval_err("Function 'def' cannot define non-symbol!");
		}
	}
	
	//if(syms->count != a->count-1){
	//	return lval_err("Function 'def' passed too many arguments for symbols. Got %i, Expected %i", syms->count, a->count-1);
	//}
	
	for(int i = 0; i < syms-> count; i++) {
		lenv_put(e,syms->cell[i], a->cell[i+1]);
	}
	
	lval_del(a);
	return lval_sexpr();
}

char* ltype_name(int t);

lval* builtin_head(lenv* e, lval* a){
	
	//Check error conditions
	if(a->count !=1){
		lval_del(a);
		return lval_err("Function 'head' has invalid number of arguments. Got %i, Expected %i.", a->count, 1);
		
	}
	if(a->cell[0]-> type !=LVAL_QEXPR){
	lval_del(a);
	return lval_err("Function 'head' passed incorrect type for argument 0. "
					"Got %s, Expected %s", ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
	}
	
	if(a->cell[0]-> count == 0) {
		lval_del(a);
		return lval_err("Function 'head' passed {}!");
	}
	
	//If no errors occur.
	lval* v = lval_take(a,0);
	
	while(v->count > 1){ lval_del(lval_pop(v,1));}
	return v;
	
}

lval* builtin_tail(lenv* e, lval* a){
	//Check error conditions
	if(a->count !=1){
		lval_del(a);
		return lval_err("Function 'head' has invalid number of arguments.");
		
	}
	if(a->cell[0]-> type !=LVAL_QEXPR){
	lval_del(a);
	return lval_err("Function head passed incorrect type");
	}
	
	if(a->cell[0]-> count == 0) {
		lval_del(a);
		return lval_err("Function 'head' passed {}!");
	}
	
	lval* v = lval_take(a,0);
	lval_del(lval_pop(v,0));
	return v;
}

lval* builtin_list(lenv* e, lval* a){
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_eval(lenv* e, lval* a){
	if(a->count != 1){
		lval_del(a);
		return lval_err("Function 'eval' passed wrong number of arguments");
	}
	if(a->cell[0]->type != LVAL_QEXPR){
		lval_del(a);
		return lval_err("Function 'eval' passed incorrect type!");
	}
	
	lval* x = lval_take(a,0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y){
	
	while(y-> count){
		x = lval_add(x,lval_pop(y,0));
	}
	
	lval_del(y);
	return x;
}

lval* builtin_join(lenv* e, lval* a) {
	for(int i = 0; i < a-> count; i++){
		if(a->cell[i]->type != LVAL_QEXPR){
			return lval_err("Function 'join' passed incorrect type");
		}
	}
	
	lval* x = lval_pop(a,0);
	
	while(a->count) {
		x = lval_join(x, lval_pop(a,0));
	}
	
	lval_del(a);
	return x;
}

lval* builtin_op(lenv* e, lval* a, char* op) {
  
  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_del(a);
      return lval_err("Cannot operate on non-number!");
    }
  }
  
  /* Pop the first element */
  lval* x = lval_pop(a, 0);
  
  /* If no arguments and sub then perform unary negation */
  if ((strcmp(op, "-") == 0) && a->count == 0) {
    x->num = -x->num;
  }
  
  /* While there are still elements remaining */
  while (a->count > 0) {
  
    /* Pop the next element */
    lval* y = lval_pop(a, 0);
    
    /* Perform operation */
    if (strcmp(op, "+") == 0) { x->num += y->num; }
    if (strcmp(op, "-") == 0) { x->num -= y->num; }
    if (strcmp(op, "*") == 0) { x->num *= y->num; }
    if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x); lval_del(y);
        x = lval_err("Division By Zero.");
        break;
      }
      x->num /= y->num;
    }
    
    /* Delete element now finished with */
    lval_del(y);
  }
  
  /* Delete input expression and return result */
  lval_del(a);
  return x;
}

//lval* builtin(lval* a, char* func){
	//	if(strstr("+-*/", func)) {return builtin_op(a,func);}
	//if(strcmp("head", func) == 0) {return builtin_head(a);}
	//if(strcmp("tail", func) == 0) {return builtin_tail(a);}
	//if(strcmp("list", func) == 0) {return builtin_list(a);}
	//if(strcmp("eval", func) == 0) {return builtin_eval(a);}
	//if(strcmp("join", func) == 0) { return builtin_join(a); }
	//lval_del(a);
//	return lval_err("Unknown function!");
//} 



lval* lval_eval_sexpr(lenv* e, lval* v) {
  
  /* Evaluate Children */
  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(e, v->cell[i]);
  }
  
  /* Error Checking */
  for (int i = 0; i < v->count; i++) {
    if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }
  
  /* Empty Expression */
  if (v->count == 0) { return v; }
  
  /* Single Expression */
  if (v->count == 1) { return lval_take(v, 0); }
  
  /* Ensure First Element is Function */
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(f); lval_del(v);
    return lval_err("S-expression Does not start with symbol.");
  }
  
  /* Call builtin with operator */
  lval* result = f->fun(e,v);
  lval_del(f);
  return result;
}

lval* builtin_add(lenv* e, lval* a){
	return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a){
	return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a){
	return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a){
	return builtin_op(e, a, "/");
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
	lval* k = lval_sym(name);
	lval* v = lval_fun(func);
	lenv_put(e, k, v);
	lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
	//Variable functions
  lenv_add_builtin(e,"def",builtin_def);
  
  //lambda functions
  lenv_add_builtin(e, "\\", builtin_lambda);
	
	
  /* List Functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);

  /* Mathematical Functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ?
    lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  
  /* If Symbol or Number return conversion to that type */
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
  
  /* If root (>) or sexpr then create empty list */
  lval* x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); } 
  if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
  if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }
  
  /* Fill this list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
	if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }
  
  return x;
}

char* ltype_name(int t) {
  switch(t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}

int main(int argc, char** argv) {
  
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr  = mpc_new("sexpr");
  mpc_parser_t* Qexpr  = mpc_new("qexpr");
  mpc_parser_t* Expr   = mpc_new("expr");
  mpc_parser_t* Lispy  = mpc_new("lispy");
  
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                          			\
      number : /-?[0-9]+/ ;                    			\
      symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/; 		\
      sexpr  : '(' <expr>* ')' ;               			\
	  qexpr  : '{' <expr>* '}';				   			\
      expr   : <number> | <symbol> | <sexpr>| <qexpr> ; \
      lispy  : /^/ <expr>* /$/ ;               			\
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  puts("Lispy Version 0.0.0.0.6");
  puts("Press Ctrl+c to Exit\n");
  
  lenv* e = lenv_new();
  lenv_add_builtins(e);
  
  while (1) {
  
    char* input = readline("lispy> ");
    add_history(input);
    
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval* x = lval_eval(e, lval_read(r.output));
      lval_println(x);
      lval_del(x);
      mpc_ast_delete(r.output);
    } else {    
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
    
  }
  
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}

