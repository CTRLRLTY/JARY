#ifndef JAYVM_AST_H
#define JAYVM_AST_H

typedef enum jy_ast_type {
	AST_NONE = -1,
	AST_ROOT,

	AST_DECL,
	AST_SECTION,

	AST_BINARY,
	AST_CALL,
	AST_UNARY,
	AST_EVENT,
	AST_MEMBER,

	AST_NAME,
	AST_PATH,
	AST_LITERAL,
} jy_ast_type_t;

#endif // JAYVM_AST_H