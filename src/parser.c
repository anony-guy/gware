#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "color.h"

enum {
    PREC_LOWEST = 1,
    PREC_EQUALS,
    PREC_LESSGREATER,
    PREC_SUM,
    PREC_PRODUCT,
    PREC_CALL,
    PREC_INDEX
};

static int getPrecedence(GwTokenType type) {
    switch (type) {
        case TOKEN_EQUALS: 
        case TOKEN_NOT_EQ: return PREC_EQUALS;
        case TOKEN_LESS: return PREC_LESSGREATER;
        case TOKEN_GREATER: return PREC_LESSGREATER;
        case TOKEN_PLUS: return PREC_SUM;
        case TOKEN_MINUS: return PREC_SUM;
        case TOKEN_STAR:
        case TOKEN_SLASH: return PREC_PRODUCT;
        case TOKEN_LBRACKET: return PREC_CALL;
        case TOKEN_LPAREN: return PREC_CALL;
        case TOKEN_DOT: return PREC_CALL;
        default: return PREC_LOWEST;
    }
}

static void nextToken(Parser* p) {
    if (p->curToken.literal) free(p->curToken.literal);
    p->curToken = p->peekToken;
    p->peekToken = Lexer_nextToken(p->l);
}

Parser* Parser_create(Lexer* l) {
    Parser* p = (Parser*)malloc(sizeof(Parser));
    p->l = l;
    p->curToken.literal = NULL;
    p->peekToken.literal = NULL;
    p->peekToken = Lexer_nextToken(p->l);
    nextToken(p);
    return p;
}

void Parser_destroy(Parser* p) {
    if (p->curToken.literal) free(p->curToken.literal);
    if (p->peekToken.literal) free(p->peekToken.literal);
    free(p);
}

static ASTNode* parseExpression(Parser* p, int precedence);
static ASTNode* parseStatement(Parser* p);
static ASTNode* parseBlockStatement(Parser* p);
static ASTNode* parseUIIfStatement(Parser* p);

static ASTNode* parsePrimary(Parser* p) {
    ASTNode* node = NULL;
    if (p->curToken.type == TOKEN_IDENTIFIER) {
        if (p->peekToken.type == TOKEN_LPAREN) {
            node = ASTNode_create_loc(AST_FUNCTION_CALL, p->curToken.line, p->curToken.file);
            node->value = strdup(p->curToken.literal);
            nextToken(p); // consume identifier
            nextToken(p); // consume (
            while (p->curToken.type != TOKEN_RPAREN && p->curToken.type != TOKEN_EOF) {
                ASTNode* arg = parseExpression(p, PREC_LOWEST);
                if (arg) ASTNode_addParameter(node, arg);
                if (p->curToken.type == TOKEN_COMMA) nextToken(p);
            }
            if (p->curToken.type == TOKEN_RPAREN) nextToken(p);
        } else {
            node = ASTNode_create_loc(AST_IDENTIFIER, p->curToken.line, p->curToken.file);
            node->value = strdup(p->curToken.literal);
            nextToken(p);
        }
    } else if (p->curToken.type == TOKEN_NUMBER) {
        node = ASTNode_create_loc(AST_INTEGER_LITERAL, p->curToken.line, p->curToken.file);
        node->value = strdup(p->curToken.literal);
        nextToken(p);
    } else if (p->curToken.type == TOKEN_STRING) {
        node = ASTNode_create_loc(AST_STRING_LITERAL, p->curToken.line, p->curToken.file);
        node->value = strdup(p->curToken.literal);
        nextToken(p);
    } else if (p->curToken.type == TOKEN_LBRACKET) {
        node = ASTNode_create_loc(AST_ARRAY_LITERAL, p->curToken.line, p->curToken.file);
        nextToken(p); // consume [
        while (p->curToken.type != TOKEN_RBRACKET && p->curToken.type != TOKEN_EOF) {
            ASTNode* elem = parseExpression(p, PREC_LOWEST);
            if (elem) ASTNode_addStatement(node, elem); // reuse statements array
            if (p->curToken.type == TOKEN_COMMA) nextToken(p);
        }
        if (p->curToken.type == TOKEN_RBRACKET) nextToken(p);
    } else if (p->curToken.type == TOKEN_LBRACE) {
        node = ASTNode_create_loc(AST_OBJECT_LITERAL, p->curToken.line, p->curToken.file);
        nextToken(p); // consume {
        while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
            ASTNode* key = NULL;
            if (p->curToken.type == TOKEN_STRING) {
                key = ASTNode_create_loc(AST_STRING_LITERAL, p->curToken.line, p->curToken.file);
                key->value = strdup(p->curToken.literal);
            } else if (p->curToken.type == TOKEN_IDENTIFIER) {
                key = ASTNode_create_loc(AST_STRING_LITERAL, p->curToken.line, p->curToken.file);
                key->value = strdup(p->curToken.literal);
            } else {
                printf(ANSI_COLOR_RED "Parse error: expected string or identifier for object key, got '%s'\n" ANSI_COLOR_RESET, p->curToken.literal);
                nextToken(p);
                continue;
            }
            nextToken(p); // consume key
            if (p->curToken.type != TOKEN_COLON) {
                printf(ANSI_COLOR_RED "Parse error: expected ':' after object key, got '%s'\n" ANSI_COLOR_RESET, p->curToken.literal);
            } else {
                nextToken(p); // consume :
            }
            ASTNode* val = parseExpression(p, PREC_LOWEST);
            if (key && val) {
                ASTNode_addStatement(node, key);
                ASTNode_addStatement(node, val);
            }
            if (p->curToken.type == TOKEN_COMMA) nextToken(p);
        }
        if (p->curToken.type == TOKEN_RBRACE) nextToken(p);
    } else if (p->curToken.type == TOKEN_DEF) {
        node = ASTNode_create_loc(AST_FUNCTION_DECLARATION, p->curToken.line, p->curToken.file);
        nextToken(p); // consume 'def'
        if (p->curToken.type == TOKEN_IDENTIFIER) {
            node->value = strdup(p->curToken.literal);
            nextToken(p);
        } else {
            node->value = strdup("anonymous");
        }
        if (p->curToken.type == TOKEN_LPAREN) {
            nextToken(p);
            while (p->curToken.type != TOKEN_RPAREN && p->curToken.type != TOKEN_EOF) {
                ASTNode* param = ASTNode_create_loc(AST_IDENTIFIER, p->curToken.line, p->curToken.file);
                if (p->curToken.type == TOKEN_TYPE_INT || p->curToken.type == TOKEN_TYPE_STRING) {
                    param->typeAnnotation = strdup(p->curToken.literal);
                    nextToken(p);
                }
                param->value = strdup(p->curToken.literal);
                nextToken(p);
                ASTNode_addParameter(node, param);
                if (p->curToken.type == TOKEN_COMMA) nextToken(p);
            }
            if (p->curToken.type == TOKEN_RPAREN) nextToken(p);
        }
        if (p->curToken.type == TOKEN_LBRACE) {
            node->right = parseBlockStatement(p);
        }
    } else {
        printf(ANSI_COLOR_RED "Parse error: expected identifier, number, or string, got type %d ('%s')\n" ANSI_COLOR_RESET, p->curToken.type, p->curToken.literal);
        nextToken(p);
    }
    return node;
}

static ASTNode* parseArrayLiteral(Parser* p) {
    ASTNode* arrayNode = ASTNode_create_loc(AST_ARRAY_LITERAL, p->curToken.line, p->curToken.file);
    nextToken(p); // consume '['
    while (p->curToken.type != TOKEN_RBRACKET && p->curToken.type != TOKEN_EOF) {
        ASTNode* expr = parseExpression(p, 0); // LOWEST precedence
        if (expr) ASTNode_addStatement(arrayNode, expr);
        nextToken(p); // consume expr
        if (p->curToken.type == TOKEN_COMMA) {
            nextToken(p); // consume ','
        }
    }
    return arrayNode;
}

static ASTNode* parseExpression(Parser* p, int precedence) {
    ASTNode* left = parsePrimary(p);
    if (!left) return NULL;
    
    while (p->curToken.type != TOKEN_EOF && precedence < getPrecedence(p->curToken.type)) {
        Token opToken = p->curToken;
        
        if (opToken.type == TOKEN_LBRACKET) {
            nextToken(p); // consume [
            ASTNode* expr = ASTNode_create_loc(AST_INDEX_EXPRESSION, p->curToken.line, p->curToken.file);
            expr->left = left;
            expr->right = parseExpression(p, PREC_LOWEST);
            if (p->curToken.type == TOKEN_RBRACKET) nextToken(p);
            left = expr;
            continue;
        }
        
        if (opToken.type == TOKEN_LPAREN) {
            nextToken(p); // consume (
            ASTNode* expr = ASTNode_create_loc(AST_CALL_EXPRESSION, p->curToken.line, p->curToken.file);
            expr->left = left;
            while (p->curToken.type != TOKEN_RPAREN && p->curToken.type != TOKEN_EOF) {
                ASTNode* arg = parseExpression(p, PREC_LOWEST);
                if (arg) ASTNode_addParameter(expr, arg);
                if (p->curToken.type == TOKEN_COMMA) nextToken(p);
            }
            if (p->curToken.type == TOKEN_RPAREN) nextToken(p);
            left = expr;
            continue;
        }
        
        if (opToken.type == TOKEN_DOT) {
            nextToken(p); // consume .
            ASTNode* expr = ASTNode_create_loc(AST_INDEX_EXPRESSION, p->curToken.line, p->curToken.file);
            expr->left = left;
            if (p->curToken.type != TOKEN_IDENTIFIER) {
                printf(ANSI_COLOR_RED "Parse error: expected identifier after '.'\n" ANSI_COLOR_RESET);
            } else {
                expr->right = ASTNode_create_loc(AST_STRING_LITERAL, p->curToken.line, p->curToken.file);
                expr->right->value = strdup(p->curToken.literal);
                nextToken(p); // consume identifier
            }
            left = expr;
            continue;
        }
        
        ASTNode* expr = ASTNode_create_loc(AST_INFIX_EXPRESSION, p->curToken.line, p->curToken.file);
        expr->value = strdup(opToken.literal);
        expr->left = left;
        
        int prec = getPrecedence(opToken.type);
        nextToken(p);
        
        expr->right = parseExpression(p, prec);
        left = expr;
    }
    return left;
}

static ASTNode* parseSetStatement(Parser* p) {
    ASTNode* stmt = ASTNode_create_loc(AST_SET_STATEMENT, p->curToken.line, p->curToken.file);
    nextToken(p); 
    
    if (p->curToken.type == TOKEN_TYPE_INT || p->curToken.type == TOKEN_TYPE_STRING) {
        stmt->typeAnnotation = strdup(p->curToken.literal);
        nextToken(p); 
    }
    
    if (p->curToken.type != TOKEN_IDENTIFIER) {
        printf(ANSI_COLOR_RED "Parse error: expected identifier after set\n" ANSI_COLOR_RESET);
        return NULL;
    }
    
    stmt->left = parseExpression(p, PREC_LOWEST);
    
    if (p->curToken.type != TOKEN_ASSIGN) {
        printf(ANSI_COLOR_RED "Parse error: expected =\n" ANSI_COLOR_RESET);
        return NULL;
    }
    nextToken(p); 
    
    stmt->right = parseExpression(p, PREC_LOWEST);
    return stmt;
}

static ASTNode* parseBlockStatement(Parser* p) {
    ASTNode* block = ASTNode_create_loc(AST_BLOCK_STATEMENT, p->curToken.line, p->curToken.file);
    nextToken(p); // consume {
    while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
        ASTNode* stmt = parseStatement(p);
        if (stmt) ASTNode_addStatement(block, stmt);
    }
    if (p->curToken.type == TOKEN_RBRACE) {
        nextToken(p); // consume }
    }
    return block;
}

static ASTNode* parseIfStatement(Parser* p) {
    ASTNode* stmt = ASTNode_create_loc(AST_IF_STATEMENT, p->curToken.line, p->curToken.file);
    nextToken(p);
    stmt->left = parseExpression(p, PREC_LOWEST);
    if (p->curToken.type == TOKEN_LBRACE) stmt->right = parseBlockStatement(p);
    return stmt;
}

static ASTNode* parseWhileStatement(Parser* p) {
    ASTNode* stmt = ASTNode_create_loc(AST_WHILE_STATEMENT, p->curToken.line, p->curToken.file);
    nextToken(p);
    stmt->left = parseExpression(p, PREC_LOWEST);
    if (p->curToken.type == TOKEN_LBRACE) stmt->right = parseBlockStatement(p);
    return stmt;
}

static ASTNode* parseFunctionDeclaration(Parser* p) {
    ASTNode* func = ASTNode_create_loc(AST_FUNCTION_DECLARATION, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'def'
    func->value = strdup(p->curToken.literal);
    nextToken(p); // consume name
    
    if (p->curToken.type == TOKEN_LPAREN) {
        nextToken(p); // consume '('
        while (p->curToken.type != TOKEN_RPAREN && p->curToken.type != TOKEN_EOF) {
            ASTNode* param = ASTNode_create_loc(AST_IDENTIFIER, p->curToken.line, p->curToken.file);
            if (p->curToken.type == TOKEN_TYPE_INT || p->curToken.type == TOKEN_TYPE_STRING) {
                param->typeAnnotation = strdup(p->curToken.literal);
                nextToken(p);
            }
            param->value = strdup(p->curToken.literal);
            nextToken(p);
            ASTNode_addParameter(func, param);
            
            if (p->curToken.type == TOKEN_COMMA) nextToken(p);
        }
        nextToken(p); // consume ')'
    }
    
    if (p->curToken.type == TOKEN_LBRACE) {
        func->right = parseBlockStatement(p);
    }
    return func;
}

static ASTNode* parseReturnStatement(Parser* p) {
    ASTNode* ret = ASTNode_create_loc(AST_RETURN_STATEMENT, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'return'
    if (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
        ret->left = parseExpression(p, PREC_LOWEST);
    }
    return ret;
}

static ASTNode* parseTryStatement(Parser* p) {
    ASTNode* stmt = ASTNode_create_loc(AST_TRY_STATEMENT, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'try'
    if (p->curToken.type == TOKEN_LBRACE) {
        stmt->left = parseBlockStatement(p);
    }
    
    if (p->curToken.type == TOKEN_CATCH) {
        nextToken(p); // consume 'catch'
        if (p->curToken.type == TOKEN_LPAREN) nextToken(p);
        if (p->curToken.type == TOKEN_IDENTIFIER) {
            stmt->value = strdup(p->curToken.literal);
            nextToken(p);
        }
        if (p->curToken.type == TOKEN_RPAREN) nextToken(p);
        
        if (p->curToken.type == TOKEN_LBRACE) {
            stmt->right = parseBlockStatement(p);
        }
    }
    return stmt;
}

static ASTNode* parseImportStatement(Parser* p) {
    ASTNode* stmt = ASTNode_create_loc(AST_IMPORT_STATEMENT, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'import'
    if (p->curToken.type == TOKEN_STRING) {
        stmt->value = strdup(p->curToken.literal);
        nextToken(p); // consume string
    } else {
        printf(ANSI_COLOR_RED "Parse error: import expects a string literal\n" ANSI_COLOR_RESET);
    }
    return stmt;
}

static ASTNode* parseExpressionStatement(Parser* p) {
    if (p->curToken.type == TOKEN_SHOW) {
        ASTNode* stmt = ASTNode_create_loc(AST_EXPRESSION_STATEMENT, p->curToken.line, p->curToken.file);
        ASTNode* call = ASTNode_create_loc(AST_CALL_EXPRESSION, p->curToken.line, p->curToken.file);
        call->left = ASTNode_create_loc(AST_IDENTIFIER, p->curToken.line, p->curToken.file);
        call->left->value = strdup("show");
        
        nextToken(p);
        if (p->curToken.type != TOKEN_LPAREN) return NULL;
        nextToken(p);
        
        ASTNode* arg = parseExpression(p, PREC_LOWEST);
        if (arg) ASTNode_addParameter(call, arg);
        
        if (p->curToken.type != TOKEN_RPAREN) return NULL;
        nextToken(p);
        
        stmt->left = call;
        return stmt;
    }
    ASTNode* stmt = ASTNode_create_loc(AST_EXPRESSION_STATEMENT, p->curToken.line, p->curToken.file);
    stmt->left = parseExpression(p, PREC_LOWEST);
    return stmt;
}

// GwareWeb Parsing
static ASTNode* parseActionDeclaration(Parser* p) {
    ASTNode* action = ASTNode_create_loc(AST_ACTION_DECLARATION, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'action'
    action->value = strdup(p->curToken.literal);
    nextToken(p); // consume name
    action->right = parseBlockStatement(p);
    return action;
}

static ASTNode* parseStyleDeclaration(Parser* p) {
    ASTNode* style = ASTNode_create_loc(AST_STYLE_DECLARATION, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'style'
    nextToken(p); // consume '{'
    
    while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
        ASTNode* prop = ASTNode_create_loc(AST_CSS_PROPERTY, p->curToken.line, p->curToken.file);
        prop->propertyName = strdup(p->curToken.literal);
        nextToken(p); // consume name
        
        if (p->curToken.type == TOKEN_ASSIGN || p->curToken.type == TOKEN_COLON) nextToken(p);
        
        prop->value = strdup(p->curToken.literal); // could be string or ident
        nextToken(p); // consume value
        
        ASTNode_addStatement(style, prop);
    }
    nextToken(p); // consume '}'
    return style;
}

static ASTNode* parseUIElement(Parser* p) {
    // e.g. button(onClick: increment) { show(clicks) }
    ASTNode* el = ASTNode_create_loc(AST_UI_ELEMENT, p->curToken.line, p->curToken.file);
    el->value = strdup(p->curToken.literal);
    nextToken(p); // consume tag name
    
    if (p->curToken.type == TOKEN_LPAREN) {
        nextToken(p); // consume '('
        while (p->curToken.type != TOKEN_RPAREN && p->curToken.type != TOKEN_EOF) {
            ASTNode* attr = ASTNode_create_loc(AST_IDENTIFIER, p->curToken.line, p->curToken.file);
            attr->propertyName = strdup(p->curToken.literal);
            nextToken(p); // consume attr name
            if (p->curToken.type == TOKEN_COLON || p->curToken.type == TOKEN_ASSIGN) nextToken(p);
            attr->value = strdup(p->curToken.literal); // attr value
            nextToken(p); // consume attr value
            while (p->curToken.type == TOKEN_DOT) {
                nextToken(p);
                char* temp = malloc(strlen(attr->value) + strlen(p->curToken.literal) + 5);
                sprintf(temp, "%s[\"%s\"]", attr->value, p->curToken.literal);
                free(attr->value);
                attr->value = temp;
                nextToken(p);
            }
            ASTNode_addAttribute(el, attr);
            // Ignore commas if present
            if (p->curToken.literal && strcmp(p->curToken.literal, ",") == 0) nextToken(p);
        }
        nextToken(p); // consume ')'
    }
    
    if (p->curToken.type == TOKEN_LBRACE) {
        ASTNode* block = ASTNode_create_loc(AST_BLOCK_STATEMENT, p->curToken.line, p->curToken.file);
        nextToken(p); // consume {
        while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
            if (p->curToken.type == TOKEN_SHOW) {
                ASTNode* exprStmt = parseExpressionStatement(p);
                if (exprStmt) ASTNode_addStatement(block, exprStmt);
            } else if (p->curToken.type == TOKEN_IF) {
                ASTNode* ifStmt = parseUIIfStatement(p);
                if (ifStmt) ASTNode_addStatement(block, ifStmt);
            } else {
                ASTNode* subEl = parseUIElement(p);
                if (subEl) ASTNode_addStatement(block, subEl);
            }
        }
        if (p->curToken.type == TOKEN_RBRACE) nextToken(p);
        el->right = block; // body block containing inner elements or show()
    }
    return el;
}

static ASTNode* parseUIIfStatement(Parser* p) {
    ASTNode* stmt = ASTNode_create_loc(AST_IF_STATEMENT, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'if'
    stmt->left = parseExpression(p, PREC_LOWEST);
    if (p->curToken.type == TOKEN_LBRACE) {
        ASTNode* block = ASTNode_create_loc(AST_BLOCK_STATEMENT, p->curToken.line, p->curToken.file);
        nextToken(p); // consume {
        while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
            if (p->curToken.type == TOKEN_SHOW) {
                ASTNode* exprStmt = parseExpressionStatement(p);
                if (exprStmt) ASTNode_addStatement(block, exprStmt);
            } else if (p->curToken.type == TOKEN_IF) {
                ASTNode* ifStmt = parseUIIfStatement(p);
                if (ifStmt) ASTNode_addStatement(block, ifStmt);
            } else {
                ASTNode* subEl = parseUIElement(p);
                if (subEl) ASTNode_addStatement(block, subEl);
            }
        }
        if (p->curToken.type == TOKEN_RBRACE) nextToken(p);
        stmt->right = block;
    }
    return stmt;
}

static ASTNode* parseViewDeclaration(Parser* p) {
    ASTNode* view = ASTNode_create_loc(AST_VIEW_DECLARATION, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'view'
    nextToken(p); // consume '{'
    while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
        if (p->curToken.type == TOKEN_SHOW) {
            ASTNode_addStatement(view, parseExpressionStatement(p));
        } else if (p->curToken.type == TOKEN_IF) {
            ASTNode_addStatement(view, parseUIIfStatement(p));
        } else {
            ASTNode_addStatement(view, parseUIElement(p));
        }
    }
    nextToken(p); // consume '}'
    return view;
}

static ASTNode* parseComponentDeclaration(Parser* p) {
    ASTNode* comp = ASTNode_create_loc(AST_COMPONENT_DECLARATION, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'component'
    comp->value = strdup(p->curToken.literal);
    nextToken(p); // consume name
    nextToken(p); // consume '{'
    
    while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
        if (p->curToken.type == TOKEN_SET) {
            ASTNode_addStatement(comp, parseSetStatement(p));
        } else if (p->curToken.type == TOKEN_ACTION) {
            ASTNode_addStatement(comp, parseActionDeclaration(p));
        } else if (p->curToken.type == TOKEN_STYLE) {
            ASTNode_addStatement(comp, parseStyleDeclaration(p));
        } else if (p->curToken.type == TOKEN_VIEW) {
            ASTNode_addStatement(comp, parseViewDeclaration(p));
        } else {
            nextToken(p); // skip unknown
        }
    }
    nextToken(p); // consume '}'
    return comp;
}

static ASTNode* parseStoreDeclaration(Parser* p) {
    ASTNode* store = ASTNode_create_loc(AST_STORE_DECLARATION, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'store'
    store->value = strdup(p->curToken.literal);
    nextToken(p); // consume name
    store->right = parseExpression(p, PREC_LOWEST);
    return store;
}

static ASTNode* parseRouterDeclaration(Parser* p) {
    ASTNode* router = ASTNode_create_loc(AST_ROUTER_DECLARATION, p->curToken.line, p->curToken.file);
    nextToken(p); // consume 'router'
    if (p->curToken.type == TOKEN_LBRACE) {
        nextToken(p); // consume '{'
        while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
            if (p->curToken.type == TOKEN_ROUTE) {
                ASTNode* route = ASTNode_create_loc(AST_ROUTE_DECLARATION, p->curToken.line, p->curToken.file);
                nextToken(p); // consume 'route'
                if (p->curToken.type == TOKEN_LPAREN) nextToken(p);
                route->propertyName = strdup(p->curToken.literal); // path (e.g. "/")
                nextToken(p); // consume path
                if (p->curToken.type == TOKEN_COMMA) nextToken(p);
                route->value = strdup(p->curToken.literal); // component name
                nextToken(p); // consume component name
                if (p->curToken.type == TOKEN_RPAREN) nextToken(p);
                ASTNode_addStatement(router, route);
            } else {
                nextToken(p);
            }
        }
        if (p->curToken.type == TOKEN_RBRACE) nextToken(p);
    }
    return router;
}

static ASTNode* parseStatement(Parser* p) {
    if (p->curToken.type == TOKEN_SET) return parseSetStatement(p);
    if (p->curToken.type == TOKEN_IF) return parseIfStatement(p);
    if (p->curToken.type == TOKEN_WHILE) return parseWhileStatement(p);
    if (p->curToken.type == TOKEN_DEF) return parseFunctionDeclaration(p);
    if (p->curToken.type == TOKEN_RETURN) return parseReturnStatement(p);
    if (p->curToken.type == TOKEN_TRY) return parseTryStatement(p);
    if (p->curToken.type == TOKEN_IMPORT) return parseImportStatement(p);
    if (p->curToken.type == TOKEN_COMPONENT) return parseComponentDeclaration(p);
    if (p->curToken.type == TOKEN_STORE) return parseStoreDeclaration(p);
    if (p->curToken.type == TOKEN_ROUTER) return parseRouterDeclaration(p);
    return parseExpressionStatement(p);
}

ASTNode* Parser_parseProgram(Parser* p) {
    ASTNode* program = ASTNode_create_loc(AST_PROGRAM, p->curToken.line, p->curToken.file);
    while (p->curToken.type != TOKEN_EOF) {
        ASTNode* stmt = parseStatement(p);
        if (stmt) ASTNode_addStatement(program, stmt);
        else nextToken(p);
    }
    return program;
}
