#include "transpiler.h"
#include <stdio.h>
#include "color.h"
#include <stdlib.h>
#include <string.h>

static void emitCSS(ASTNode* styleBlock, char* compName, FILE* out) {
    if (!styleBlock) return;
    fprintf(out, ".%s {\n", compName);
    for (int i = 0; i < styleBlock->statementCount; i++) {
        ASTNode* prop = styleBlock->statements[i];
        if (prop->type == AST_CSS_PROPERTY) {
            // Strip quotes if they exist in the value for CSS
            char* val = prop->value;
            if (val[0] == '"') {
                fprintf(out, "    %s: %.*s;\n", prop->propertyName, (int)(strlen(val)-2), val+1);
            } else {
                fprintf(out, "    %s: %s;\n", prop->propertyName, val);
            }
        }
    }
    fprintf(out, "}\n");
}

static void emitJSState(ASTNode* setStmt, FILE* out) {
    fprintf(out, "let %s = ", setStmt->left->value);
    ASTNode* right = setStmt->right;
    if (right->type == AST_INTEGER_LITERAL) fprintf(out, "%s", right->value);
    else if (right->type == AST_STRING_LITERAL) fprintf(out, "\"%s\"", right->value);
    else fprintf(out, "0"); // Fallback
    fprintf(out, ";\n");
}

static void emitJSExpr(ASTNode* expr, FILE* out) {
    if (!expr) return;
    if (expr->type == AST_IDENTIFIER) fprintf(out, "%s", expr->value);
    else if (expr->type == AST_INTEGER_LITERAL) fprintf(out, "%s", expr->value);
    else if (expr->type == AST_STRING_LITERAL) fprintf(out, "\"%s\"", expr->value);
    else if (expr->type == AST_INFIX_EXPRESSION) {
        emitJSExpr(expr->left, out);
        fprintf(out, " %s ", expr->value);
        emitJSExpr(expr->right, out);
    }
}

static void emitJSAction(ASTNode* actionBlock, FILE* out) {
    fprintf(out, "function %s() {\n", actionBlock->value);
    ASTNode* block = actionBlock->right;
    for (int i = 0; i < block->statementCount; i++) {
        ASTNode* stmt = block->statements[i];
        if (stmt->type == AST_SET_STATEMENT) {
            fprintf(out, "    %s = ", stmt->left->value);
            emitJSExpr(stmt->right, out);
            fprintf(out, ";\n");
        }
    }
    fprintf(out, "    updateDOM();\n");
    fprintf(out, "}\n");
}

static void emitHTMLView(ASTNode* viewBlock, char* compName, FILE* out) {
    for (int i = 0; i < viewBlock->statementCount; i++) {
        ASTNode* el = viewBlock->statements[i];
        if (el->type == AST_EXPRESSION_STATEMENT && el->left->type == AST_CALL_EXPRESSION) {
            // It's a show() call
            ASTNode* arg = el->left->right;
            if (arg->type == AST_STRING_LITERAL) {
                fprintf(out, "%s ", arg->value);
            } else if (arg->type == AST_IDENTIFIER) {
                fprintf(out, "<span id=\"var_%s\"></span> ", arg->value);
            }
        } else if (el->type == AST_UI_ELEMENT) {
            fprintf(out, "<%s class=\"%s\"", el->value, compName);
            for (int a = 0; a < el->attributeCount; a++) {
                ASTNode* attr = el->attributes[a];
                if (strcmp(attr->propertyName, "onClick") == 0) {
                    fprintf(out, " onclick=\"%s()\"", attr->value);
                } else {
                    fprintf(out, " %s=\"%s\"", attr->propertyName, attr->value);
                }
            }
            fprintf(out, ">");
            if (el->right && el->right->type == AST_BLOCK_STATEMENT) {
                emitHTMLView(el->right, compName, out);
            }
            fprintf(out, "</%s>\n", el->value);
        }
    }
}

void Transpile_to_html(ASTNode* program, const char* outputFile) {
    FILE* out = fopen(outputFile, "w");
    if (!out) {
        printf(ANSI_COLOR_RED "Error: Could not open output file %s\n" ANSI_COLOR_RESET, outputFile);
        return;
    }

    fprintf(out, "<!DOCTYPE html>\n<html>\n<head>\n<style>\n");
    
    // 1. Pass: Emit CSS
    for (int i = 0; i < program->statementCount; i++) {
        if (program->statements[i]->type == AST_COMPONENT_DECLARATION) {
            ASTNode* comp = program->statements[i];
            for (int j = 0; j < comp->statementCount; j++) {
                if (comp->statements[j]->type == AST_STYLE_DECLARATION) {
                    emitCSS(comp->statements[j], comp->value, out);
                }
            }
        }
    }
    
    fprintf(out, "</style>\n</head>\n<body>\n<div id=\"app\">\n");
    
    // 2. Pass: Emit HTML
    for (int i = 0; i < program->statementCount; i++) {
        if (program->statements[i]->type == AST_COMPONENT_DECLARATION) {
            ASTNode* comp = program->statements[i];
            for (int j = 0; j < comp->statementCount; j++) {
                if (comp->statements[j]->type == AST_VIEW_DECLARATION) {
                    emitHTMLView(comp->statements[j], comp->value, out);
                }
            }
        }
    }
    
    fprintf(out, "</div>\n<script>\n");
    
    // 3. Pass: Emit JS (State and Actions)
    for (int i = 0; i < program->statementCount; i++) {
        if (program->statements[i]->type == AST_COMPONENT_DECLARATION) {
            ASTNode* comp = program->statements[i];
            for (int j = 0; j < comp->statementCount; j++) {
                if (comp->statements[j]->type == AST_SET_STATEMENT) {
                    emitJSState(comp->statements[j], out);
                } else if (comp->statements[j]->type == AST_ACTION_DECLARATION) {
                    emitJSAction(comp->statements[j], out);
                }
            }
        }
    }
    
    // 4. Emit DOM Update function (binding variables to their spans)
    fprintf(out, "function updateDOM() {\n");
    for (int i = 0; i < program->statementCount; i++) {
        if (program->statements[i]->type == AST_COMPONENT_DECLARATION) {
            ASTNode* comp = program->statements[i];
            for (int j = 0; j < comp->statementCount; j++) {
                if (comp->statements[j]->type == AST_SET_STATEMENT) {
                    char* varName = comp->statements[j]->left->value;
                    fprintf(out, "    let el_%s = document.getElementById('var_%s');\n", varName, varName);
                    fprintf(out, "    if (el_%s) el_%s.innerText = %s;\n", varName, varName, varName);
                }
            }
        }
    }
    fprintf(out, "}\n");
    
    // Initial DOM population
    fprintf(out, "updateDOM();\n");
    
    fprintf(out, "</script>\n</body>\n</html>\n");
    fclose(out);
    printf(ANSI_COLOR_GREEN "Successfully transpiled to %s\n" ANSI_COLOR_RESET, outputFile);
}
