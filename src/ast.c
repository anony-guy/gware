#include "ast.h"
#include <stdlib.h>
#include <string.h>

ASTNode* ASTNode_create(ASTNodeType type) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    node->type = type;
    node->value = NULL;
    node->typeAnnotation = NULL;
    node->left = NULL;
    node->right = NULL;
    node->statements = NULL;
    node->statementCount = 0;
    node->statementCapacity = 0;
    
    node->propertyName = NULL;
    node->attributes = NULL;
    node->attributeCount = 0;
    node->attributeCapacity = 0;
    return node;
}

void ASTNode_addStatement(ASTNode* parent, ASTNode* statement) {
    if (parent->statementCount >= parent->statementCapacity) {
        parent->statementCapacity = parent->statementCapacity == 0 ? 4 : parent->statementCapacity * 2;
        parent->statements = (ASTNode**)realloc(parent->statements, sizeof(ASTNode*) * parent->statementCapacity);
    }
    parent->statements[parent->statementCount++] = statement;
}

void ASTNode_addAttribute(ASTNode* element, ASTNode* attribute) {
    if (element->attributeCount >= element->attributeCapacity) {
        element->attributeCapacity = element->attributeCapacity == 0 ? 4 : element->attributeCapacity * 2;
        element->attributes = (ASTNode**)realloc(element->attributes, sizeof(ASTNode*) * element->attributeCapacity);
    }
    element->attributes[element->attributeCount++] = attribute;
}

void ASTNode_destroy(ASTNode* node) {
    if (!node) return;
    if (node->value) free(node->value);
    if (node->typeAnnotation) free(node->typeAnnotation);
    if (node->propertyName) free(node->propertyName);
    ASTNode_destroy(node->left);
    ASTNode_destroy(node->right);
    if (node->statements) {
        for (int i = 0; i < node->statementCount; i++) {
            ASTNode_destroy(node->statements[i]);
        }
        free(node->statements);
    }
    if (node->attributes) {
        for (int i = 0; i < node->attributeCount; i++) {
            ASTNode_destroy(node->attributes[i]);
        }
        free(node->attributes);
    }
    free(node);
}
