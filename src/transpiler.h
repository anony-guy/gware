#ifndef TRANSPILER_H
#define TRANSPILER_H

#include "ast.h"

void Transpiler_transpileToWeb(ASTNode* program, char* outputFile);

#endif
