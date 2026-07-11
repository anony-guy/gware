#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"

void printNode(ASTNode* node, int depth) {
    if (!node) return;
    for(int i=0; i<depth; i++) printf("  ");
    printf("Type: %d", node->type);
    if (node->value) printf(", Value: %s", node->value);
    printf("\n");
    printNode(node->left, depth + 1);
    printNode(node->right, depth + 1);
    for (int i=0; i<node->statementCount; i++) printNode(node->statements[i], depth + 1);
}

int main() {
    Lexer* l = Lexer_create("set AppState.users = fetch(\"url\")");
    Parser* p = Parser_create(l);
    ASTNode* prog = Parser_parseProgram(p);
    printNode(prog, 0);
    return 0;
}
