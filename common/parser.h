#ifdef __cplusplus
extern "C" {
#endif

#pragma once
// #include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef enum { TOKEN_NONE,
	TOKEN_NUMBER,
	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_STAR,
	TOKEN_SLASH,
	TOKEN_VARIABLE,
	TOKEN_EOF } token_type;

typedef struct {
	token_type type;
	union {
		float f;
		struct {
			const char* s;
			int len;
		};
	};
} Token;

typedef struct {
	float lbp;
	float rbp;
} BindingPower;

typedef enum { AST_FLOAT,
	AST_ADD,
	AST_SUB,
	AST_DIV,
	AST_MUL,
	AST_VARIABLE,
	AST_ERR } ast_type;

typedef struct ast_node {
	ast_type type;
	union {
		float f;
		struct {
			struct ast_node* left_node;
			struct ast_node* right_node;
		};
		float* variable_data;
	};
} ast_node;

typedef struct {
	float* pitch;
	float* mod;
	float* key_freq;
	float* freq_log2;
	float* velocity;
	float* c1;
	float* c2;
	float* c3;
	float* c4;
	float* osc1;
	float* osc2;
	float* osc3;
	float* osc4;
	float* osc5;
} variable_pointers;

typedef struct {
	int token_i;
	Token* tokens;
	ast_node* ast_nodes;
	int next_free_ast_node_i;
	int max_ast_nodes;
	ast_node* ast_root;
	variable_pointers* vars;
} parser_state;

int parse_token(const char* exp, const char** remaining, Token* token);

int parser(const char* exp, parser_state* parser_state_ptr, variable_pointers* variable_pointers_inst);

float eval(ast_node* n);
void parser_cleanup(parser_state* P);

#ifdef __cplusplus
}
#endif
