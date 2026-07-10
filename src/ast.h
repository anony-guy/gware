#ifndef AST_H
#define AST_H

typedef enum {
    AST_PROGRAM,
    AST_SET_STATEMENT,
    AST_EXPRESSION_STATEMENT,
    AST_IDENTIFIER,
    AST_INTEGER_LITERAL,
    AST_STRING_LITERAL,
    AST_INFIX_EXPRESSION,
    AST_CALL_EXPRESSION,
    AST_IF_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_BLOCK_STATEMENT,
    
    // GwareWeb Nodes
    AST_COMPONENT_DECLARATION,
    AST_ACTION_DECLARATION,
    AST_STYLE_DECLARATION,
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
    struct ASTNode** attributes; // Attributes like onClick
    int attributeCount;
    int attributeCapacity;
} ASTNode;

ASTNode* ASTNode_create(ASTNodeType type);
void ASTNode_addStatement(ASTNode* parent, ASTNode* statement);
void ASTNode_addAttribute(ASTNode* element, ASTNode* attribute);
void ASTNode_destroy(ASTNode* node);

#endif // AST_H
