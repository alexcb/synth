// #include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "acb_atof.h"
#include "acb_isspace.h"
#include "parser.h"

int parse_token(const char* exp, const char** remaining, Token* token)
{
	const char* s = NULL;
	char c;
	do {
		s = exp;
		c = *exp++;
	} while (acb_isspace(c));

	switch (c) {
	case '+':
		*token = (Token) { TOKEN_PLUS, c };
		*remaining = exp;
		return 0;
	case '-':
		*token = (Token) { TOKEN_MINUS, c };
		*remaining = exp;
		return 0;
	case '*':
		*token = (Token) { TOKEN_STAR, c };
		*remaining = exp;
		return 0;
	case '/':
		*token = (Token) { TOKEN_SLASH, c };
		*remaining = exp;
		return 0;
	case '\0':
		*token = (Token) { TOKEN_EOF, c };
		*remaining = exp;
		return 0;
	};

	const char* next = NULL;
	float f = acb_strtof(s, &next);
	if (next != NULL) {
		*token = (Token) {
			.type = TOKEN_NUMBER,
			.f = f
		};
		exp += next - s - 1;
		*remaining = exp;
		return 0;
	}

	if ('a' <= c && c <= 'z') {
		for (;;) {
			char peek = *exp;
			if (('a' <= peek && peek <= 'z') || ('0' <= peek && peek <= '9') || peek == '_') {
				exp++;
				continue;
			}
			break;
		}
		*token = (Token) {
			.type = TOKEN_VARIABLE,
			.s = s,
			.len = (exp - s)
		};
		*remaining = exp;
		return 0;
	}

	// TODO return an error saying unrecognized token
	return 1;
}

Token current_token(parser_state* P)
{
	return P->tokens[P->token_i];
}

Token peek_token(parser_state* P)
{
	return P->tokens[P->token_i + 1];
}

BindingPower binding_power(token_type op)
{
	switch (op) {
	case TOKEN_PLUS:
	case TOKEN_MINUS:
		return (BindingPower) { 1, 1.1 };
	case TOKEN_STAR:
	case TOKEN_SLASH:
		return (BindingPower) { 2, 2.1 };
	default:
		return (BindingPower) { 0 };
	}
}

int is_op(token_type op)
{
	switch (op) {
	case TOKEN_PLUS:
	case TOKEN_MINUS:
	case TOKEN_STAR:
	case TOKEN_SLASH:
		return 1;
	default:
		return 0;
	}
}

int reserve_ast_node(parser_state* P, ast_node** node)
{
	if (P->next_free_ast_node_i >= P->max_ast_nodes) {
		return 1;
	}
	*node = &P->ast_nodes[P->next_free_ast_node_i++];
	return 0;
}

int resolve_variable_address(parser_state* P, const char* s, int strlen, float** memory_address)
{
	if (strncmp(s, "pitch", strlen) == 0) {
		*memory_address = P->vars->pitch;
		return 0;
	}
	if (strncmp(s, "mod", strlen) == 0) {
		*memory_address = P->vars->mod;
		return 0;
	}
	if (strncmp(s, "key_freq", strlen) == 0) {
		*memory_address = P->vars->key_freq;
		return 0;
	}
	if (strncmp(s, "velocity", strlen) == 0) {
		*memory_address = P->vars->velocity;
		return 0;
	}
	if (strncmp(s, "c1", strlen) == 0) {
		*memory_address = P->vars->c1;
		return 0;
	}
	if (strncmp(s, "c2", strlen) == 0) {
		*memory_address = P->vars->c2;
		return 0;
	}
	if (strncmp(s, "c3", strlen) == 0) {
		*memory_address = P->vars->c3;
		return 0;
	}
	if (strncmp(s, "c4", strlen) == 0) {
		*memory_address = P->vars->c4;
		return 0;
	}
	if (strncmp(s, "osc1", strlen) == 0) {
		*memory_address = P->vars->osc1;
		return 0;
	}
	if (strncmp(s, "osc2", strlen) == 0) {
		*memory_address = P->vars->osc2;
		return 0;
	}
	if (strncmp(s, "osc3", strlen) == 0) {
		*memory_address = P->vars->osc3;
		return 0;
	}
	if (strncmp(s, "osc4", strlen) == 0) {
		*memory_address = P->vars->osc4;
		return 0;
	}
	if (strncmp(s, "osc5", strlen) == 0) {
		*memory_address = P->vars->osc5;
		return 0;
	}
	// printf("failed to lookup %.*s\n", strlen, s);
	return 1;
}

int create_ast_node_from_token(parser_state* P, const Token t, ast_node** node)
{
	if (reserve_ast_node(P, node)) {
		return 1;
	}

	switch (t.type) {
	case TOKEN_NUMBER:
		(**node).type = AST_FLOAT;
		(**node).f = t.f;
		return 0;
	case TOKEN_VARIABLE:
		if (resolve_variable_address(P, t.s, t.len, &((**node).variable_data))) {
			return 1;
		}
		(**node).type = AST_VARIABLE;
		return 0;
	default:
		// TODO set an error message
		return 1;
	}
}

ast_type token_type_to_ast_type(token_type tok)
{
	switch (tok) {
	case TOKEN_PLUS:
		return AST_ADD;
	case TOKEN_MINUS:
		return AST_SUB;
	case TOKEN_STAR:
		return AST_MUL;
	case TOKEN_SLASH:
		return AST_DIV;
	default:
		return AST_ERR;
	}
}

int parse_expression(parser_state* P, float min_bp, ast_node** ast_op)
{
	Token lhs_token = current_token(P);
	if (lhs_token.type != TOKEN_NUMBER && lhs_token.type != TOKEN_VARIABLE) {
		return 1;
	}
	ast_node* lhs;
	if (create_ast_node_from_token(P, lhs_token, &lhs)) {
		return 1;
	}
	P->token_i++;
	while (is_op(current_token(P).type)) {
		Token tok = current_token(P);
		BindingPower bpow = binding_power(tok.type);
		if (min_bp > bpow.lbp) {
			*ast_op = lhs;
			return 0;
		}
		P->token_i++;
		ast_node* rhs = NULL;
		if (parse_expression(P, bpow.rbp, &rhs)) {
			return 1;
		}

		ast_node* op_node;
		if (reserve_ast_node(P, &op_node)) {
			return 1;
		}
		op_node->type = token_type_to_ast_type(tok.type);
		op_node->left_node = lhs;
		op_node->right_node = rhs;

		lhs = op_node;
	}
	*ast_op = lhs;
	return 0;
}

int parser(const char* exp, parser_state* parser_state_ptr, variable_pointers* variable_pointers_inst)
{
	int max_tokens = 1024;
	Token* tokens = malloc(sizeof(Token) * max_tokens);
	for (int i = 0;; i++) {
		if (i == max_tokens) {
			// TODO error
			goto error;
		}
		parse_token(exp, &exp, &tokens[i]);
		if (tokens[i].type == TOKEN_EOF) {
			break;
		}
	}

	int max_ast_nodes = 1024;
	ast_node* ast_nodes = malloc(sizeof(ast_node) * max_ast_nodes);

	parser_state P = {
		.token_i = 0,
		.tokens = tokens,
		.ast_nodes = ast_nodes,
		.next_free_ast_node_i = 0,
		.max_ast_nodes = max_ast_nodes,
		.ast_root = NULL,
		.vars = variable_pointers_inst
	};
	if (parse_expression(&P, 0, &P.ast_root)) {
		// printf("error: failed to parse expression\n");
		goto error;
	}
	if (current_token(&P).type != TOKEN_EOF) {
		// printf("error: unprocessed tokens remain (expected EOF instead)\n");
		goto error;
	}
	*parser_state_ptr = P;
	return 0;
error:
	parser_cleanup(&P);
	return 1;
}

void parser_cleanup(parser_state* P)
{
	if (P->tokens) {
		free(P->tokens);
		P->tokens = NULL;
	}
	if (P->ast_nodes) {
		free(P->ast_nodes);
		P->ast_nodes = NULL;
	}
}

float eval(ast_node* n)
{
	float lhs;
	float rhs;
	float f;
	ast_type type = n->type;

	if (n->type == AST_FLOAT) {
		f = n->f;
		return f;
	}

	if (n->type == AST_VARIABLE) {
		return *(n->variable_data);
	}

	lhs = eval(n->left_node);
	rhs = eval(n->right_node);

	switch (type) {
	case AST_ADD:
		return lhs + rhs;
	case AST_SUB:
		return lhs - rhs;
	case AST_MUL:
		return lhs * rhs;
	case AST_DIV:
		return lhs / rhs;
	default:
		break;
	}
	return 0;
}

// int main(void)
// {
// 	char* exp = "3.14*var2+1";
//
// 	//exp = "1+2*5+1";
// 	exp = "2*velocity+1-0.3";
//
// 	float velocity;
// 	variable_pointers vars = {
// 		.velocity = &velocity
// 	};
//
// 	velocity = 0.4;
//
// 	parser_state p;
// 	if( parser(exp, &p, &vars) ) {
// 		return 1;
// 	}
// 	float result = eval(p.ast_root);
// 	printf("%f\n", result);
// }
