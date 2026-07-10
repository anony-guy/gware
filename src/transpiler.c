#include "transpiler.h"
#include <stdio.h>
#include "color.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char* compName;
    char* instanceId;
    ASTNode* compNode;
    ASTNode* instanceEl;
} ComponentInstance;

static ComponentInstance instances[100];
static int instanceCount = 0;

static void addInstance(char* compName, char* instanceId, ASTNode* compNode, ASTNode* instanceEl) {
    instances[instanceCount].compName = strdup(compName);
    instances[instanceCount].instanceId = strdup(instanceId);
    instances[instanceCount].compNode = compNode;
    instances[instanceCount].instanceEl = instanceEl;
    instanceCount++;
}

static ASTNode* findComponent(ASTNode* program, char* name) {
    for (int i = 0; i < program->statementCount; i++) {
        if (program->statements[i]->type == AST_COMPONENT_DECLARATION) {
            if (strcmp(program->statements[i]->value, name) == 0) {
                return program->statements[i];
            }
        }
    }
    return NULL;
}

static void emitCSS(ASTNode* styleBlock, char* compName, FILE* out) {
    if (!styleBlock) return;
    fprintf(out, ".%s {\n", compName);
    for (int i = 0; i < styleBlock->statementCount; i++) {
        ASTNode* prop = styleBlock->statements[i];
        if (prop->type == AST_CSS_PROPERTY) {
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

static void emitJSExpr(ASTNode* expr, char* instanceId, FILE* out) {
    if (!expr) return;
    if (expr->type == AST_IDENTIFIER) fprintf(out, "%s_%s", instanceId, expr->value);
    else if (expr->type == AST_INTEGER_LITERAL) fprintf(out, "%s", expr->value);
    else if (expr->type == AST_STRING_LITERAL) fprintf(out, "\"%s\"", expr->value);
    else if (expr->type == AST_INFIX_EXPRESSION) {
        emitJSExpr(expr->left, instanceId, out);
        fprintf(out, " %s ", expr->value);
        emitJSExpr(expr->right, instanceId, out);
    } else if (expr->type == AST_ARRAY_LITERAL) {
        fprintf(out, "[");
        for (int i = 0; i < expr->statementCount; i++) {
            emitJSExpr(expr->statements[i], instanceId, out);
            if (i < expr->statementCount - 1) fprintf(out, ", ");
        }
        fprintf(out, "]");
    }
}

static void emitJSState(ASTNode* setStmt, ComponentInstance* inst, FILE* out) {
    char* varName = setStmt->left->value;
    fprintf(out, "let %s_%s = ", inst->instanceId, varName);
    
    // Check if a prop was passed
    char* propValue = NULL;
    if (inst->instanceEl) {
        for (int i = 0; i < inst->instanceEl->attributeCount; i++) {
            if (strcmp(inst->instanceEl->attributes[i]->propertyName, varName) == 0) {
                propValue = inst->instanceEl->attributes[i]->value;
                break;
            }
        }
    }
    
    if (propValue) {
        // If prop looks like a number, emit as number, else string
        int isNum = 1;
        for (int i = 0; propValue[i]; i++) {
            if (!isdigit(propValue[i]) && propValue[i] != '-') { isNum = 0; break; }
        }
        if (isNum) fprintf(out, "%s", propValue);
        else fprintf(out, "\"%s\"", propValue);
    } else {
        ASTNode* right = setStmt->right;
        if (right->type == AST_INTEGER_LITERAL) fprintf(out, "%s", right->value);
        else if (right->type == AST_STRING_LITERAL) fprintf(out, "\"%s\"", right->value);
        else if (right->type == AST_ARRAY_LITERAL) emitJSExpr(right, inst->instanceId, out);
        else fprintf(out, "0"); 
    }
    fprintf(out, ";\n");
}

static void emitJSAction(ASTNode* actionBlock, char* instanceId, FILE* out) {
    fprintf(out, "function %s_%s() {\n", instanceId, actionBlock->value);
    ASTNode* block = actionBlock->right;
    for (int i = 0; i < block->statementCount; i++) {
        ASTNode* stmt = block->statements[i];
        if (stmt->type == AST_SET_STATEMENT) {
            fprintf(out, "    %s_%s = ", instanceId, stmt->left->value);
            emitJSExpr(stmt->right, instanceId, out);
            fprintf(out, ";\n");
        }
    }
    fprintf(out, "    updateDOM();\n");
    fprintf(out, "}\n");
}

static int loopCounter = 0;

static void emitJSTemplate(ASTNode* block, char* instanceId, char* loopVar, FILE* out) {
    if (!block) return;
    for (int i = 0; i < block->statementCount; i++) {
        ASTNode* el = block->statements[i];
        if (el->type == AST_EXPRESSION_STATEMENT && el->left && el->left->type == AST_CALL_EXPRESSION) {
            ASTNode* arg = el->left->right;
            if (arg->type == AST_STRING_LITERAL) {
                fprintf(out, "%s ", arg->value);
            } else if (arg->type == AST_IDENTIFIER) {
                if (strcmp(arg->value, loopVar) == 0) {
                    fprintf(out, "${%s} ", loopVar);
                } else {
                    fprintf(out, "${%s_%s} ", instanceId, arg->value);
                }
            }
        } else if (el->type == AST_UI_ELEMENT) {
            fprintf(out, "<%s", el->value);
            for (int a = 0; a < el->attributeCount; a++) {
                fprintf(out, " %s=\"%s\"", el->attributes[a]->propertyName, el->attributes[a]->value);
            }
            fprintf(out, ">");
            if (el->right && el->right->type == AST_BLOCK_STATEMENT) {
                emitJSTemplate(el->right, instanceId, loopVar, out);
            }
            fprintf(out, "</%s>", el->value);
        }
    }
}

static void emitUpdateLoops(ASTNode* block, char* instanceId, FILE* out) {
    if (!block) return;
    for (int i = 0; i < block->statementCount; i++) {
        ASTNode* el = block->statements[i];
        if (el->type == AST_IF_STATEMENT) {
            char* ifId = el->typeAnnotation;
            if (ifId) {
                fprintf(out, "    {\n");
                fprintf(out, "        let container = document.getElementById('%s');\n", ifId);
                fprintf(out, "        if (container) {\n");
                fprintf(out, "            if (");
                emitJSExpr(el->left, instanceId, out);
                fprintf(out, ") {\n");
                fprintf(out, "                container.innerHTML = `");
                emitJSTemplate(el->right, instanceId, NULL, out);
                fprintf(out, "`;\n");
                fprintf(out, "            } else {\n");
                fprintf(out, "                container.innerHTML = '';\n");
                fprintf(out, "            }\n");
                fprintf(out, "        }\n");
                fprintf(out, "    }\n");
            }
        } else if (el->type == AST_UI_ELEMENT) {
            if (strcmp(el->value, "for") == 0) {
                char* inAttr = NULL;
                char* asAttr = NULL;
                for (int a = 0; a < el->attributeCount; a++) {
                    if (strcmp(el->attributes[a]->propertyName, "in") == 0) inAttr = el->attributes[a]->value;
                    if (strcmp(el->attributes[a]->propertyName, "as") == 0) asAttr = el->attributes[a]->value;
                }
                char* loopId = el->typeAnnotation;
                if (inAttr && asAttr && loopId) {
                    fprintf(out, "    {\n");
                    fprintf(out, "        let html = ``;\n");
                    fprintf(out, "        if (%s_%s) {\n", instanceId, inAttr);
                    fprintf(out, "            for (let %s of %s_%s) {\n", asAttr, instanceId, inAttr);
                    fprintf(out, "                html += `");
                    emitJSTemplate(el->right, instanceId, asAttr, out);
                    fprintf(out, "`;\n");
                    fprintf(out, "            }\n");
                    fprintf(out, "        }\n");
                    fprintf(out, "        let container = document.getElementById('%s');\n", loopId);
                    fprintf(out, "        if (container) container.innerHTML = html;\n");
                    fprintf(out, "    }\n");
                }
            } else if (el->right && el->right->type == AST_BLOCK_STATEMENT) {
                emitUpdateLoops(el->right, instanceId, out);
            }
        }
    }
}

static void emitHTMLView(ASTNode* viewBlock, char* compName, char* instanceId, ASTNode* program, FILE* out) {
    for (int i = 0; i < viewBlock->statementCount; i++) {
        ASTNode* el = viewBlock->statements[i];
        if (el->type == AST_EXPRESSION_STATEMENT && el->left && el->left->type == AST_CALL_EXPRESSION) {
            ASTNode* arg = el->left->right;
            if (arg->type == AST_STRING_LITERAL) {
                fprintf(out, "%s ", arg->value);
            } else if (arg->type == AST_IDENTIFIER) {
                fprintf(out, "<span id=\"var_%s_%s\"></span> ", instanceId, arg->value);
            }
        } else if (el->type == AST_IF_STATEMENT) {
            if (!el->typeAnnotation) {
                el->typeAnnotation = malloc(256);
                sprintf(el->typeAnnotation, "if_%d", loopCounter++);
            }
            fprintf(out, "<div id=\"%s\"></div>\n", el->typeAnnotation);
        } else if (el->type == AST_UI_ELEMENT) {
            if (strcmp(el->value, "for") == 0) {
                if (!el->typeAnnotation) {
                    el->typeAnnotation = malloc(256);
                    sprintf(el->typeAnnotation, "loop_%d", loopCounter++);
                }
                fprintf(out, "<div id=\"%s\"></div>\n", el->typeAnnotation);
            } else if (isupper(el->value[0])) {
                ASTNode* subComp = findComponent(program, el->value);
                if (subComp) {
                    char newInstanceId[256];
                    sprintf(newInstanceId, "%s_%d", el->value, instanceCount);
                    addInstance(el->value, newInstanceId, subComp, el);
                    
                    ASTNode* subView = NULL;
                    for (int j = 0; j < subComp->statementCount; j++) {
                        if (subComp->statements[j]->type == AST_VIEW_DECLARATION) {
                            subView = subComp->statements[j];
                        }
                    }
                    if (subView) {
                        fprintf(out, "<div class=\"%s\">", el->value);
                        emitHTMLView(subView, el->value, newInstanceId, program, out);
                        fprintf(out, "</div>");
                    }
                }
            } else {
                fprintf(out, "<%s class=\"%s\"", el->value, compName);
                for (int a = 0; a < el->attributeCount; a++) {
                    ASTNode* attr = el->attributes[a];
                    if (strcmp(attr->propertyName, "onClick") == 0) {
                        fprintf(out, " onclick=\"%s_%s()\"", instanceId, attr->value);
                    } else if (strcmp(attr->propertyName, "bind") == 0) {
                        fprintf(out, " id=\"var_%s_%s_input\" oninput=\"%s_%s = this.value; updateDOM()\"", instanceId, attr->value, instanceId, attr->value);
                    } else {
                        fprintf(out, " %s=\"%s\"", attr->propertyName, attr->value);
                    }
                }
                fprintf(out, ">");
                if (el->right && el->right->type == AST_BLOCK_STATEMENT) {
                    emitHTMLView(el->right, compName, instanceId, program, out);
                }
                fprintf(out, "</%s>\n", el->value);
            }
        }
    }
}

void Transpile_to_html(ASTNode* program, const char* outputFile) {
    instanceCount = 0;
    FILE* out = fopen(outputFile, "w");
    if (!out) {
        printf(ANSI_COLOR_RED "Error: Could not open output file %s\n" ANSI_COLOR_RESET, outputFile);
        return;
    }

    fprintf(out, "<!DOCTYPE html>\n<html>\n<head>\n<style>\n");
    
    // 1. Pass: Emit CSS (only once per component type)
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
    
    // 2. Pass: Emit HTML for Root Component (Assume last component is root if Main not found, or Main)
    ASTNode* rootComp = NULL;
    for (int i = 0; i < program->statementCount; i++) {
        if (program->statements[i]->type == AST_COMPONENT_DECLARATION) {
            rootComp = program->statements[i];
            if (strcmp(rootComp->value, "Main") == 0) break;
        }
    }
    
    if (rootComp) {
        char rootInstanceId[256];
        sprintf(rootInstanceId, "%s_%d", rootComp->value, instanceCount);
        addInstance(rootComp->value, rootInstanceId, rootComp, NULL);
        
        ASTNode* rootView = NULL;
        for (int j = 0; j < rootComp->statementCount; j++) {
            if (rootComp->statements[j]->type == AST_VIEW_DECLARATION) {
                rootView = rootComp->statements[j];
            }
        }
        if (rootView) emitHTMLView(rootView, rootComp->value, rootInstanceId, program, out);
    }
    
    fprintf(out, "</div>\n<script>\n");
    
    // 3. Pass: Emit JS for all collected instances
    for (int k = 0; k < instanceCount; k++) {
        ComponentInstance inst = instances[k];
        for (int j = 0; j < inst.compNode->statementCount; j++) {
            if (inst.compNode->statements[j]->type == AST_SET_STATEMENT) {
                emitJSState(inst.compNode->statements[j], &instances[k], out);
            } else if (inst.compNode->statements[j]->type == AST_ACTION_DECLARATION) {
                emitJSAction(inst.compNode->statements[j], inst.instanceId, out);
            }
        }
    }
    
    // 4. Emit DOM Update function
    fprintf(out, "function updateDOM() {\n");
    for (int k = 0; k < instanceCount; k++) {
        ComponentInstance inst = instances[k];
        for (int j = 0; j < inst.compNode->statementCount; j++) {
            if (inst.compNode->statements[j]->type == AST_SET_STATEMENT) {
                char* varName = inst.compNode->statements[j]->left->value;
                fprintf(out, "    let el_%s_%s = document.getElementById('var_%s_%s');\n", inst.instanceId, varName, inst.instanceId, varName);
                fprintf(out, "    if (el_%s_%s) el_%s_%s.innerText = %s_%s;\n", inst.instanceId, varName, inst.instanceId, varName, inst.instanceId, varName);
                fprintf(out, "    let inp_%s_%s = document.getElementById('var_%s_%s_input');\n", inst.instanceId, varName, inst.instanceId, varName);
                fprintf(out, "    if (inp_%s_%s && document.activeElement !== inp_%s_%s) inp_%s_%s.value = %s_%s;\n", inst.instanceId, varName, inst.instanceId, varName, inst.instanceId, varName, inst.instanceId, varName);
            }
        }
    }
    
    // 5. Emit List Rendering updates
    for (int k = 0; k < instanceCount; k++) {
        ComponentInstance inst = instances[k];
        for (int j = 0; j < inst.compNode->statementCount; j++) {
            if (inst.compNode->statements[j]->type == AST_VIEW_DECLARATION) {
                emitUpdateLoops(inst.compNode->statements[j], inst.instanceId, out);
            }
        }
    }
    
    fprintf(out, "}\n");
    
    fprintf(out, "updateDOM();\n");
    fprintf(out, "</script>\n</body>\n</html>\n");
    fclose(out);
    printf(ANSI_COLOR_GREEN "Successfully transpiled to %s\n" ANSI_COLOR_RESET, outputFile);
}

