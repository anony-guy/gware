#include "formatter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "color.h"

static void format_node(ASTNode* node, FILE* out, int indent);

static void print_indent(FILE* out, int indent) {
    for (int i = 0; i < indent * 4; i++) {
        fputc(' ', out);
    }
}

static void format_node(ASTNode* node, FILE* out, int indent) {
    if (!node) return;
    
    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK_STATEMENT:
            for (int i = 0; i < node->statementCount; i++) {
                if (node->type == AST_BLOCK_STATEMENT) print_indent(out, indent);
                format_node(node->statements[i], out, indent);
                if (node->type == AST_BLOCK_STATEMENT || node->type == AST_PROGRAM) {
                    fprintf(out, "\n");
                }
            }
            break;
            
        case AST_EXPRESSION_STATEMENT:
            format_node(node->left, out, indent);
            break;
            
        case AST_SET_STATEMENT:
            fprintf(out, "set ");
            format_node(node->left, out, indent);
            fprintf(out, " = ");
            format_node(node->right, out, indent);
            break;
            
        case AST_COMPONENT_DECLARATION:
            fprintf(out, "component %s {\n", node->value);
            for (int i = 0; i < node->statementCount; i++) {
                print_indent(out, indent + 1);
                format_node(node->statements[i], out, indent + 1);
                fprintf(out, "\n");
            }
            print_indent(out, indent);
            fprintf(out, "}");
            break;
            
        case AST_UI_ELEMENT:
            fprintf(out, "<%s", node->value);
            // Attributes (simplified - properties)
            if (node->right && node->right->type == AST_OBJECT_LITERAL) {
                for (int i = 0; i < node->right->parameterCount; i+=2) {
                    fprintf(out, " %s=", node->right->parameters[i]->value);
                    if (node->right->parameters[i+1]->type == AST_STRING_LITERAL) {
                        fprintf(out, "\"%s\"", node->right->parameters[i+1]->value);
                    } else if (node->right->parameters[i+1]->type == AST_INTERP_STRING) {
                        fprintf(out, "`%s`", node->right->parameters[i+1]->value);
                    } else {
                        fprintf(out, "{");
                        format_node(node->right->parameters[i+1], out, indent);
                        fprintf(out, "}");
                    }
                }
            }
            fprintf(out, ">");
            
            // Children
            if (node->left && node->left->type == AST_BLOCK_STATEMENT) {
                if (node->left->statementCount > 0) {
                    fprintf(out, "\n");
                    format_node(node->left, out, indent + 1);
                    print_indent(out, indent);
                }
            }
            
            fprintf(out, "</%s>", node->value);
            break;
            
        case AST_FUNCTION_DECLARATION:
            fprintf(out, "def %s(", node->value);
            for (int i = 0; i < node->parameterCount; i++) {
                format_node(node->parameters[i], out, indent);
                if (i < node->parameterCount - 1) fprintf(out, ", ");
            }
            fprintf(out, ") {\n");
            format_node(node->right, out, indent + 1);
            print_indent(out, indent);
            fprintf(out, "}");
            break;
            
        case AST_IF_STATEMENT:
            fprintf(out, "if ");
            format_node(node->left, out, indent);
            fprintf(out, " {\n");
            format_node(node->right, out, indent + 1);
            print_indent(out, indent);
            fprintf(out, "}");
            
            if (node->parameterCount > 0) {
                fprintf(out, " else ");
                ASTNode* elseNode = node->parameters[0];
                if (elseNode->type == AST_IF_STATEMENT) {
                    format_node(elseNode, out, indent);
                } else {
                    fprintf(out, "{\n");
                    format_node(elseNode, out, indent + 1);
                    print_indent(out, indent);
                    fprintf(out, "}");
                }
            }
            break;
            
        case AST_WHILE_STATEMENT:
            fprintf(out, "while ");
            format_node(node->left, out, indent);
            fprintf(out, " {\n");
            format_node(node->right, out, indent + 1);
            print_indent(out, indent);
            fprintf(out, "}");
            break;
            
        case AST_FOR_IN_STATEMENT:
            fprintf(out, "for %s in ", node->value);
            format_node(node->left, out, indent);
            fprintf(out, " {\n");
            format_node(node->right, out, indent + 1);
            print_indent(out, indent);
            fprintf(out, "}");
            break;
            
        case AST_TRY_STATEMENT:
            fprintf(out, "try {\n");
            format_node(node->left, out, indent + 1);
            print_indent(out, indent);
            fprintf(out, "} catch (%s) {\n", node->value);
            format_node(node->right, out, indent + 1);
            print_indent(out, indent);
            fprintf(out, "}");
            break;
            
        case AST_RETURN_STATEMENT:
            fprintf(out, "return");
            if (node->left) {
                fprintf(out, " ");
                format_node(node->left, out, indent);
            }
            break;
            
        case AST_IMPORT_STATEMENT:
            fprintf(out, "import \"%s\"", node->value);
            break;
            
        case AST_INFIX_EXPRESSION:
            format_node(node->left, out, indent);
            fprintf(out, " %s ", node->value);
            format_node(node->right, out, indent);
            break;
            
        case AST_CALL_EXPRESSION:
            format_node(node->left, out, indent);
            fprintf(out, "(");
            for (int i = 0; i < node->parameterCount; i++) {
                format_node(node->parameters[i], out, indent);
                if (i < node->parameterCount - 1) fprintf(out, ", ");
            }
            fprintf(out, ")");
            break;
            
        case AST_FUNCTION_CALL:
            fprintf(out, "%s(", node->value);
            for (int i = 0; i < node->parameterCount; i++) {
                format_node(node->parameters[i], out, indent);
                if (i < node->parameterCount - 1) fprintf(out, ", ");
            }
            fprintf(out, ")");
            break;
            
        case AST_INDEX_EXPRESSION:
            format_node(node->left, out, indent);
            fprintf(out, "[");
            format_node(node->right, out, indent);
            fprintf(out, "]");
            break;
            
        case AST_IDENTIFIER:
            fprintf(out, "%s", node->value);
            break;
            
        case AST_INTEGER_LITERAL:
            fprintf(out, "%s", node->value);
            break;
            
        case AST_STRING_LITERAL:
            fprintf(out, "\"%s\"", node->value);
            break;
            
        case AST_INTERP_STRING:
            fprintf(out, "`%s`", node->value);
            break;
            
        case AST_ARRAY_LITERAL:
            fprintf(out, "[");
            for (int i = 0; i < node->parameterCount; i++) {
                format_node(node->parameters[i], out, indent);
                if (i < node->parameterCount - 1) fprintf(out, ", ");
            }
            fprintf(out, "]");
            break;
            
        case AST_OBJECT_LITERAL:
            fprintf(out, "{");
            if (node->parameterCount > 0) {
                fprintf(out, "\n");
                for (int i = 0; i < node->parameterCount; i += 2) {
                    print_indent(out, indent + 1);
                    fprintf(out, "\"%s\": ", node->parameters[i]->value);
                    format_node(node->parameters[i+1], out, indent + 1);
                    if (i < node->parameterCount - 2) fprintf(out, ",\n");
                    else fprintf(out, "\n");
                }
                print_indent(out, indent);
            }
            fprintf(out, "}");
            break;
            
        default:
            break;
    }
}

void format_file(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        printf(ANSI_COLOR_RED "Error: Could not open file '%s'\n" ANSI_COLOR_RESET, filename);
        return;
    }
    
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, fp);
    source[size] = '\0';
    fclose(fp);
    
    Lexer* l = Lexer_create(source, filename);
    Parser* p = Parser_create(l);
    ASTNode* ast = Parser_parseProgram(p);
    
    if (!ast) {
        printf(ANSI_COLOR_RED "Syntax error detected, cannot format file.\n" ANSI_COLOR_RESET);
        free(source);
        Parser_destroy(p);
        Lexer_destroy(l);
        return;
    }
    
    // Write formatted output back to file
    FILE* out = fopen(filename, "wb");
    if (out) {
        format_node(ast, out, 0);
        fclose(out);
        printf(ANSI_COLOR_GREEN "Formatted %s successfully!\n" ANSI_COLOR_RESET, filename);
    }
    
    ASTNode_destroy(ast);
    Parser_destroy(p);
    Lexer_destroy(l);
    free(source);
}
