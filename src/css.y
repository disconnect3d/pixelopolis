%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "css.h"

FILE *yyin;
struct Program* global_program;
int lines;
int chars;
int yylex(void);

struct Prop {
    char* name;
    struct Obj **objs;
};

int yyerror (char* err) {
    fprintf(stderr, "%d:%d %s\n", lines, chars, err);
    return 1;
}

int yywrap (void) {
    return 1;
}

char* concat_and_free(char* a, char* b) {
    size_t size = strlen(a) + strlen(b);
    char* out = malloc(sizeof(char) * (size + 1));
    strcpy(out, a);
    strcat(out, b);
    out[size] = 0;
    free(a);
    free(b);
    return out;
}

#define make_array(type, size, first_obj) \
    type **arr = malloc(sizeof(type*) * (size + 1)); \
    size_t i; \
    arr[0] = first_obj; \
    for(i=1; i < size; i++) { \
        arr[i] = NULL; \
    }

#define append_to_array(arr, size, obj) \
    size_t i; \
    for(i=0; i < size; i++) { \
        if (arr[i]) continue; \
        arr[i] = obj; \
        break; \
    }

struct Program* make_program(struct Rule **rules) {
    struct Program *program = malloc(sizeof(struct Program));
    char top[] = "top";
    char* name = malloc(sizeof(top));
    strcpy(name, top);
    program->name = name;
    program->rules = rules;

    return program;
}

struct Rule* make_rule(struct RuleSelector* selector, struct Prop **props) {
    struct Rule *rule = malloc(sizeof(struct Rule));
    struct HashMap *map_props = hash_make();

    struct Prop *prop;
    css_iter(prop, props) { // transform array to hashmap
        hash_set(map_props, prop->name, prop->objs, NULL);
        free(prop);
    }
    free(props);

    rule->selector = selector;
    rule->props = map_props;
    return rule;
}

struct Prop* make_prop(char *name, struct Obj **objs) {
    struct Prop *prop = malloc(sizeof(struct Prop));
    prop->name = name;
    prop->objs = objs;
    return prop;
}

char** _make_klasses() {
    make_array(char, KLASSES_SIZE, NULL);
    return arr;
}

struct RuleSelector* make_rule_selector() {
    struct RuleSelector *rule_selector = malloc(sizeof(struct RuleSelector));
    rule_selector->element = NULL;
    rule_selector->klasses = _make_klasses();
    rule_selector->pseudo_klass = NULL;
    return rule_selector;
}

struct Obj* make_obj_as_number(int value) {
    struct Obj* obj = malloc(sizeof(struct Obj));
    int *ptr = malloc(sizeof(int));
    *ptr = value;
    obj->type = OBJ_NUMBER;
    obj->value = (void*)ptr;
    return obj;
}

struct Obj* make_obj_as_percent(int value) {
    struct Obj* obj = malloc(sizeof(struct Obj));
    int *ptr = malloc(sizeof(int));
    *ptr = value;
    obj->type = OBJ_PERCENT;
    obj->value = (void*)ptr;
    return obj;
}

struct Obj* make_obj_as_string(char* string) {
    struct Obj* obj = malloc(sizeof(struct Obj));
    obj->type = OBJ_STRING;
    obj->value = (void*)string;
    return obj;
}

struct Obj* make_obj_as_variable(char* variable) {
    struct Obj* obj = malloc(sizeof(struct Obj));
    obj->type = OBJ_VARIABLE;
    obj->value = (void*)variable;
    return obj;
}

struct Obj* make_obj_as_rule(struct RuleSelector* rule_selector) {
    struct Obj* obj = malloc(sizeof(struct Obj));
    obj->type = OBJ_RULE;
    obj->value = (void*)rule_selector;
    return obj;
}

struct Obj* _make_obj_as_op(enum ObjType type, struct Obj* left, struct Obj* right) {
    struct PairObj* pair = malloc(sizeof(struct PairObj));
    pair->left = left;
    pair->right = right;

    struct Obj* obj = malloc(sizeof(struct Obj));
    obj->type = type;
    obj->value = (void*)pair;

    return obj;
}

#define APPEND_OP(name, op) \
    struct Obj* make_obj_as_ ## name(struct Obj* left, struct Obj* right) {\
        _make_obj_as_op(op, left, right);\
    }

APPEND_OP(add, OBJ_ADD)
APPEND_OP(sub, OBJ_SUB)
APPEND_OP(mul, OBJ_MUL)
APPEND_OP(div, OBJ_DIV)

struct Obj* make_obj_as_func(char* name, struct Obj** args) {
    size_t size = 0;
    while(args[size]) size++; // counter

    struct FuncObj* func = malloc(sizeof(struct FuncObj));
    func->name = name;
    func->args = args;
    func->args_size = size;

    struct Obj* obj = malloc(sizeof(struct Obj));
    obj->type = OBJ_FUNC;
    obj->value = (void*)func;
    return obj;
}

struct Obj* make_obj_as_noargs_func(char* name) {
    struct Obj** args = malloc(sizeof(struct Obj**));
    args[0] = NULL;
    return make_obj_as_func(name, args);
}

%}

%start program
%union {
    char sIndex; // symbol table index
	int number;
    char* string;
    struct Program* programPtr;
    struct Rule* rulePtr;
    struct Prop* propPtr;
    struct Obj* objPtr;
    struct RuleSelector* ruleSelectorPtr;
    struct Rule** rulePtrMany;
    struct Prop** propPtrMany;
    struct Obj** objPtrMany;
};

%token
    START_BODY END_BODY START_FUNC END_FUNC
    COLON SEMICOLON PIPE COMMA PERCENT
    ADD_OP SUB_OP MUL_OP DIV_OP
%token <string> WORD STRING CLASS PSEUDO_CLASS VARIABLE
%token <number> NUMBER
%left ADD_OP SUB_OP
%left MUL_OP DIV_OP
%right START_FUNC END_FUNC
%right START_BODY END_BODY
%type <programPtr> program
%type <rulePtr> rule
%type <propPtr> prop
%type <objPtr> obj
%type <rulePtrMany> rules;
%type <propPtrMany> props;
%type <objPtrMany> objs args;
%type <ruleSelectorPtr> rule_selector rule_addons;

%%
program:
        rules { global_program = make_program($1); }
        ;

rules:
        rule { make_array(struct Rule, REGULES_SIZE, $1); $$ = arr; }
        | rules rule { append_to_array($1, REGULES_SIZE, $2); $$ = $1; }
        ;

rule:
        rule_selector START_BODY props END_BODY { $$ = make_rule($1, $3); }
        ;

rule_selector:
        WORD rule_addons { $$ = $2; $$->element = $1; }
        | rule_addons { $$ = $1; }
        ;

rule_addons:
        %empty { $$ = make_rule_selector(); }
        | rule_addons CLASS { append_to_array($$->klasses, KLASSES_SIZE, $2); }
        | rule_addons PSEUDO_CLASS { $$->pseudo_klass = $2; }
        ;

props:
        prop { make_array(struct Prop, PROPS_SIZE, $1); $$ = arr; }
        | props prop { append_to_array($1, PROPS_SIZE, $2); $$ = $1; }
        ;

prop:
        WORD COLON objs SEMICOLON { $$ = make_prop($1, $3); }
        ;

objs:
        obj { make_array(struct Obj, OBJS_SIZE, $1); $$ = arr; }
        | objs PIPE obj { append_to_array($1, OBJS_SIZE, $3); $$ = $1; }
        ;

obj:
        NUMBER { $$ = make_obj_as_number($1); }
        | NUMBER PERCENT { $$ = make_obj_as_percent($1); }
        | STRING { $$ = make_obj_as_string($1); }
        | VARIABLE { $$ = make_obj_as_variable($1); }
        | rule_selector { $$ = make_obj_as_rule($1); }
        | obj ADD_OP obj { $$ = make_obj_as_add($1, $3); }
        | obj SUB_OP obj { $$ = make_obj_as_sub($1, $3); }
        | obj MUL_OP obj { $$ = make_obj_as_mul($1, $3); }
        | obj DIV_OP obj { $$ = make_obj_as_div($1, $3); }
        | START_FUNC obj END_FUNC { $$ = $2; }
        | WORD START_FUNC END_FUNC { $$ = make_obj_as_noargs_func($1); }
        | WORD START_FUNC args END_FUNC { $$ = make_obj_as_func($1, $3); }
        ;

args:
        obj { make_array(struct Obj, OBJS_SIZE, $1); $$ = arr; }
        | args COMMA obj { append_to_array($1, OBJS_SIZE, $3); $$ = $1; }
        ;
%%

struct Program* css_parse_file(char* filename) {
    FILE *stream = fopen(filename, "r");
    struct Program* program = css_parse_file_as_stream(stream);
    fclose(stream);
    return program;
}

struct Program* css_parse_file_as_stream(FILE* stream) {
    yyin = stream;
    yyparse();
    return global_program;
}
