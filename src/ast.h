#ifndef AST_H
#define AST_H

typedef enum {
    AST_PROGRAM,
    AST_SET_STATEMENT,
    AST_CONST_STATEMENT,
    AST_EXPRESSION_STATEMENT,
    AST_IDENTIFIER,
    AST_INTEGER_LITERAL,
    AST_STRING_LITERAL,
    AST_INTERP_STRING,
    AST_INFIX_EXPRESSION,
    AST_TERNARY_EXPRESSION,
    AST_CALL_EXPRESSION,
    AST_IF_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_FOR_IN_STATEMENT,
    AST_BLOCK_STATEMENT,
    AST_FUNCTION_DECLARATION,
    AST_RETURN_STATEMENT,
    AST_BREAK_STATEMENT,
    AST_CONTINUE_STATEMENT,
    AST_TRY_STATEMENT,
    AST_IMPORT_STATEMENT,
    AST_FUNCTION_CALL,
    AST_ARRAY_LITERAL,
    AST_INDEX_EXPRESSION,
    AST_OBJECT_LITERAL,
    
    // GwareWeb Nodes
    AST_COMPONENT_DECLARATION,
    AST_ACTION_DECLARATION,
    AST_STYLE_DECLARATION,
    AST_STORE_DECLARATION,
    AST_ROUTER_DECLARATION,
    AST_ROUTE_DECLARATION,
    
    AST_VIEW_DECLARATION,
    AST_UI_ELEMENT,
    AST_CSS_PROPERTY
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    char* value; // Holds identifier name, number/string string, or operator
    char* typeAnnotation; // "int", "string", or NULL if dynamic
    
    struct ASTNode* left;  // condition for if/while
    struct ASTNode* right; // consequence/body block for if/while

    // For Program and Block nodes
    struct ASTNode** statements;
    int statementCount;
    int statementCapacity;

    // For GwareWeb
    char* propertyName;
    char* tag;
    struct ASTNode** attributes;
    int attributeCount;
    int attributeCapacity;
    struct ASTNode** children;
    int childCount;
    int childCapacity;
    
    int line;
    char* file;
    
    // For Functions
    struct ASTNode** parameters;
    int parameterCount;
    int parameterCapacity;
} ASTNode;

ASTNode* ASTNode_create_loc(ASTNodeType type, int line, const char* file);
void ASTNode_addStatement(ASTNode* parent, ASTNode* statement);
void ASTNode_addAttribute(ASTNode* element, ASTNode* attribute);
void ASTNode_addParameter(ASTNode* func, ASTNode* param);
void ASTNode_destroy(ASTNode* node);

#endif // AST_H
