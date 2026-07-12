#include "transpiler.h"
#include <stdio.h>
#include "color.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char* compName;
    char* instanceId;
    char* parentInstanceId;
    ASTNode* compNode;
    ASTNode* instanceEl;
} ComponentInstance;

static ComponentInstance instances[100];
static int instanceCount = 0;
static ASTNode* globalProgram = NULL;

static void addInstance(char* compName, char* instanceId, char* parentInstanceId, ASTNode* compNode, ASTNode* instanceEl) {
    instances[instanceCount].compName = strdup(compName);
    instances[instanceCount].instanceId = strdup(instanceId);
    instances[instanceCount].parentInstanceId = parentInstanceId ? strdup(parentInstanceId) : NULL;
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

static void emitJSExpr(ASTNode* expr, ComponentInstance* inst, FILE* out) {
    if (!expr) return;
    char* instanceId = inst ? inst->instanceId : "";
    
    if (expr->type == AST_IDENTIFIER) {
        int isStore = 0;
        if (globalProgram) {
            for (int i = 0; i < globalProgram->statementCount; i++) {
                if (globalProgram->statements[i]->type == AST_STORE_DECLARATION) {
                    if (strcmp(globalProgram->statements[i]->value, expr->value) == 0) {
                        isStore = 1;
                        break;
                    }
                }
            }
        }
        if (isStore) fprintf(out, "%s", expr->value);
        else if (strcmp(expr->value, "window") == 0 || strcmp(expr->value, "document") == 0 || strcmp(expr->value, "console") == 0 || strcmp(expr->value, "Math") == 0)
            fprintf(out, "%s", expr->value);
        else fprintf(out, "%s_%s", instanceId, expr->value);
    }
    else if (expr->type == AST_INTEGER_LITERAL) fprintf(out, "%s", expr->value);
    else if (expr->type == AST_STRING_LITERAL) fprintf(out, "\"%s\"", expr->value);
    else if (expr->type == AST_INFIX_EXPRESSION) {
        emitJSExpr(expr->left, inst, out);
        fprintf(out, " %s ", expr->value);
        emitJSExpr(expr->right, inst, out);
    } else if (expr->type == AST_ARRAY_LITERAL) {
        fprintf(out, "[");
        for (int i = 0; i < expr->statementCount; i++) {
            emitJSExpr(expr->statements[i], inst, out);
            if (i < expr->statementCount - 1) fprintf(out, ", ");
        }
        fprintf(out, "]");
    } else if (expr->type == AST_INDEX_EXPRESSION) {
        emitJSExpr(expr->left, inst, out);
        fprintf(out, "[");
        emitJSExpr(expr->right, inst, out);
        fprintf(out, "]");
    } else if (expr->type == AST_FUNCTION_CALL) {
        char* funcName = (expr->left && expr->left->type == AST_IDENTIFIER) ? expr->left->value : "";
        if (strcmp(funcName, "js") == 0) {
            if (expr->parameterCount > 0 && expr->parameters[0]->type == AST_STRING_LITERAL) {
                fprintf(out, "%s", expr->parameters[0]->value);
            }
            return;
        } else if (strcmp(funcName, "fetch") == 0) {
            fprintf(out, "(await fetch(");
            for (int i = 0; i < expr->parameterCount; i++) {
                emitJSExpr(expr->parameters[i], inst, out);
                if (i < expr->parameterCount - 1) fprintf(out, ", ");
            }
            fprintf(out, ").then(r => r.json()))");
            return;
        } else if (strcmp(funcName, "sleep") == 0) {
            fprintf(out, "(await new Promise(r => setTimeout(r, ");
            if (expr->parameterCount > 0) emitJSExpr(expr->parameters[0], inst, out);
            else fprintf(out, "0");
            fprintf(out, ")))");
            return;
        } else if (strcmp(funcName, "emit") == 0) {
            if (expr->parameterCount >= 1 && expr->parameters[0]->type == AST_STRING_LITERAL) {
                char* eventName = expr->parameters[0]->value;
                char* parentCallback = NULL;
                if (inst && inst->instanceEl) {
                    for (int j = 0; j < inst->instanceEl->attributeCount; j++) {
                        if (strcmp(inst->instanceEl->attributes[j]->propertyName, eventName) == 0) {
                            parentCallback = inst->instanceEl->attributes[j]->value;
                            break;
                        }
                    }
                }
                if (parentCallback) {
                    fprintf(out, "window['%s_%s'](", eventName, instanceId);
                    if (expr->parameterCount > 1) {
                        emitJSExpr(expr->parameters[1], inst, out);
                    }
                    fprintf(out, ")");
                    return;
                }
            }
        }
        
        if (expr->left && expr->left->type == AST_INDEX_EXPRESSION && expr->left->left && expr->left->left->type == AST_IDENTIFIER && strcmp(expr->left->left->value, "json") == 0) {
            fprintf(out, "JSON");
            fprintf(out, "[");
            emitJSExpr(expr->left->right, inst, out);
            fprintf(out, "]");
        } else if (expr->left && expr->left->type == AST_INDEX_EXPRESSION && expr->left->left && expr->left->left->type == AST_IDENTIFIER && strcmp(expr->left->left->value, "env") == 0) {
            if (expr->parameterCount > 0 && expr->parameters[0]->type == AST_STRING_LITERAL) {
                char* envVal = getenv(expr->parameters[0]->value);
                fprintf(out, "\"%s\"", envVal ? envVal : "");
            } else {
                fprintf(out, "\"\"");
            }
            return;
        } else if (expr->left && expr->left->type == AST_INDEX_EXPRESSION && expr->left->left && expr->left->left->type == AST_IDENTIFIER && strcmp(expr->left->left->value, "ws") == 0) {
            if (expr->left->right && expr->left->right->type == AST_STRING_LITERAL && strcmp(expr->left->right->value, "connect") == 0) {
                fprintf(out, "new WebSocket(");
                for (int i = 0; i < expr->parameterCount; i++) {
                    emitJSExpr(expr->parameters[i], inst, out);
                    if (i < expr->parameterCount - 1) fprintf(out, ", ");
                }
                fprintf(out, ")");
                return;
            }
        } else {
            emitJSExpr(expr->left, inst, out);
        }
        
        fprintf(out, "(");
        for (int i = 0; i < expr->parameterCount; i++) {
            emitJSExpr(expr->parameters[i], inst, out);
            if (i < expr->parameterCount - 1) fprintf(out, ", ");
        }
        fprintf(out, ")");
    }
}

static void emitJSState(ASTNode* setStmt, ComponentInstance* inst, FILE* out) {
    if (setStmt->left->type != AST_IDENTIFIER) {
        emitJSExpr(setStmt->left, inst, out);
        fprintf(out, " = ");
        emitJSExpr(setStmt->right, inst, out);
        fprintf(out, ";\n");
        return;
    }
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
        emitJSExpr(setStmt->right, inst, out);
    }
    fprintf(out, ";\n");
}

static void emitJSAction(ASTNode* actionBlock, ComponentInstance* inst, FILE* out) {
    char* instanceId = inst->instanceId;
    if (actionBlock->typeAnnotation) {
        fprintf(out, "async function %s_%s(%s_%s) {\n", instanceId, actionBlock->value, instanceId, actionBlock->typeAnnotation);
    } else {
        fprintf(out, "async function %s_%s() {\n", instanceId, actionBlock->value);
    }
    ASTNode* block = actionBlock->right;
    for (int i = 0; i < block->statementCount; i++) {
        ASTNode* stmt = block->statements[i];
        if (stmt->type == AST_SET_STATEMENT) {
            fprintf(out, "    ");
            emitJSExpr(stmt->left, inst, out);
            fprintf(out, " = ");
            emitJSExpr(stmt->right, inst, out);
            fprintf(out, ";\n");
        } else if (stmt->type == AST_EXPRESSION_STATEMENT) {
            fprintf(out, "    ");
            emitJSExpr(stmt->left, inst, out);
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
        if (el->type == AST_EXPRESSION_STATEMENT && el->left && (el->left->type == AST_CALL_EXPRESSION || el->left->type == AST_FUNCTION_CALL)) {
            ASTNode* arg = (el->left->type == AST_FUNCTION_CALL && el->left->parameterCount > 0) ? el->left->parameters[0] : el->left->right;
            if (arg && arg->type == AST_STRING_LITERAL) {
                fprintf(out, "%s ", arg->value);
            } else if (arg->type == AST_IDENTIFIER) {
                if (strcmp(arg->value, loopVar) == 0) {
                    fprintf(out, "${%s} ", loopVar);
                } else {
                    fprintf(out, "${%s_%s} ", instanceId, arg->value);
                }
            } else if (arg->type == AST_INDEX_EXPRESSION) {
                if (arg->left->type == AST_IDENTIFIER && arg->right->type == AST_STRING_LITERAL) {
                    fprintf(out, "${%s[\"%s\"]} ", arg->left->value, arg->right->value);
                }
            }
        } else if (el->type == AST_UI_ELEMENT) {
            char* tag = el->value;
            int isLayout = 0;
            if (strcmp(tag, "layout") == 0) {
                tag = "div";
                isLayout = 1;
            }
            fprintf(out, "<%s", tag);
            if (isLayout) {
                fprintf(out, " class=\"gware-layout\"");
            }
            for (int a = 0; a < el->attributeCount; a++) {
                fprintf(out, " %s=\"%s\"", el->attributes[a]->propertyName, el->attributes[a]->value);
            }
            fprintf(out, ">");
            if (el->right && el->right->type == AST_BLOCK_STATEMENT) {
                emitJSTemplate(el->right, instanceId, loopVar, out);
            }
            fprintf(out, "</%s>", tag);
        }
    }
}

static void emitUpdateLoops(ASTNode* viewBlock, ComponentInstance* inst, FILE* out) {
    char* instanceId = inst->instanceId;
    for (int i = 0; i < viewBlock->statementCount; i++) {
        ASTNode* el = viewBlock->statements[i];
        if (el->type == AST_IF_STATEMENT) {
            char* ifId = el->typeAnnotation;
            if (ifId) {
                fprintf(out, "    {\n");
                fprintf(out, "        let container = document.getElementById('%s');\n", ifId);
                fprintf(out, "        if (container) {\n");
                fprintf(out, "            if (");
                emitJSExpr(el->left, inst, out);
                fprintf(out, ") {\n");
                fprintf(out, "                updateDOMHtml(container, `");
                emitJSTemplate(el->right, instanceId, NULL, out);
                fprintf(out, "`);\n");
                fprintf(out, "            } else {\n");
                fprintf(out, "                updateDOMHtml(container, '');\n");
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
                    int isStore = 0;
                    if (globalProgram) {
                        for (int s = 0; s < globalProgram->statementCount; s++) {
                            if (globalProgram->statements[s]->type == AST_STORE_DECLARATION) {
                                if (strncmp(inAttr, globalProgram->statements[s]->value, strlen(globalProgram->statements[s]->value)) == 0) {
                                    isStore = 1;
                                    break;
                                }
                            }
                        }
                    }
                    if (isStore) {
                        fprintf(out, "        if (%s) {\n", inAttr);
                        fprintf(out, "            for (let %s of %s) {\n", asAttr, inAttr);
                    } else {
                        fprintf(out, "        if (%s_%s) {\n", instanceId, inAttr);
                        fprintf(out, "            for (let %s of %s_%s) {\n", asAttr, instanceId, inAttr);
                    }
                    fprintf(out, "                html += `");
                    emitJSTemplate(el->right, instanceId, asAttr, out);
                    fprintf(out, "`;\n");
                    fprintf(out, "            }\n");
                    fprintf(out, "        }\n");
                    fprintf(out, "        let container = document.getElementById('%s');\n", loopId);
                    fprintf(out, "        if (container) updateDOMHtml(container, html);\n");
                    fprintf(out, "    }\n");
                }
            } else if (el->right && el->right->type == AST_BLOCK_STATEMENT) {
                emitUpdateLoops(el->right, inst, out);
            }
        }
    }
}

static void emitHTMLView(ASTNode* viewBlock, char* compName, char* instanceId, ASTNode* program, FILE* out) {
    for (int i = 0; i < viewBlock->statementCount; i++) {
        ASTNode* el = viewBlock->statements[i];
        if (el->type == AST_EXPRESSION_STATEMENT && el->left && (el->left->type == AST_CALL_EXPRESSION || el->left->type == AST_FUNCTION_CALL)) {
            ASTNode* arg = (el->left->parameterCount > 0) ? el->left->parameters[0] : el->left->right;
            if (arg && arg->type == AST_STRING_LITERAL) {
                fprintf(out, "%s", arg->value);
            } else if (arg && arg->type == AST_IDENTIFIER) {
                fprintf(out, "<span id=\"var_%s_%s\"></span> ", instanceId, arg->value);
            } else if (arg && arg->type == AST_INDEX_EXPRESSION) {
                if (arg->left && arg->right && arg->left->type == AST_IDENTIFIER && arg->right->type == AST_STRING_LITERAL) {
                    fprintf(out, "<span class=\"bind_store_%s_%s\"></span> ", arg->left->value, arg->right->value);
                }
            }
        } else if (el->type == AST_IF_STATEMENT) {
            if (!el->typeAnnotation) {
                el->typeAnnotation = malloc(256);
                sprintf(el->typeAnnotation, "if_%d", loopCounter++);
            }
            fprintf(out, "<div id=\"%s\"></div>\n", el->typeAnnotation);
            
            // Register components inside IF
            if (el->right && el->right->type == AST_BLOCK_STATEMENT) {
                for (int j = 0; j < el->right->statementCount; j++) {
                    ASTNode* subEl = el->right->statements[j];
                    if (subEl->type == AST_UI_ELEMENT && isupper(subEl->value[0])) {
                        ASTNode* subComp = findComponent(program, subEl->value);
                        if (subComp) {
                            char newInstanceId[256];
                            sprintf(newInstanceId, "%s_%d", subEl->value, instanceCount);
                            addInstance(subEl->value, newInstanceId, instanceId, subComp, subEl);
                        }
                    }
                }
            }
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
                    addInstance(el->value, newInstanceId, instanceId, subComp, el);
                    
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
                char* tag = el->value;
                int isLayout = 0;
                int isFile = 0;
                if (strcmp(tag, "layout") == 0) {
                    tag = "div";
                    isLayout = 1;
                }
                fprintf(out, "<%s class=\"%s%s\"", tag, compName, isLayout ? " gware-layout" : "");
                for (int a = 0; a < el->attributeCount; a++) {
                    ASTNode* attr = el->attributes[a];
                    if (strcmp(attr->propertyName, "type") == 0 && strcmp(attr->value, "file") == 0) {
                        isFile = 1;
                    }
                }
                for (int a = 0; a < el->attributeCount; a++) {
                    ASTNode* attr = el->attributes[a];
                    if (strcmp(attr->propertyName, "onClick") == 0 || strcmp(attr->propertyName, "onclick") == 0) {
                        fprintf(out, " onclick=\"%s_%s()\"", instanceId, attr->value);
                    } else if (strcmp(attr->propertyName, "bind") == 0) {
                        if (isFile) {
                            fprintf(out, " id=\"var_%s_%s_input\" onchange=\"let f = this.files[0]; if(f){ let r = new FileReader(); r.onload = e => { %s_%s = e.target.result; updateDOM(); }; r.readAsDataURL(f); }\"", instanceId, attr->value, instanceId, attr->value);
                        } else {
                            fprintf(out, " id=\"var_%s_%s_input\" oninput=\"%s_%s = this.value; updateDOM()\"", instanceId, attr->value, instanceId, attr->value);
                        }
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

void Transpiler_transpileToWeb(ASTNode* program, char* outputFile) {
    if (!program) return;
    globalProgram = program;
    
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
        addInstance(rootComp->value, rootInstanceId, NULL, rootComp, NULL);
        
        ASTNode* rootView = NULL;
        for (int j = 0; j < rootComp->statementCount; j++) {
            if (rootComp->statements[j]->type == AST_VIEW_DECLARATION) {
                rootView = rootComp->statements[j];
            }
        }
        if (rootView) emitHTMLView(rootView, rootComp->value, rootInstanceId, program, out);
    }
    
    fprintf(out, "</div>\n<script>\n");
    
    // 2.5 Pass: Emit Stores
    for (int i = 0; i < program->statementCount; i++) {
        if (program->statements[i]->type == AST_STORE_DECLARATION) {
            ASTNode* store = program->statements[i];
            char* storeName = store->value;
            fprintf(out, "const _%s_data = {};\n", storeName);
            ASTNode* obj = store->right;
            if (obj && obj->type == AST_OBJECT_LITERAL) {
                for (int j = 0; j < obj->statementCount; j += 2) {
                    char* key = obj->statements[j]->value;
                    ASTNode* valNode = obj->statements[j+1];
                    fprintf(out, "_%s_data[\"%s\"] = ", storeName, key);
                    emitJSExpr(valNode, NULL, out);
                    fprintf(out, ";\n");
                }
            }
            fprintf(out, "const %s = new Proxy(_%s_data, {\n", storeName, storeName);
            fprintf(out, "    set(target, property, value) {\n");
            fprintf(out, "        target[property] = value;\n");
            fprintf(out, "        updateDOM();\n");
            fprintf(out, "        return true;\n");
            fprintf(out, "    }\n");
            fprintf(out, "});\n");
        }
    }
    
    // 3. Pass: Emit JS for all collected instances
    for (int k = 0; k < instanceCount; k++) {
        ComponentInstance inst = instances[k];
        for (int j = 0; j < inst.compNode->statementCount; j++) {
            if (inst.compNode->statements[j]->type == AST_SET_STATEMENT) {
                emitJSState(inst.compNode->statements[j], &instances[k], out);
            } else if (inst.compNode->statements[j]->type == AST_ACTION_DECLARATION) {
                emitJSAction(inst.compNode->statements[j], &instances[k], out);
            }
        }
    }
    
    // 4. Emit DOM Update function
    fprintf(out, "function updateDOM() {\n");
    for (int k = 0; k < instanceCount; k++) {
        ComponentInstance inst = instances[k];
        for (int j = 0; j < inst.compNode->statementCount; j++) {
            if (inst.compNode->statements[j]->type == AST_SET_STATEMENT) {
                if (inst.compNode->statements[j]->left->type != AST_IDENTIFIER) continue;
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
                emitUpdateLoops(inst.compNode->statements[j], &instances[k], out);
            }
        }
    }
    
    // 6. Emit Store updates
    if (globalProgram) {
        for (int i = 0; i < globalProgram->statementCount; i++) {
            if (globalProgram->statements[i]->type == AST_STORE_DECLARATION) {
                ASTNode* store = globalProgram->statements[i];
                char* storeName = store->value;
                ASTNode* obj = store->right;
                if (obj && obj->type == AST_OBJECT_LITERAL) {
                    for (int j = 0; j < obj->statementCount; j += 2) {
                        char* key = obj->statements[j]->value;
                        fprintf(out, "    document.querySelectorAll('.bind_store_%s_%s').forEach(el => el.innerText = %s[\"%s\"]);\n", storeName, key, storeName, key);
                    }
                }
            }
        }
    }
    
    fprintf(out, "}\n");
    
    // Emit deep differ
    fprintf(out, "function diffDOM(realNode, tempNode) {\n");
    fprintf(out, "    if (realNode.nodeType === 3) { if (realNode.nodeValue !== tempNode.nodeValue) realNode.nodeValue = tempNode.nodeValue; return; }\n");
    fprintf(out, "    if (realNode.nodeType === 1 && realNode.nodeName === tempNode.nodeName) {\n");
    fprintf(out, "        for (let i = 0; i < tempNode.attributes.length; i++) {\n");
    fprintf(out, "            let attr = tempNode.attributes[i];\n");
    fprintf(out, "            if (realNode.getAttribute(attr.name) !== attr.value) realNode.setAttribute(attr.name, attr.value);\n");
    fprintf(out, "        }\n");
    fprintf(out, "        for (let i = 0; i < tempNode.childNodes.length; i++) {\n");
    fprintf(out, "            if (i < realNode.childNodes.length) diffDOM(realNode.childNodes[i], tempNode.childNodes[i]);\n");
    fprintf(out, "            else realNode.appendChild(tempNode.childNodes[i].cloneNode(true));\n");
    fprintf(out, "        }\n");
    fprintf(out, "        while (realNode.childNodes.length > tempNode.childNodes.length) realNode.removeChild(realNode.lastChild);\n");
    fprintf(out, "    } else {\n");
    fprintf(out, "        realNode.replaceWith(tempNode.cloneNode(true));\n");
    fprintf(out, "    }\n");
    fprintf(out, "}\n");
    fprintf(out, "function updateDOMHtml(container, html) {\n");
    fprintf(out, "    let temp = document.createElement(container.nodeName);\n");
    fprintf(out, "    temp.innerHTML = html;\n");
    fprintf(out, "    for (let i = 0; i < temp.childNodes.length; i++) {\n");
    fprintf(out, "        if (i < container.childNodes.length) diffDOM(container.childNodes[i], temp.childNodes[i]);\n");
    fprintf(out, "        else container.appendChild(temp.childNodes[i].cloneNode(true));\n");
    fprintf(out, "    }\n");
    fprintf(out, "    while (container.childNodes.length > temp.childNodes.length) container.removeChild(container.lastChild);\n");
    fprintf(out, "}\n");
    
    // Bind Event Emitters
    for (int k = 0; k < instanceCount; k++) {
        ComponentInstance inst = instances[k];
        if (inst.instanceEl && inst.parentInstanceId) {
            for (int a = 0; a < inst.instanceEl->attributeCount; a++) {
                char* propName = inst.instanceEl->attributes[a]->propertyName;
                char* propValue = inst.instanceEl->attributes[a]->value;
                if (propName && propValue && strncmp(propName, "on", 2) == 0) {
                    fprintf(out, "window['%s_%s'] = %s_%s;\n", propName, inst.instanceId, inst.parentInstanceId, propValue);
                }
            }
        }
    }
    
    fprintf(out, "updateDOM();\n");
    
    // 7. Emit Router initialization
    if (globalProgram) {
        for (int i = 0; i < globalProgram->statementCount; i++) {
            if (globalProgram->statements[i]->type == AST_ROUTER_DECLARATION) {
                ASTNode* router = globalProgram->statements[i];
                fprintf(out, "    const gwareRoutes = {\n");
                for (int j = 0; j < router->statementCount; j++) {
                    ASTNode* route = router->statements[j];
                    if (route->propertyName && route->value) {
                        fprintf(out, "        \"%s\": \"%s\",\n", route->propertyName, route->value);
                    }
                }
                fprintf(out, "    };\n");
                fprintf(out, "    function renderRouter() {\n");
                fprintf(out, "        let path = window.location.pathname;\n");
                fprintf(out, "        let comp = gwareRoutes[path] || gwareRoutes[\"/\"];\n");
                fprintf(out, "        let view = document.getElementById('app');\n");
                fprintf(out, "        if (view) {\n");
                fprintf(out, "            Array.from(view.children).forEach(c => c.style.display = 'none');\n");
                fprintf(out, "            let active = view.querySelector(`.${comp}`);\n");
                fprintf(out, "            if (active) active.style.display = 'block';\n");
                fprintf(out, "        }\n");
                fprintf(out, "    }\n");
                fprintf(out, "    window.addEventListener('popstate', renderRouter);\n");
                fprintf(out, "    function navigate(url) { history.pushState(null, '', url); renderRouter(); }\n");
                fprintf(out, "    document.addEventListener('click', e => {\n");
                fprintf(out, "        let a = e.target.closest('a');\n");
                fprintf(out, "        if (a && a.getAttribute('href') && a.getAttribute('href').startsWith('/')) {\n");
                fprintf(out, "            e.preventDefault(); navigate(a.getAttribute('href'));\n");
                fprintf(out, "        }\n");
                fprintf(out, "    });\n");
                fprintf(out, "    renderRouter();\n");
            }
        }
    }
    
    fprintf(out, "    // HMR Snippet\n");
    fprintf(out, "    const _hmr_ws = new WebSocket('ws://' + window.location.host + '/__hmr');\n");
    fprintf(out, "    _hmr_ws.onmessage = (e) => {\n");
    fprintf(out, "        if (e.data === 'RELOAD') { window.location.reload(); }\n");
    fprintf(out, "    };\n");
    fprintf(out, "</script>\n</body>\n</html>\n");
    fclose(out);
    printf(ANSI_COLOR_GREEN "Successfully transpiled to %s\n" ANSI_COLOR_RESET, outputFile);
}

