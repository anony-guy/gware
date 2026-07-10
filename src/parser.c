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

static int getPrecedence(TokenType type) {
    switch (type) {
        case TOKEN_EQUALS: return PREC_EQUALS;
        case TOKEN_LESS: return PREC_LESSGREATER;
        case TOKEN_GREATER: return PREC_LESSGREATER;
        case TOKEN_PLUS: return PREC_SUM;
        case TOKEN_MINUS: return PREC_SUM;
        case TOKEN_STAR: return PREC_PRODUCT;
        case TOKEN_SLASH: return PREC_PRODUCT;
        case TOKEN_LBRACKET: return PREC_INDEX;
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

static ASTNode* parsePrimary(Parser* p) {
    ASTNode* node = NULL;
    if (p->curToken.type == TOKEN_IDENTIFIER) {
        if (p->peekToken.type == TOKEN_LPAREN) {
            node = ASTNode_create(AST_FUNCTION_CALL);
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
            node = ASTNode_create(AST_IDENTIFIER);
            node->value = strdup(p->curToken.literal);
            nextToken(p);
        }
    } else if (p->curToken.type == TOKEN_NUMBER) {
        node = ASTNode_create(AST_INTEGER_LITERAL);
        node->value = strdup(p->curToken.literal);
        nextToken(p);
    } else if (p->curToken.type == TOKEN_STRING) {
        node = ASTNode_create(AST_STRING_LITERAL);
        node->value = strdup(p->curToken.literal);
        nextToken(p);
    } else if (p->curToken.type == TOKEN_LBRACKET) {
        node = ASTNode_create(AST_ARRAY_LITERAL);
        nextToken(p); // consume [
        while (p->curToken.type != TOKEN_RBRACKET && p->curToken.type != TOKEN_EOF) {
            ASTNode* elem = parseExpression(p, PREC_LOWEST);
            if (elem) ASTNode_addStatement(node, elem); // reuse statements array
            if (p->curToken.type == TOKEN_COMMA) nextToken(p);
        }
        if (p->curToken.type == TOKEN_RBRACKET) nextToken(p);
    } else {
        printf(ANSI_COLOR_RED "Parse error: expected identifier, number, or string, got type %d ('%s')\n" ANSI_COLOR_RESET, p->curToken.type, p->curToken.literal);
        nextToken(p);
    }
    return node;
}

static ASTNode* parseExpression(Parser* p, int precedence) {
    ASTNode* left = parsePrimary(p);
    if (!left) return NULL;
    
    while (p->curToken.type != TOKEN_EOF && precedence < getPrecedence(p->curToken.type)) {
        Token opToken = p->curToken;
        
        if (opToken.type == TOKEN_LBRACKET) {
            nextToken(p); // consume [
            ASTNode* expr = ASTNode_create(AST_INDEX_EXPRESSION);
            expr->left = left;
            expr->right = parseExpression(p, PREC_LOWEST);
            if (p->curToken.type == TOKEN_RBRACKET) nextToken(p);
            left = expr;
            continue;
        }
        
        ASTNode* expr = ASTNode_create(AST_INFIX_EXPRESSION);
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
    ASTNode* stmt = ASTNode_create(AST_SET_STATEMENT);
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
    ASTNode* block = ASTNode_create(AST_BLOCK_STATEMENT);
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
    ASTNode* stmt = ASTNode_create(AST_IF_STATEMENT);
    nextToken(p);
    stmt->left = parseExpression(p, PREC_LOWEST);
    if (p->curToken.type == TOKEN_LBRACE) stmt->right = parseBlockStatement(p);
    return stmt;
}

static ASTNode* parseWhileStatement(Parser* p) {
    ASTNode* stmt = ASTNode_create(AST_WHILE_STATEMENT);
    nextToken(p);
    stmt->left = parseExpression(p, PREC_LOWEST);
    if (p->curToken.type == TOKEN_LBRACE) stmt->right = parseBlockStatement(p);
    return stmt;
}

static ASTNode* parseFunctionDeclaration(Parser* p) {
    ASTNode* func = ASTNode_create(AST_FUNCTION_DECLARATION);
    nextToken(p); // consume 'def'
    func->value = strdup(p->curToken.literal);
    nextToken(p); // consume name
    
    if (p->curToken.type == TOKEN_LPAREN) {
        nextToken(p); // consume '('
        while (p->curToken.type != TOKEN_RPAREN && p->curToken.type != TOKEN_EOF) {
            ASTNode* param = ASTNode_create(AST_IDENTIFIER);
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
    ASTNode* ret = ASTNode_create(AST_RETURN_STATEMENT);
    nextToken(p); // consume 'return'
    if (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
        ret->left = parseExpression(p, PREC_LOWEST);
    }
    return ret;
}

static ASTNode* parseExpressionStatement(Parser* p) {
    if (p->curToken.type == TOKEN_SHOW) {
        ASTNode* stmt = ASTNode_create(AST_EXPRESSION_STATEMENT);
        ASTNode* call = ASTNode_create(AST_CALL_EXPRESSION);
        call->left = ASTNode_create(AST_IDENTIFIER);
        call->left->value = strdup("show");
        
        nextToken(p);
        if (p->curToken.type != TOKEN_LPAREN) return NULL;
        nextToken(p);
        
        call->right = parseExpression(p, PREC_LOWEST);
        
        if (p->curToken.type != TOKEN_RPAREN) return NULL;
        nextToken(p);
        
        stmt->left = call;
        return stmt;
    }
    ASTNode* stmt = ASTNode_create(AST_EXPRESSION_STATEMENT);
    stmt->left = parseExpression(p, PREC_LOWEST);
    return stmt;
}

// GwareWeb Parsing
static ASTNode* parseActionDeclaration(Parser* p) {
    ASTNode* action = ASTNode_create(AST_ACTION_DECLARATION);
    nextToken(p); // consume 'action'
    action->value = strdup(p->curToken.literal);
    nextToken(p); // consume name
    action->right = parseBlockStatement(p);
    return action;
}

static ASTNode* parseStyleDeclaration(Parser* p) {
    ASTNode* style = ASTNode_create(AST_STYLE_DECLARATION);
    nextToken(p); // consume 'style'
    nextToken(p); // consume '{'
    
    while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
        ASTNode* prop = ASTNode_create(AST_CSS_PROPERTY);
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
    ASTNode* el = ASTNode_create(AST_UI_ELEMENT);
    el->value = strdup(p->curToken.literal);
    nextToken(p); // consume tag name
    
    if (p->curToken.type == TOKEN_LPAREN) {
        nextToken(p); // consume '('
        while (p->curToken.type != TOKEN_RPAREN && p->curToken.type != TOKEN_EOF) {
            ASTNode* attr = ASTNode_create(AST_IDENTIFIER);
            attr->propertyName = strdup(p->curToken.literal);
            nextToken(p); // consume attr name
            if (p->curToken.type == TOKEN_COLON || p->curToken.type == TOKEN_ASSIGN) nextToken(p);
            attr->value = strdup(p->curToken.literal); // attr value
            nextToken(p); // consume attr value
            ASTNode_addAttribute(el, attr);
            // Ignore commas if present
            if (p->curToken.literal && strcmp(p->curToken.literal, ",") == 0) nextToken(p);
        }
        nextToken(p); // consume ')'
    }
    
    if (p->curToken.type == TOKEN_LBRACE) {
        el->right = parseBlockStatement(p); // body block containing inner elements or show()
    }
    return el;
}

static ASTNode* parseViewDeclaration(Parser* p) {
    ASTNode* view = ASTNode_create(AST_VIEW_DECLARATION);
    nextToken(p); // consume 'view'
    nextToken(p); // consume '{'
    while (p->curToken.type != TOKEN_RBRACE && p->curToken.type != TOKEN_EOF) {
        if (p->curToken.type == TOKEN_SHOW) {
            ASTNode_addStatement(view, parseExpressionStatement(p));
        } else {
            ASTNode_addStatement(view, parseUIElement(p));
        }
    }
    nextToken(p); // consume '}'
    return view;
}

static ASTNode* parseComponentDeclaration(Parser* p) {
    ASTNode* comp = ASTNode_create(AST_COMPONENT_DECLARATION);
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

static ASTNode* parseStatement(Parser* p) {
    if (p->curToken.type == TOKEN_SET) return parseSetStatement(p);
    if (p->curToken.type == TOKEN_IF) return parseIfStatement(p);
    if (p->curToken.type == TOKEN_WHILE) return parseWhileStatement(p);
    if (p->curToken.type == TOKEN_DEF) return parseFunctionDeclaration(p);
    if (p->curToken.type == TOKEN_RETURN) return parseReturnStatement(p);
    if (p->curToken.type == TOKEN_COMPONENT) return parseComponentDeclaration(p);
    return parseExpressionStatement(p);
}

ASTNode* Parser_parseProgram(Parser* p) {
    ASTNode* program = ASTNode_create(AST_PROGRAM);
    while (p->curToken.type != TOKEN_EOF) {
        ASTNode* stmt = parseStatement(p);
        if (stmt) ASTNode_addStatement(program, stmt);
        else nextToken(p);
    }
    return program;
}
