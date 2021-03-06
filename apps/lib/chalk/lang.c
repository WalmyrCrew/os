/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lang.c

Abstract:

    This module implements the language specification for the Chalk scripting
    language.

Author:

    Evan Green 14-Oct-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chalkp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define YY_DIGITS "[0-9]"
#define YY_OCTAL_DIGITS "[0-7]"
#define YY_NAME0 "[a-zA-Z_]"
#define YY_HEX "[a-fA-F0-9]"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

PSTR ChalkLexerExpressions[] = {
    "/\\*.*?\\*/", // Multiline comment
    "//(\\\\.|[^\n])*", // single line comment
    "break",
    "continue",
    "do",
    "else",
    "for",
    "if",
    "return",
    "while",
    "function",
    "in",
    "null",
    YY_NAME0 "(" YY_NAME0 "|" YY_DIGITS ")*", // identifier
    "0[xX]" YY_HEX "+", // hex integer
    "0" YY_OCTAL_DIGITS "+", // octal integer
    YY_DIGITS "+", // decimal integer
    "L?\"(\\\\.|[^\\\"])*\"", // string literal
    ">>=",
    "<<=",
    "\\+=",
    "-=",
    "\\*=",
    "/=",
    "%=",
    "&=",
    "^=",
    "\\|=",
    "?=",
    ">>",
    "<<",
    "\\+\\+",
    "--",
    "&&",
    "\\|\\|",
    "<=",
    ">=",
    "==",
    "!=",
    ";",
    "\\{",
    "}",
    ",",
    ":",
    "=",
    "\\(",
    "\\)",
    "\\[",
    "]",
    "&",
    "!",
    "~",
    "-",
    "\\+",
    "*",
    "/",
    "%",
    "<",
    ">",
    "^",
    "\\|",
    "\\?",
    NULL
};

PSTR ChalkLexerTokenNames[] = {
    "MultilineComment", // Multiline comment
    "Comment", // single line comment
    "break",
    "continue",
    "do",
    "else",
    "for",
    "if",
    "return",
    "while",
    "function",
    "in",
    "null",
    "ID", // identifier
    "HEXINT", // hex integer
    "OCTINT", // octal integer
    "DECINT", // decimal integer
    "STRING", // string literal
    ">>=",
    "<<=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "&=",
    "^=",
    "|=",
    "?=",
    ">>",
    "<<",
    "++",
    "--",
    "&&",
    "||",
    "<=",
    ">=",
    "==",
    "!=",
    ";",
    "{",
    "}",
    ",",
    ":",
    "=",
    "(",
    ")",
    "[",
    "]",
    "&",
    "!",
    "~",
    "-",
    "+",
    "*",
    "/",
    "%",
    "<",
    ">",
    "^",
    "|",
    "?",
    NULL
};

PSTR ChalkLexerIgnoreExpressions[] = {
    "[ \t\v\r\n\f]",
    NULL
};

ULONG ChalkGrammarListElementList[] = {
    ChalkNodeConditionalExpression, 0,
    ChalkNodeListElementList, ChalkTokenComma,
        ChalkNodeConditionalExpression, 0,

    0
};

ULONG ChalkGrammarList[] = {
    ChalkTokenOpenBracket, ChalkTokenCloseBracket, 0,
    ChalkTokenOpenBracket, ChalkNodeListElementList, ChalkTokenCloseBracket, 0,
    ChalkTokenOpenBracket, ChalkNodeListElementList, ChalkTokenComma,
        ChalkTokenCloseBracket, 0,

    0
};

ULONG ChalkGrammarDictElement[] = {
    ChalkNodeExpression, ChalkTokenColon, ChalkNodeConditionalExpression, 0,
    0
};

ULONG ChalkGrammarDictElementList[] = {
    ChalkNodeDictElement, 0,
    ChalkNodeDictElementList, ChalkTokenComma, ChalkNodeDictElement, 0,
    0
};

ULONG ChalkGrammarDict[] = {
    ChalkTokenOpenBrace, ChalkTokenCloseBrace, 0,
    ChalkTokenOpenBrace, ChalkNodeDictElementList, ChalkTokenCloseBrace, 0,
    ChalkTokenOpenBrace, ChalkNodeDictElementList, ChalkTokenComma,
        ChalkTokenCloseBrace, 0,

    0
};

ULONG ChalkGrammarPrimaryExpression[] = {
    ChalkTokenIdentifier, 0,
    ChalkTokenHexInteger, 0,
    ChalkTokenOctalInteger, 0,
    ChalkTokenDecimalInteger, 0,
    ChalkTokenString, 0,
    ChalkTokenNull, 0,
    ChalkNodeDict, 0,
    ChalkNodeList, 0,
    ChalkTokenOpenParentheses, ChalkNodeExpression, ChalkTokenCloseParentheses,
        0,

    0
};

ULONG ChalkGrammarPostfixExpression[] = {
    ChalkNodePrimaryExpression, 0,
    ChalkNodePostfixExpression, ChalkTokenOpenBracket, ChalkNodeExpression,
        ChalkTokenCloseBracket, 0,

    ChalkNodePostfixExpression, ChalkTokenOpenParentheses,
        ChalkNodeArgumentExpressionList, ChalkTokenCloseParentheses, 0,

    ChalkNodePostfixExpression, ChalkTokenOpenParentheses,
        ChalkTokenCloseParentheses, 0,

    ChalkNodePostfixExpression, ChalkTokenIncrement, 0,
    ChalkNodePostfixExpression, ChalkTokenDecrement, 0,
    0
};

ULONG ChalkGrammarArgumentExpressionList[] = {
    ChalkNodeAssignmentExpression, 0,
    ChalkNodeArgumentExpressionList, ChalkTokenComma,
        ChalkNodeAssignmentExpression, 0,

    0
};

ULONG ChalkGrammarUnaryExpression[] = {
    ChalkNodePostfixExpression, 0,
    ChalkTokenIncrement, ChalkNodeUnaryExpression, 0,
    ChalkTokenDecrement, ChalkNodeUnaryExpression, 0,
    ChalkNodeUnaryOperator, ChalkNodeUnaryExpression, 0,
    0
};

ULONG ChalkGrammarUnaryOperator[] = {
    ChalkTokenPlus, 0,
    ChalkTokenMinus, 0,
    ChalkTokenBitNot, 0,
    ChalkTokenLogicalNot, 0,
    0
};

ULONG ChalkGrammarMultiplicativeExpression[] = {
    ChalkNodeUnaryExpression, 0,
    ChalkNodeMultiplicativeExpression, ChalkTokenAsterisk,
        ChalkNodeUnaryExpression, 0,

    ChalkNodeMultiplicativeExpression, ChalkTokenDivide,
        ChalkNodeUnaryExpression, 0,

    ChalkNodeMultiplicativeExpression, ChalkTokenModulo,
        ChalkNodeUnaryExpression, 0,

    0,
};

ULONG ChalkGrammarAdditiveExpression[] = {
    ChalkNodeMultiplicativeExpression, 0,
    ChalkNodeAdditiveExpression, ChalkTokenPlus,
        ChalkNodeMultiplicativeExpression, 0,

    ChalkNodeAdditiveExpression, ChalkTokenMinus,
        ChalkNodeMultiplicativeExpression, 0,

    0
};

ULONG ChalkGrammarShiftExpression[] = {
    ChalkNodeAdditiveExpression, 0,
    ChalkNodeShiftExpression, ChalkTokenLeftShift,
        ChalkNodeAdditiveExpression, 0,

    ChalkNodeShiftExpression, ChalkTokenRightShift,
        ChalkNodeAdditiveExpression, 0,

    0
};

ULONG ChalkGrammarRelationalExpression[] = {
    ChalkNodeShiftExpression, 0,
    ChalkNodeRelationalExpression, ChalkTokenLessThan,
        ChalkNodeShiftExpression, 0,

    ChalkNodeRelationalExpression, ChalkTokenGreaterThan,
        ChalkNodeShiftExpression, 0,

    ChalkNodeRelationalExpression, ChalkTokenLessOrEqual,
        ChalkNodeShiftExpression, 0,

    ChalkNodeRelationalExpression, ChalkTokenGreaterOrEqual,
        ChalkNodeShiftExpression, 0,

    0
};

ULONG ChalkGrammarEqualityExpression[] = {
    ChalkNodeRelationalExpression, 0,
    ChalkNodeEqualityExpression, ChalkTokenIsEqual,
        ChalkNodeRelationalExpression, 0,

    ChalkNodeEqualityExpression, ChalkTokenIsNotEqual,
        ChalkNodeRelationalExpression, 0,

    0
};

ULONG ChalkGrammarAndExpression[] = {
    ChalkNodeEqualityExpression, 0,
    ChalkNodeAndExpression, ChalkTokenBitAnd, ChalkNodeEqualityExpression, 0,
    0
};

ULONG ChalkGrammarExclusiveOrExpression[] = {
    ChalkNodeAndExpression, 0,
    ChalkNodeExclusiveOrExpression, ChalkTokenXor, ChalkNodeAndExpression, 0,
    0
};

ULONG ChalkGrammarInclusiveOrExpression[] = {
    ChalkNodeExclusiveOrExpression, 0,
    ChalkNodeInclusiveOrExpression, ChalkTokenBitOr,
        ChalkNodeExclusiveOrExpression, 0,

    0
};

ULONG ChalkGrammarLogicalAndExpression[] = {
    ChalkNodeInclusiveOrExpression, 0,
    ChalkNodeLogicalAndExpression, ChalkTokenLogicalAnd,
        ChalkNodeExclusiveOrExpression, 0,

    0
};

ULONG ChalkGrammarLogicalOrExpression[] = {
    ChalkNodeLogicalAndExpression, 0,
    ChalkNodeLogicalOrExpression, ChalkTokenLogicalOr,
        ChalkNodeLogicalAndExpression, 0,

    0
};

ULONG ChalkGrammarConditionalExpression[] = {
    ChalkNodeLogicalOrExpression, ChalkTokenQuestion, ChalkNodeExpression,
        ChalkTokenColon, ChalkNodeConditionalExpression, 0,

    ChalkNodeLogicalOrExpression, 0,
    0
};

ULONG ChalkGrammarAssignmentExpression[] = {
    ChalkNodeUnaryExpression, ChalkNodeAssignmentOperator,
        ChalkNodeAssignmentExpression, 0,

    ChalkNodeConditionalExpression, 0,
    0
};

ULONG ChalkGrammarAssignmentOperator[] = {
    ChalkTokenAssign, 0,
    ChalkTokenMultiplyAssign, 0,
    ChalkTokenDivideAssign, 0,
    ChalkTokenModuloAssign, 0,
    ChalkTokenAddAssign, 0,
    ChalkTokenSubtractAssign, 0,
    ChalkTokenLeftAssign, 0,
    ChalkTokenRightAssign, 0,
    ChalkTokenAndAssign, 0,
    ChalkTokenXorAssign, 0,
    ChalkTokenOrAssign, 0,
    ChalkTokenNullAssign, 0,
    0
};

ULONG ChalkGrammarExpression[] = {
    ChalkNodeAssignmentExpression, 0,
    ChalkNodeExpression, ChalkTokenComma, ChalkNodeAssignmentExpression, 0,
    0
};

ULONG ChalkGrammarStatement[] = {
    ChalkNodeExpressionStatement, 0,
    ChalkNodeSelectionStatement, 0,
    ChalkNodeIterationStatement, 0,
    ChalkNodeJumpStatement, 0,
    0
};

ULONG ChalkGrammarCompoundStatement[] = {
    ChalkTokenOpenBrace, ChalkTokenCloseBrace, 0,
    ChalkTokenOpenBrace, ChalkNodeStatementList, ChalkTokenCloseBrace, 0,
    0
};

ULONG ChalkGrammarStatementList[] = {
    ChalkNodeStatement, 0,
    ChalkNodeStatementList, ChalkNodeStatement, 0,
    0
};

ULONG ChalkGrammarExpressionStatement[] = {
    ChalkTokenSemicolon, 0,
    ChalkNodeExpression, ChalkTokenSemicolon, 0,
    0
};

ULONG ChalkGrammarSelectionStatement[] = {
    ChalkTokenIf, ChalkTokenOpenParentheses, ChalkNodeExpression,
        ChalkTokenCloseParentheses, ChalkNodeCompoundStatement, ChalkTokenElse,
        ChalkNodeSelectionStatement, 0,

    ChalkTokenIf, ChalkTokenOpenParentheses, ChalkNodeExpression,
        ChalkTokenCloseParentheses, ChalkNodeCompoundStatement, ChalkTokenElse,
        ChalkNodeCompoundStatement, 0,

    ChalkTokenIf, ChalkTokenOpenParentheses, ChalkNodeExpression,
        ChalkTokenCloseParentheses, ChalkNodeCompoundStatement, 0,

    0
};

ULONG ChalkGrammarIterationStatement[] = {
    ChalkTokenWhile, ChalkTokenOpenParentheses, ChalkNodeExpression,
        ChalkTokenCloseParentheses, ChalkNodeCompoundStatement, 0,

    ChalkTokenDo, ChalkNodeCompoundStatement, ChalkTokenWhile,
        ChalkTokenOpenParentheses, ChalkNodeExpression,
        ChalkTokenCloseParentheses, ChalkTokenSemicolon, 0,

    ChalkTokenFor, ChalkTokenOpenParentheses, ChalkTokenIdentifier,
        ChalkTokenIn, ChalkNodeExpression, ChalkTokenCloseParentheses,
        ChalkNodeCompoundStatement, 0,

    ChalkTokenFor, ChalkTokenOpenParentheses, ChalkNodeExpressionStatement,
        ChalkNodeExpressionStatement, ChalkTokenCloseParentheses,
        ChalkNodeCompoundStatement, 0,

    ChalkTokenFor, ChalkTokenOpenParentheses, ChalkNodeExpressionStatement,
        ChalkNodeExpressionStatement, ChalkNodeExpression,
        ChalkTokenCloseParentheses, ChalkNodeCompoundStatement, 0,

    0
};

ULONG ChalkGrammarJumpStatement[] = {
    ChalkTokenBreak, ChalkTokenSemicolon, 0,
    ChalkTokenContinue, ChalkTokenSemicolon, 0,
    ChalkTokenReturn, ChalkTokenSemicolon, 0,
    ChalkTokenReturn, ChalkNodeExpression, ChalkTokenSemicolon, 0,
    0
};

ULONG ChalkGrammarTranslationUnit[] = {
    ChalkNodeExternalDeclaration, 0,
    ChalkNodeTranslationUnit, ChalkNodeExternalDeclaration, 0,
    0
};

ULONG ChalkGrammarExternalDeclaration[] = {
    ChalkNodeFunctionDefinition, 0,
    ChalkNodeStatement, 0,
    0
};

ULONG ChalkGrammarIdentifierList[] = {
    ChalkTokenIdentifier, 0,
    ChalkNodeIdentifierList, ChalkTokenComma, ChalkTokenIdentifier, 0,
    0
};

ULONG ChalkGrammarFunctionDefinition[] = {
    ChalkTokenFunction, ChalkTokenIdentifier, ChalkTokenOpenParentheses,
        ChalkTokenCloseParentheses, ChalkNodeCompoundStatement, 0,

    ChalkTokenFunction, ChalkTokenIdentifier, ChalkTokenOpenParentheses,
        ChalkNodeIdentifierList, ChalkTokenCloseParentheses,
        ChalkNodeCompoundStatement, 0,

    0
};

PARSER_GRAMMAR_ELEMENT ChalkGrammar[] = {
    {"ListElementList", 0, ChalkGrammarListElementList},
    {"List", 0, ChalkGrammarList},
    {"DictElement", 0, ChalkGrammarDictElement},
    {"DictElementList", 0, ChalkGrammarDictElementList},
    {"Dict", 0, ChalkGrammarDict},
    {"PrimaryExpression", 0, ChalkGrammarPrimaryExpression},
    {"PostfixExpression",
     YY_GRAMMAR_COLLAPSE_ONE | YY_GRAMMAR_NEST_LEFT_RECURSION,
     ChalkGrammarPostfixExpression},

    {"ArgumentExpressionList", 0, ChalkGrammarArgumentExpressionList},
    {"UnaryExpression", YY_GRAMMAR_COLLAPSE_ONE, ChalkGrammarUnaryExpression},
    {"UnaryOperator", 0, ChalkGrammarUnaryOperator},
    {"MultiplicativeExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarMultiplicativeExpression},

    {"AdditiveExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarAdditiveExpression},

    {"ShiftExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarShiftExpression},

    {"RelationalExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarRelationalExpression},

    {"EqualityExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarEqualityExpression},

    {"AndExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarAndExpression},

    {"ExclusiveOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarExclusiveOrExpression},

    {"InclusiveOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarInclusiveOrExpression},

    {"LogicalAndExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarLogicalAndExpression},

    {"LogicalOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarLogicalOrExpression},

    {"ConditionalExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarConditionalExpression},

    {"AssignmentExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarAssignmentExpression},

    {"AssignmentOperator", 0, ChalkGrammarAssignmentOperator},
    {"Expression", 0, ChalkGrammarExpression},
    {"Statement", YY_GRAMMAR_COLLAPSE_ONE, ChalkGrammarStatement},
    {"CompoundStatement", 0, ChalkGrammarCompoundStatement},
    {"StatementList", 0, ChalkGrammarStatementList},
    {"ExpressionStatement", 0, ChalkGrammarExpressionStatement},
    {"SelectionStatement", 0, ChalkGrammarSelectionStatement},
    {"IterationStatement", 0, ChalkGrammarIterationStatement},
    {"JumpStatement", 0, ChalkGrammarJumpStatement},
    {"TranslationUnit", 0, ChalkGrammarTranslationUnit},
    {"ExternalDeclaration",
     YY_GRAMMAR_COLLAPSE_ONE,
     ChalkGrammarExternalDeclaration},

    {"IdentifierList", 0, ChalkGrammarIdentifierList},
    {"FunctionDefinition", 0, ChalkGrammarFunctionDefinition},
    {NULL, 0, NULL},
};

PARSER ChalkParser;

//
// ------------------------------------------------------------------ Functions
//

INT
ChalkParseScript (
    PCHALK_INTERPRETER Interpreter,
    PCHALK_SCRIPT Script,
    PVOID *TranslationUnit
    )

/*++

Routine Description:

    This routine lexes and parses the given script data.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Script - Supplies a pointer to the script to parse.

    TranslationUnit - Supplies a pointer where the translation unit will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONG Column;
    KSTATUS KStatus;
    LEXER Lexer;
    ULONG Line;
    PPARSER Parser;
    INT Status;

    memset(&Lexer, 0, sizeof(Lexer));
    Lexer.Input = Script->Data;
    Lexer.InputSize = Script->Size;
    Lexer.Expressions = ChalkLexerExpressions;
    Lexer.IgnoreExpressions = ChalkLexerIgnoreExpressions;
    Lexer.ExpressionNames = ChalkLexerTokenNames;
    Lexer.TokenBase = CHALK_TOKEN_BASE;
    YyLexInitialize(&Lexer);
    Parser = Script->Parser;
    Parser->Context = &Lexer;
    Parser->Lexer = &Lexer;
    YyParserInitialize(Parser);
    KStatus = YyParse(Parser, (PPARSER_NODE *)TranslationUnit);
    Parser->Context = NULL;
    Parser->Lexer = NULL;
    if (!KSUCCESS(KStatus)) {
        Column = 0;
        Line = 0;
        if (Parser->NextToken != NULL) {
            Column = Parser->NextToken->Column;
            Line = Parser->NextToken->Line;
        }

        fprintf(stderr,
                "Parsing script %s failed at line %d:%d: %d\n",
                Script->Path,
                Line,
                Column,
                KStatus);

        Status = EILSEQ;
        goto ParseScriptEnd;
    }

    Status = 0;

ParseScriptEnd:
    return Status;
}

PSTR
ChalkGetNodeGrammarName (
    PCHALK_NODE Node
    )

/*++

Routine Description:

    This routine returns the grammatical element name for the given node.

Arguments:

    Node - Supplies a pointer to the node.

Return Value:

    Returns a pointer to a constant string of the name of the grammar element
    represented by this execution node.

--*/

{

    PPARSER_NODE ParseNode;

    ParseNode = Node->ParseNode;
    return ChalkGrammar[ParseNode->GrammarElement - ChalkNodeBegin].Name;
}

KSTATUS
ChalkLexGetToken (
    PVOID Context,
    PLEXER_TOKEN Token
    )

/*++

Routine Description:

    This routine gets the next token for the parser.

Arguments:

    Context - Supplies a context pointer initialized in the parser.

    Token - Supplies a pointer where the next token will be filled out and
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_END_OF_FILE if the file end was reached.

    STATUS_MALFORMED_DATA_STREAM if the given input matched no rule in the
    lexer and the lexer was not configured to ignore such things.

--*/

{

    PLEXER Lexer;
    KSTATUS Status;

    Lexer = Context;
    while (TRUE) {
        Status = YyLexGetToken(Lexer, Token);
        if (!KSUCCESS(Status)) {
            break;
        }

        if ((Token->Value == ChalkTokenMultilineComment) ||
            (Token->Value == ChalkTokenComment)) {

            continue;
        }

        break;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

