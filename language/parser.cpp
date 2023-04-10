
// These is the global variables of the program.
// I put them in a struct like this as an experiment to see how I liked it vs the g_ prefix.
// I don't know why it's called chai. Probably because I had some chai tea that day.
struct Chai {
    bool console_can_output_colors;
    bool error_was_reported;
    
    Str fully_pathed_output_file_name;
    Str path_to_input_file;
    
    Arena ast_arena;
    
    Arena string_arena;
    
    s32 variable_count;
    Variable variables[8*1024]; // @Incomplete: Use Array<Variable> or hash table
    
    Data_Type string_count_type;
    Data_Type string_data_type;
    Data_Type array_count_type;
    Ast_Proc_Defn *current_proc;
    Ast *current_loop;
};
global Chai chai;

#include "lexer.cpp"

#define RetIfError(...) if (chai.error_was_reported) { return __VA_ARGS__; }

internal inline char *
alloc_string(umm size) {
    return (char *)arena_alloc(&chai.string_arena, size, 1);
}

internal void
report_error(Ast_Expr *expr, char *format, ...) {
    va_list args; va_start(args, format);
    __report_base_proc(REPORT_ERROR, expr->loc, format, args);
    va_end(args);
}

internal void
report_info(Ast_Expr *expr, char *format, ...) {
    va_list args; va_start(args, format);
    __report_base_proc(REPORT_INFO, expr->loc, format, args);
    va_end(args);
}

internal void
report_error(Ast_Parameter_Node *param, char *format, ...) {
    va_list args; va_start(args, format);
    __report_base_proc(REPORT_ERROR, param->decl.loc, format, args);
    va_end(args);
}

internal void
report_info(Ast_Parameter_Node *param, char *format, ...) {
    va_list args; va_start(args, format);
    __report_base_proc(REPORT_INFO, param->decl.loc, format, args);
    va_end(args);
}





global const s32 NO_PRECEDENCE = 0;

struct Binary_Info {
    s32 precedence;
    Binary_Tag tag;
};

internal Ast *parse_statement(Lexer *lexer);
internal Ast_Unary *parse_unary(Lexer *lexer, s32 precedence, Unary_Tag tag);
internal Ast_Subexpr *parse_binary(Lexer *lexer, s32 precedence, Ast_Subexpr *left, Binary_Info binary_info);
internal Ast_Expr *parse_expr(Lexer *lexer);
internal void parse_type_expr(Lexer *lexer, Ast_Type_Expr *type_expr);

internal inline Unary_Tag
get_unary_tag(s32 token_type) {
    using enum Unary_Tag;
    switch (token_type) {
        case '*': return POINTER_TO;
        case '-': return MINUS;
        case '!': return LOGICAL_NOT;
        case '~': return BITWISE_NOT;
        case TOKEN_LEFT_SHIFT_OR_DEREFERENCE: return DEREFERENCE;
        case TOKEN_KEYWORD_CAST: return CAST;
    }
    return UNKNOWN;
}

internal Binary_Info
get_binary_info(s32 token_type) {
    Binary_Info result = {};
    using enum Binary_Tag;
    switch (token_type) {
        Case TOKEN_LOGICAL_OR:    { result.precedence = 2; result.tag = LOGICAL_OR;  }
        Case TOKEN_LOGICAL_AND:   { result.precedence = 3; result.tag = LOGICAL_AND; }
        
        Case TOKEN_IS_EQUAL:      { result.precedence = 4; result.tag = IS_EQUAL;    }
        Case TOKEN_NOT_EQUAL:     { result.precedence = 4; result.tag = NOT_EQUAL;   }
        
        Case '>':                 { result.precedence = 5; result.tag = GREATER_THAN;          }
        Case '<':                 { result.precedence = 5; result.tag = LESS_THAN;             }
        Case TOKEN_GREATER_EQUAL: { result.precedence = 5; result.tag = GREATER_THAN_OR_EQUAL; }
        Case TOKEN_LESS_EQUAL:    { result.precedence = 5; result.tag = LESS_THAN_OR_EQUAL;    }
        
        Case '|':                 { result.precedence = 6; result.tag = BITWISE_OR;  }
        Case '^':                 { result.precedence = 7; result.tag = BITWISE_XOR; }
        Case '&':                 { result.precedence = 8; result.tag = BITWISE_AND; }
        
        Case TOKEN_LEFT_SHIFT_OR_DEREFERENCE: { result.precedence = 9; result.tag = LEFT_SHIFT;  }
        Case TOKEN_RIGHT_SHIFT:               { result.precedence = 9; result.tag = RIGHT_SHIFT; }
        
        Case '+': { result.precedence = 10; result.tag = ADD; }
        Case '-': { result.precedence = 10; result.tag = SUB; }
        
        Case '*': { result.precedence = 11; result.tag = MUL; }
        Case '/': { result.precedence = 11; result.tag = DIV; }
        Case '%': { result.precedence = 11; result.tag = MOD; }
        
        // precedence 12 is for unary operators
        
        Case '[': { result.precedence = 13; result.tag = ARRAY_ACCESS; }
        Case '.': { result.precedence = 13; result.tag = DOT;          }
    }
    return result;
}

internal Str
to_string(Binary_Tag type) {
    using enum Binary_Tag;
    switch (type) {
        case LOGICAL_AND:  return sl("&&");
        case LOGICAL_OR:   return sl("||");
        case GREATER_THAN: return sl(">");
        case LESS_THAN:    return sl("<");
        case GREATER_THAN_OR_EQUAL: return sl(">=");
        case LESS_THAN_OR_EQUAL:    return sl("<=");
        case IS_EQUAL:    return sl("==");
        case NOT_EQUAL:   return sl("!=");
        case BITWISE_AND: return sl("&");
        case BITWISE_XOR: return sl("^");
        case BITWISE_OR:  return sl("|");
        case ADD: return sl("+");
        case SUB: return sl("-");
        case MUL: return sl("*");
        case DIV: return sl("/");
        case MOD: return sl("%");
        case LEFT_SHIFT:  return sl("<<");
        case RIGHT_SHIFT: return sl(">>");
        case ARRAY_ACCESS: return sl("[@Temporary]");
        case DOT: return sl(".");
        InvalidDefaultCase("Unhandled binary type");
    }
    return sl("BINARY_UNKNOWN");;
}

internal Ast_Subexpr *
parse_subexpr(Lexer *lexer, s32 precedence) {
    auto token = peek_next_token(lexer);
    Ast_Subexpr *subexpr = null;
    
    switch (token.tag) {
        
        Case '(': {
            eat_token(lexer);
            auto open_paren = token;
            subexpr = parse_subexpr(lexer, 0);
            RetIfError(null);
            token = get_token(lexer);
            if (token.tag != ')') {
                report_error(token, "Expected a ')' to surround an expression, but found this:");
                report_info(open_paren, "... here's the matching '('.");
                return null;
            }
        }
        
        Case TOKEN_KEYWORD_NULL: {
            eat_token(lexer);
            auto literal  = NewAst(Ast_Literal);
            literal->loc  = token.loc;
            literal->tag  = Ast_Literal::NULL;
            subexpr = literal;
        }
        Case TOKEN_KEYWORD_TRUE: {
            eat_token(lexer);
            auto literal  = NewAst(Ast_Literal);
            literal->loc  = token.loc;
            literal->tag  = Ast_Literal::TRUE;
            subexpr = literal;
        }
        Case TOKEN_KEYWORD_FALSE: {
            eat_token(lexer);
            auto literal  = NewAst(Ast_Literal);
            literal->loc  = token.loc;
            literal->tag  = Ast_Literal::FALSE;
            subexpr = literal;
        }
        Case TOKEN_NUMBER: {
            eat_token(lexer);
            auto literal  = NewAst(Ast_Literal);
            literal->loc  = token.loc;
            literal->text = token.text;
            literal->tag  = Ast_Literal::NUMBER;
            subexpr = literal;
        }
        Case TOKEN_DIRECTIVE_CHAR: {
            eat_token(lexer);
            token = get_token(lexer);
            if (token.tag != TOKEN_STRING) {
                report_error(token, "Expected a string literal after #char directive, but found this.");
                return null;
            }
            auto literal  = NewAst(Ast_Literal);
            literal->loc  = token.loc;
            literal->text = token.text;
            literal->tag  = Ast_Literal::NUMBER;
            literal->is_char = true;
            subexpr = literal;
        }
        Case TOKEN_STRING: {
            eat_token(lexer);
            auto literal  = NewAst(Ast_Literal);
            literal->loc  = token.loc;
            literal->text = token.text;
            literal->tag  = Ast_Literal::STRING;
            subexpr = literal;
        }
        
        Case TOKEN_IDENT: {
            auto ident_token = token;
            eat_token(lexer);
            
            token = peek_next_token(lexer);
            if (token.tag != '(') {
                auto ident  = NewAst(Ast_Ident);
                ident->name = ident_token.text;
                ident->loc  = ident_token.loc;
                subexpr = ident;
                break;
            }
            eat_token(lexer);
            
            // Procedure call
            
            auto call = NewAst(Ast_Call);
            call->loc = ident_token.loc;
            call->name = ident_token.text;
            
            token = peek_next_token(lexer);
            if (token.tag == ')') {
                eat_token(lexer);
            } else {
                auto expr = parse_subexpr(lexer, 0);
                RetIfError(null);
                assert(expr);
                
                call->first_arg = NewAst(Ast_Argument_Node);
                call->first_arg->expr = expr;
                auto arg = call->first_arg;
                s32 arg_count = 1;
                
                while (true) {
                    token = peek_next_token(lexer);
                    if (token.tag != ',') break;
                    
                    eat_token(lexer);
                    expr = parse_subexpr(lexer, 0);
                    RetIfError(null);
                    if (!expr) {
                        report_error(token, "Empty expression after ',' in procedure call.");
                        return null;
                    }
                    arg->next = NewAst(Ast_Argument_Node);
                    arg->next->expr = expr;
                    arg = arg->next;
                }
                token = get_token(lexer);
                if (token.tag != ')') {
                    report_error(token, "Expected ')' after procedure call arguments, but found this:");
                    return null;
                }
                
            }
            
            subexpr = call;
        }
        
        Case TOKEN_KEYWORD_SIZEOF: {
            auto sizeof_token = token;
            eat_token(lexer);
            token = get_token(lexer);
            
            if (token.tag != '(') {
                report_error(token, "Expected '(' after 'sizeof' keyword, but found this:");
            }
            
            auto size_of = NewAst(Ast_Sizeof);
            
            parse_type_expr(lexer, &size_of->type_expr);
            RetIfError(null);
            
            token = get_token(lexer);
            if (token.tag != ')') {
                report_error(token, "Expected ')' after sizeof expression, but found this:");
            }
            
            size_of->loc = sizeof_token.loc;
            size_of->loc.l1 = token.loc.l1;
            size_of->loc.c1 = token.loc.c1;
            
            subexpr = size_of;
        }
        
        Default: {
            auto unary_tag = get_unary_tag(token.tag);
            if (unary_tag != Unary_Tag::UNKNOWN) {
                auto unary = parse_unary(lexer, precedence, unary_tag);
                RetIfError(null);
                // @Copypaste: :AssignVsEqualCheck :ParseBinaryAtEndOfStuff
                token = peek_next_token(lexer);
                
                subexpr = unary;
            } else {
                report_error(token, "Expected an expression, but found this:");
                return null;
            }
        }
    } // switch (token.tag)
    assert(subexpr);
    token = peek_next_token(lexer);
    auto binary_info = get_binary_info(token.tag);
    if (binary_info.tag != Binary_Tag::UNKNOWN) {
        return parse_binary(lexer, precedence, subexpr, binary_info);
    }
    return subexpr;
}

internal void
parse_decl(Lexer *lexer, Ast_Decl *decl, Token ident_token) {
    decl->loc = ident_token.loc;
    decl->ident.name = ident_token.text;
    decl->ident.loc  = ident_token.loc;
    
#if 0
    // :MultiReturnRefactor
    if (multi) {
        decl->ident.next = NewAst(Ast_Ident_Node);
        auto node = decl->ident.next;
        auto token = get_token(lexer);
        while (true) {
            if (token.tag != TOKEN_IDENT) {
                report_error(token, "Expected an identifier in a comma-sparated list, but found this.");
                return;
            }
            node->name = token.text;
            node->loc  = token.loc;
            token = peek_next_token(lexer);
            if (token.tag != ',') break;
            eat_token(lexer);
            node->next = NewAst(Ast_Ident_Node);
            node = node->next;
            token = get_token(lexer);
        }
        if (token.tag != ':') {
            report_error(token, "Expected a ':' after a comma-separated list for a declaration.");
            return;
        }
        eat_token(lexer);
    }
#endif
    
    auto token = peek_next_token(lexer);
    if (!(token.tag == '=' || token.tag == ':')) {
        decl->_type_expr = NewAst(Ast_Type_Expr);
        parse_type_expr(lexer, decl->_type_expr);
        RetIfError();
        token = peek_next_token(lexer);
    }
    bool type_is_inferred = !decl->_type_expr;
#if 0
    if (multi) {
        if (!type_is_inferred || token.tag != '=') {
            report_error(token, "A comma-sparated declaration use ':='");
            return;
        }
    }
#endif
    
    if (token.tag == '=' || token.tag == ':') {
        bool is_constant = token.tag == ':';
        if (is_constant) {
            decl->ident.flags |= Ast_Ident::CONSTANT;
        }
        
        auto equal_or_colon = token;
        eat_token(lexer);
        
        token = peek_next_token(lexer);
        if (token.tag == TOKEN_TRIPLE_MINUS) {
            decl->loc.l1 = token.loc.l1;
            decl->loc.c1 = token.loc.c1;
            eat_token(lexer);
            decl->initialization = NewAst(Ast_Explicitly_Uninitalize);
        } else {
            decl->initialization = parse_expr(lexer);
            RetIfError();
            if (!decl->initialization) {
                report_error(equal_or_colon, "Missing the initalization expression for a declaration.");
                return;
            }
            decl->loc.l1 = decl->initialization->loc.l1;
            decl->loc.c1 = decl->initialization->loc.c1;
            
            if (type_is_inferred) {
#if 0
                if (multi) {
                    decl->preceding_whitespace_and_comments = ident_token.preceding_whitespace_and_comments;
                    return;
                } else
#endif
                if (decl->initialization->ast_tag == AST_TYPE_EXPR) {
                    token = peek_next_token(lexer);
                    if (token.tag == TOKEN_DIRECTIVE_C_DECL) {
                        eat_token(lexer);
                        decl->ident.flags |= Ast_Ident::IS_C_DECL;
                    }
                }
            } else {
                assert(decl->initialization->ast_tag != AST_TYPE_EXPR); // @Incomplete
            }
        }
    } else {
        decl->loc.l1 = decl->_type_expr->loc.l1;
        decl->loc.c1 = decl->_type_expr->loc.c1;
    }
    decl->preceding_whitespace_and_comments = ident_token.preceding_whitespace_and_comments;
}



internal Ast_Subexpr *
parse_binary(Lexer *lexer, s32 precedence, Ast_Subexpr *left, Binary_Info binary_info) {
    using enum Binary_Tag;
    assert(binary_info.tag != UNKNOWN);
    
    auto binary_token = peek_next_token(lexer);
    
    while (true) {
        if (!(binary_info.precedence > precedence)) break; // >  for left-associativity
        if (binary_info.tag == IS_EQUAL) {
            auto next_token = peek_token(lexer, 1);
            if (next_token.tag == '{') break;
        }
        eat_token(lexer);
        
        if (binary_info.tag == DOT) {
            auto ident_token = get_token(lexer);
            
            if (ident_token.tag != TOKEN_IDENT) {
                report_error(ident_token, "Right side of '.' isn't an identifer.");
                return null;
            }
            
            Ast_Ident *ident = null;
            ident = NewAst(Ast_Ident);
            ident->loc = ident_token.loc;
            ident->name = ident_token.text;
            //ident->variable_index should be default
            
            auto binary = NewAst(Ast_Binary);
            binary->tag    = DOT;
            binary->left   = left;
            binary->right  = ident;
            binary->loc    = left->loc;
            binary->loc.l1 = ident_token.loc.l1;
            binary->loc.c1 = ident_token.loc.c1;
            binary_token = peek_next_token(lexer);
            
            left = binary;
        } else {
            auto binary = NewAst(Ast_Binary);
            binary->tag = binary_info.tag;
            binary->left = left;
            binary->loc = left->loc;
            if (binary_info.tag == ARRAY_ACCESS) {
                binary->right = parse_subexpr(lexer, NO_PRECEDENCE);
                RetIfError(null);
                auto token = get_token(lexer);
                if (token.tag != ']') {
                    report_error(token, "Expected ']' after array access expression, but found this:");
                    return null;
                }
                binary->loc.l1 = token.loc.l1;
                binary->loc.l1 = token.loc.c1;
            } else {
                binary->right = parse_subexpr(lexer, binary_info.precedence);
                RetIfError(null);
                binary->loc.l1 = binary->right->loc.l1;
                binary->loc.c1 = binary->right->loc.c1;
            }
            
            binary_token = peek_next_token(lexer);
            left = binary;
        }
        
        binary_info = get_binary_info(binary_token.tag);
        if (binary_info.tag == UNKNOWN) break;
    }
    return left;
}

/////// this has A LOT of overlap cause i'm not really sure how i'll be structuring this
internal void
parse_type_expr(Lexer *lexer, Ast_Type_Expr *type_expr) {
    
    Data_Type dummy_type = {};
    Data_Type *type = &dummy_type;
    
    Data_Type_Tag parent_tag = Data_Type_Tag::UNKNOWN;
    
    auto token = get_token(lexer);
    Code_Location loc = token.loc;
    
    while (true) {
        if (token.tag == '[') {
            type->tag = Data_Type_Tag::STATIC_ARRAY;
            type->array.size_expr = parse_subexpr(lexer, NO_PRECEDENCE);
            RetIfError();
            token = get_token(lexer);
            if (token.tag != ']') {
                report_error(token, "Expected a ']' after an array size expression, but found this:");
                return;
            }
            type->array.of = AllocDataTypeUninitialized();
            type = type->array.of;
            *type = {};
            parent_tag = Data_Type_Tag::STATIC_ARRAY;
        } else if (token.tag == '*') {
            type->tag = Data_Type_Tag::POINTER;
            type->pointer.to = AllocDataTypeUninitialized();
            type = type->pointer.to;
            *type = {};
            parent_tag = Data_Type_Tag::POINTER;
        } else {
            break;
        }
        token = get_token(lexer);
    }
    
    if (token.tag >= TOKEN_KEYWORD_FIRST_TYPE && token.tag <= TOKEN_KEYWORD_LAST_TYPE) {
        //type_keyword_to_data_type(token.tag, type);
        type->number.is_solid = true;
        assert(token.tag >= TOKEN_KEYWORD_FIRST_TYPE && token.tag <= TOKEN_KEYWORD_LAST_TYPE);
        switch (token.tag) {
            Case   TOKEN_KEYWORD_INT:
            OrCase TOKEN_KEYWORD_S64:  { type->tag = Data_Type_Tag::INT;   type->number.size = 8; type->number.is_signed = true; }
            Case   TOKEN_KEYWORD_S32:  { type->tag = Data_Type_Tag::INT;   type->number.size = 4; type->number.is_signed = true; }
            Case   TOKEN_KEYWORD_S16:  { type->tag = Data_Type_Tag::INT;   type->number.size = 2; type->number.is_signed = true; }
            Case   TOKEN_KEYWORD_S8:   { type->tag = Data_Type_Tag::INT;   type->number.size = 1; type->number.is_signed = true; }
            
            Case TOKEN_KEYWORD_U64:    { type->tag = Data_Type_Tag::INT;   type->number.size = 8; }
            Case TOKEN_KEYWORD_U32:    { type->tag = Data_Type_Tag::INT;   type->number.size = 4; }
            Case TOKEN_KEYWORD_U16:    { type->tag = Data_Type_Tag::INT;   type->number.size = 2; }
            Case TOKEN_KEYWORD_U8:     { type->tag = Data_Type_Tag::INT;   type->number.size = 1; }
            
            Case TOKEN_KEYWORD_F64:    { type->tag = Data_Type_Tag::FLOAT; type->number.size = 8; }
            Case TOKEN_KEYWORD_F32:    { type->tag = Data_Type_Tag::FLOAT; type->number.size = 4; }
            
            Case TOKEN_KEYWORD_BOOL:   { type->tag = Data_Type_Tag::BOOL;   }
            Case TOKEN_KEYWORD_STRING: { type->tag = Data_Type_Tag::STRING; }
            Case TOKEN_KEYWORD_VOID:   { type->tag = Data_Type_Tag::VOID;   }
            Default: {
                report_error(token, "Internal compiler error: Unhandled keyword type in declaration (token.tag = %d).", type);
            }
        }
        loc.l1 = token.loc.l1;
        loc.c1 = token.loc.c1;
    } else if (token.tag == TOKEN_IDENT) {
        auto ident = NewAst(Ast_Ident);
        ident->name = token.text;
        ident->loc  = token.loc;
        type->tag = Data_Type_Tag::UNKNOWN;
        type->user_type_ident = ident;
        
        loc.l1 = token.loc.l1;
        loc.c1 = token.loc.c1;
    } else if (token.tag == TOKEN_KEYWORD_STRUCT || token.tag == TOKEN_KEYWORD_UNION || token.tag == TOKEN_KEYWORD_ENUM) {
        loc.l1 = token.loc.l1;
        loc.c1 = token.loc.c1;
        
        auto struct_text = sl("struct");
        auto struct_union_enum = Struct_Kind::STRUCT;
        if (token.tag == TOKEN_KEYWORD_UNION) {
            struct_union_enum = Struct_Kind::UNION;
            struct_text = sl("union");
        } else if (token.tag == TOKEN_KEYWORD_ENUM) {
            struct_union_enum = Struct_Kind::ENUM;
            struct_text = sl("enum");
            report_error(token, "enum isn't implemented yet.");
            return;
        } else assert(token.tag == TOKEN_KEYWORD_STRUCT);
        
        token = get_token(lexer);
        if (token.tag != '{') {
            report_error(token, "Expected '{' after '%S', but found this:", struct_text);
            return;
        }
        // @Copypaste: From parse_statement and parse_expr TOKEN_IDENT
        token = peek_next_token(lexer);
        if (token.tag == '}') {
            report_error(token, "Empty %S.", struct_text);
            return;
        }
        
        type->tag = Data_Type_Tag::STRUCT;
        type->structure.struct_union_enum = struct_union_enum;
        type->structure.first_member = NewAst(Ast_Struct_Member_Node);
        
        {
            auto _member = type->structure.first_member;
            Ast_Struct_Member_Node new_member; // :GetsDefaultValues
            while (true) {
                if (token.tag == TOKEN_KEYWORD_USING) {
                    new_member.decl.using_kind = Ast_Decl::NORMAL_USING;
                    eat_token(lexer);
                    token = peek_next_token(lexer);
                    if (token.tag != TOKEN_IDENT) {
                        report_error(token, "Expected an identifer after 'using', but found this.");
                        return;
                    }
                }
                
                if (token.tag == TOKEN_IDENT) {
                    eat_token(lexer);
                    
                    auto ident_token = token;
                    token = get_token(lexer);
                    if (token.tag != ':') {
                        report_error(token, "Expected ':' after an identifier, but found this.");
                        return;
                    }
                    
                    parse_decl(lexer, &new_member.decl, ident_token);
                    RetIfError();
                    
                    if (new_member.decl.initialization) {
                        if (new_member.decl.initialization->ast_tag == AST_EXPLICITLY_UNINITALIZE) {
                            report_error(new_member.decl.initialization, "You can't explicitly uninitialize a %S member.", struct_text);
                        } else {
                            report_error(new_member.decl.initialization, "Default %S member values aren't implemented yet.", struct_text);
                        }
                        return;
                    }
                    
                    token = peek_next_token(lexer);
                    while (token.tag == ';') {
                        eat_token(lexer);
                        token = peek_next_token(lexer);
                    }
                } else {
                    assert(new_member.decl.using_kind == Ast_Decl::NO_USING);
                    if (token.tag == TOKEN_KEYWORD_STRUCT) {
                        new_member.decl.using_kind = Ast_Decl::ANON_USING;
                    } else if (token.tag == TOKEN_KEYWORD_UNION) {
                        new_member.decl.using_kind = Ast_Decl::ANON_USING;
                    } else if (token.tag == TOKEN_KEYWORD_ENUM) {
                        new_member.decl.using_kind = Ast_Decl::ANON_USING;
                    } else {
                        report_error(token, "Unexpected token for %S member statement.", struct_text);
                        return;
                    }
                    new_member.decl.loc = token.loc;
                    new_member.decl.preceding_whitespace_and_comments = token.preceding_whitespace_and_comments;
                    
                    new_member.decl._type_expr = NewAst(Ast_Type_Expr);
                    parse_type_expr(lexer, new_member.decl._type_expr);
                    RetIfError();
                }
                
                *_member = new_member;
                
                token = peek_next_token(lexer);
                if (token.tag == '}') {
                    eat_token(lexer);
                    break;
                }
                new_member = {};
                _member->next = NewAst(Ast_Struct_Member_Node);
                _member = _member->next;
            }
            assert(!_member->next);
        }
    } else if (token.tag == '(') {
        auto open_paren = token;
        token = get_token(lexer);
        Ast_Parameter_Node *first_param = null;
        if (token.tag == ')') {
            // No args, do nothing
        } else if (token.tag == TOKEN_IDENT && peek_next_token(lexer).tag == ':') {
            eat_token(lexer);
            
            auto ident_token = token;
            first_param = NewAst(Ast_Parameter_Node);
            auto param = first_param;
            while (true) {
                parse_decl(lexer, &param->decl, ident_token);
                
                token = peek_next_token(lexer);
                if (token.tag != ',') break;
                
                eat_token(lexer);
                ident_token = get_token(lexer);
                if (ident_token.tag != TOKEN_IDENT) {
                    if (token.tag == ')') {
                        report_error(token, "Found a trailing ',' in a parameter list.");
                    } else {
                        report_error(token, "Unxpected token for a parameter after ',' in a parameter list.");
                    }
                    return;
                }
                
                token = get_token(lexer);
                if (token.tag != ':') {
                    report_error(token, "Expected a ':' after an identifer in a parameter list, but found this:");
                    return;
                }
                
                param->next = NewAst(Ast_Parameter_Node);
                param = param->next;
            }
            
            token = get_token(lexer);
            if (token.tag != ')') {
                report_error(token, "Expected a ')' for a procedure signature, but found this:");
                report_info(open_paren, "... here's the matching '('.");
                return;
            }
        } else {
            report_error(token, "Expected a procedure signature after seeing a '(' in a type expression, but found this:");
            report_info(open_paren, "... here's the matching '('.");
            return;
        }
        
        assert(token.tag == ')');
        auto l1 = token.loc.l1;
        auto c1 = token.loc.c1;
        Ast_Return_Type_Node *first_return_type_expr = null;
        token = peek_next_token(lexer);
        if (token.tag == TOKEN_ARROW) {
            eat_token(lexer);
            first_return_type_expr = NewAst(Ast_Return_Type_Node);
            auto node = first_return_type_expr;
            while (true) {
                parse_type_expr(lexer, node);
                RetIfError();
                token = peek_next_token(lexer);
                if (token.tag == TOKEN_DIRECTIVE_MUST) {
                    node->must_receive = true;
                    eat_token(lexer);
                    l1 = token.loc.l1;
                    c1 = token.loc.c1;
                } else {
                    l1 = node->loc.l1;
                    c1 = node->loc.c1;
                }
                token = peek_next_token(lexer);
                if (token.tag != ',') break;
                eat_token(lexer);
                node->next = NewAst(Ast_Return_Type_Node);
                node = node->next;
            }
        }
        type->tag = Data_Type_Tag::PROC;
        type->proc.first_param = first_param;
        type->proc.first_return_type_expr = first_return_type_expr;
        loc.l1 = l1;
        loc.c1 = c1;
    } else if (token.tag == TOKEN_KEYWORD_TYPEOF) {
        auto typeof_token = token;
        token = get_token(lexer);
        
        if (token.tag != '(') {
            report_error(token, "Expected '(' after 'typeof' keyword, but found this:");
        }
        auto expr = parse_subexpr(lexer, NO_PRECEDENCE);
        RetIfError();
        
        token = get_token(lexer);
        if (token.tag != ')') {
            report_error(token, "Expected ')' after 'typeof' expression, but found this:");
        }
        
        type->tag = Data_Type_Tag::TYPEOF;
        type->typeof.expr = expr;
        loc = typeof_token.loc;
        loc.l1 = token.loc.l1;
        loc.c1 = token.loc.c1;
    } else {
        report_error(token, "Unexpected token for a type expression.");
        return; // @Error: Mention '=', ':' for a type inferrence?
    }
    //auto type_expr = NewAst(Ast_Type_Expr);
    type_expr->loc = loc;
    type_expr->type = dummy_type;
    //return type_expr;
}

internal Ast_Unary *
parse_unary(Lexer *lexer, s32 precedence, Unary_Tag tag) {
    using enum Unary_Tag;
    assert(tag != UNKNOWN);
    
    s32 unary_precedence = 12;
    if (!(unary_precedence >= precedence)) return null; // >= for right-associativity
    
    auto token = get_token(lexer);
    
    Ast_Unary *unary = null;
    
    if (tag == CAST) {
        assert(token.tag == TOKEN_KEYWORD_CAST);
        auto cast = NewAst(Ast_Cast);
        cast->tag = tag;
        cast->loc = token.loc;
        
        token = get_token(lexer);
        if (token.tag != '(') {
            report_error(token, "Expected '(' after 'cast' keyword, but found this:");
            return null;
        }
        parse_type_expr(lexer, &cast->type_expr);
        RetIfError(null);
        
        token = get_token(lexer);
        if (token.tag != ')') {
            report_error(token, "Expected ')' after cast expression, but found this:");
            return null;
        }
        
        unary = cast;
    } else {
        unary = NewAst(Ast_Unary);
        unary->tag = tag;
        unary->loc = token.loc;
    }
    assert(unary);
    
    unary->right = parse_subexpr(lexer, unary_precedence);
    RetIfError(null);
    
    unary->loc.l1 = unary->right->loc.l1;
    unary->loc.c1 = unary->right->loc.c1;
    return unary;
}

internal void
parse_block(Lexer *lexer, Ast_Block *block) {
    auto open_brace = get_token(lexer);
    assert(open_brace.tag == '{');
    
    auto token = peek_next_token(lexer);
    if (token.tag != '}') {
        block->first_statement = NewAst(Ast_Block_Statement_Node);
        auto node = block->first_statement;
        while (true) {
            node->statement = parse_statement(lexer);
            RetIfError();
            if (!node->statement) {
                token = peek_next_token(lexer);
                if (token.tag == TOKEN_END_OF_FILE) {
                    report_error(open_brace, "Unexpected end of file in a block. No matching '}' before file ended.");
                } else {
                    report_error(open_brace, "Internal compiler error: parse_statement returned null.");
                }
                return;
            }
            assert(node->statement->preceding_whitespace_and_comments.count >= 0);
            
            token = peek_next_token(lexer);
            while (token.tag == ';') {
                eat_token(lexer);
                token = peek_next_token(lexer);
            }
            
            if (token.tag == '}') break;
            node->next = NewAst(Ast_Block_Statement_Node);
            node = node->next;
        }
    }
    eat_token(lexer);
}

internal Assign_Tag
get_assign_tag(s32 token_tag) {
    using enum Assign_Tag;
    switch (token_tag) {
        case '=':               return NORMAL;
        case TOKEN_PLUS_EQUAL:  return ADD;
        case TOKEN_MINUS_EQUAL: return SUB;
        case TOKEN_TIMES_EQUAL: return MUL;
        case TOKEN_DIV_EQUAL:   return DIV;
        case TOKEN_MOD_EQUAL:   return MOD;
        
        case TOKEN_BITWISE_AND_EQUAL: return BITWISE_AND;
        case TOKEN_BITWISE_OR_EQUAL:  return BITWISE_OR;
        case TOKEN_BITWISE_XOR_EQUAL: return BITWISE_XOR;
        
        case TOKEN_LEFT_SHIFT_EQUAL:  return LEFT_SHIFT;
        case TOKEN_RIGHT_SHIFT_EQUAL: return RIGHT_SHIFT;
        
        default: return UNKNOWN;
    }
    //return BINARY_UNKNOWN;
}

internal Ast_Expr *
parse_expr(Lexer *lexer) {
    auto token = peek_next_token(lexer);
    switch (token.tag) {
        Case   TOKEN_KEYWORD_STRUCT: // through
        OrCase TOKEN_KEYWORD_UNION: {
            auto type_expr = NewAst(Ast_Type_Expr);
            parse_type_expr(lexer, type_expr);
            RetIfError(null);
            return type_expr;
        }
        
        Case TOKEN_DIRECTIVE_TYPE: {
            eat_token(lexer);
            auto type_expr = NewAst(Ast_Type_Expr);
            parse_type_expr(lexer, type_expr);
            RetIfError(null);
            return type_expr;
        }
        
        Case TOKEN_KEYWORD_INLINE:
        OrCase '(': {
            bool is_inline = (token.tag == TOKEN_KEYWORD_INLINE);
            if (is_inline) {
                eat_token(lexer);
                token = peek_next_token(lexer);
            }
            
            token = peek_token(lexer, 1);
            if (token.tag == ')' || (token.tag == TOKEN_IDENT && peek_token(lexer, 2).tag == ':')) {
                Ast_Type_Expr type_expr; // :GetsDefaultValues
                parse_type_expr(lexer, &type_expr);
                RetIfError(null);
                assert(type_expr.type.tag == Data_Type_Tag::PROC);
                
                token = peek_next_token(lexer);
                if (token.tag == TOKEN_DIRECTIVE_EXPORT || token.tag == TOKEN_DIRECTIVE_EXPAND || token.tag == '{') {
                    
                    auto proc_defn = NewAst(Ast_Proc_Defn);
                    proc_defn->loc       = type_expr.loc;
                    proc_defn->signature = type_expr;
                    
                    // @Hack @Temporary: body get parsed in parse_statement, for recursive calls and proc overloading
                    
                    return proc_defn;
                } else {
                    if (is_inline) {
                        report_error(token, "Expected a procedure definition after 'inline', but found this.");
                        return null;
                    }
                    auto signature = NewAst(Ast_Type_Expr);
                    *signature = type_expr;
                    return signature;
                }
            }
            if (is_inline) {
                report_error(token, "Expected a procedure definition after 'inline', but found this.");
                return null;
            }
        }
    } // switch (token.tag)
    return parse_subexpr(lexer, NO_PRECEDENCE);
}

internal Ast_Switch *
parse_switch (Lexer *lexer, Ast_Subexpr *switch_expr) {
    // @Incomplete: check that switch_expr is a viable expression for a switch
    auto token = get_token(lexer);
    if (token.tag != '{') {
        report_error(token, "Expected '{' after '==' for a switch statement, but found this:");
        return null;
    }
    
    token = get_token(lexer);
    if (token.tag != TOKEN_KEYWORD_CASE) {
        report_error(token, "Expected 'case' at the start of a switch statement block, but found this:");
        return null;
    }
    
    Token default_case = {};
    
    auto first_case = NewAst(Ast_Case);
    auto c = first_case;
    while (true) {
        token = peek_next_token(lexer);
        if (token.tag == ';') {
            if (default_case.tag != TOKEN_UNKNOWN) {
                report_error(token, "Found a second default 'case' in a switch statement.");
                report_info(default_case, "The first default 'case' is here.");
                return null;
            }
            default_case = token;
            eat_token(lexer);
            // (c->expr == null) means it's the default case
        } else {
            c->expr = parse_subexpr(lexer, 0);
            RetIfError(null);
            token = get_token(lexer);
            if (token.tag != ';') {
                report_error(token, "Expected a ';' after the case expression, but found this:");
                return null;
            }
        }
        
        token = peek_next_token(lexer);
        if (token.tag == TOKEN_DIRECTIVE_THROUGH) {
            assert(c->expr);
            c->fall_through = true;
            eat_token(lexer);
            token = peek_next_token(lexer);
            if (token.tag == ';') { // @Hack @Temporary: Only cause no ; makes 4coder's indentaion freak out
                eat_token(lexer);
                token = peek_next_token(lexer);
            }
            if (token.tag != TOKEN_KEYWORD_CASE) {
                report_error(token, "Need a 'case' after a #through, otherwise there's nowhere for the previous case to fall through to.");
                return null;
            }
        }
        
        if (token.tag != TOKEN_KEYWORD_CASE && token.tag != '}') {
            assert(!c->fall_through);
            c->first_statement = NewAst(Ast_Block_Statement_Node);
            auto node = c->first_statement;
            while (true) {
                node->statement = parse_statement(lexer);
                RetIfError(null);
                assert(node->statement && node->statement->preceding_whitespace_and_comments.count > 0);
                
                token = peek_next_token(lexer);
                while (token.tag == ';') {
                    eat_token(lexer);
                    token = peek_next_token(lexer);
                }
                
                if (token.tag == TOKEN_KEYWORD_CASE || token.tag == '}') break;
                node->next = NewAst(Ast_Block_Statement_Node);
                node = node->next;
            }
        }
        
        if (token.tag == '}') break;
        assert(token.tag == TOKEN_KEYWORD_CASE);
        eat_token(lexer);
        c->next = NewAst(Ast_Case);
        c = c->next;
    }
    eat_token(lexer);
    
    auto _switch = NewAst(Ast_Switch);
    _switch->first_case = first_case;
    _switch->switch_expr = switch_expr;
    return _switch;
}

internal Str
to_string(Assign_Tag tag) {
    using enum Assign_Tag;
    switch (tag) {
        case NORMAL:      return sl("=");
        case ADD:         return sl("+=");
        case SUB:         return sl("-=");
        case MUL:         return sl("*=");
        case DIV:         return sl("/=");
        case MOD:         return sl("%=");
        case BITWISE_AND: return sl("&=");
        case BITWISE_OR:  return sl("|=");
        case BITWISE_XOR: return sl("^=");
        case LEFT_SHIFT:  return sl("<<=");
        case RIGHT_SHIFT: return sl(">>=");
        InvalidDefaultCase("Unhandled assign type");
    }
    return sl("ASSIGN_UNKNOWN");;
}

internal Ast_Assignment *
parse_assignment(Lexer *lexer, Ast_Subexpr *left, Assign_Tag tag) {
    using enum Assign_Tag;
    assert(tag != UNKNOWN);
    
    auto assign_token = get_token(lexer);
    assert(get_assign_tag(assign_token.tag) != UNKNOWN);
    
    auto assign = NewAst(Ast_Assignment);
    assign->op_loc = assign_token.loc;
    assign->tag = tag;
    assign->left = left;
    assign->right = parse_subexpr(lexer, NO_PRECEDENCE);
    RetIfError(null);
    
    return assign;
}

internal Ast *
parse_statement(Lexer *lexer) {
    auto token = peek_next_token(lexer);
    auto preceding_whitespace_and_comments = token.preceding_whitespace_and_comments;
    assert(preceding_whitespace_and_comments.count >= 0);
    defer {
        while (peek_next_token(lexer).tag == ';') {
            eat_token(lexer);
        }
    };
    
    switch (token.tag) {
        Case TOKEN_END_OF_FILE: return null;
        Case ';': { ShouldntReachHere; eat_token(lexer); }
        
        OrCase TOKEN_IDENT: {
            auto next_token = peek_token(lexer, 1);
            
            if (next_token.tag == ':' || next_token.tag == ',') {
                eat_token(lexer);
                eat_token(lexer);
                
                auto decl = NewAst(Ast_Decl);
                //bool multi = next_token.tag == ',';
                
                parse_decl(lexer, decl, token);
                RetIfError(null);
                
                if (decl->initialization && decl->initialization->ast_tag == AST_PROC_DEFN) {
                    auto proc_defn = cast(Ast_Proc_Defn *, decl->initialization);
                    
                    bool has_export = false;
                    bool has_expand = false;
                    
                    token = peek_next_token(lexer);
                    while (true) {
                        if (token.tag == TOKEN_DIRECTIVE_EXPORT) {
                            has_export = true;
                            eat_token(lexer);
                            token = peek_next_token(lexer);
                        } else if (token.tag == TOKEN_DIRECTIVE_EXPAND) {
                            has_expand = true;
                            eat_token(lexer);
                            token = peek_next_token(lexer);
                        } else break;
                    }
                    if (token.tag != '{') {
                        report_error(token, "Expected a '{' a procedure definition, but found this.");
                        return null;
                    }
                    if (has_export) decl->ident.flags |= Ast_Ident::IS_EXPORT;
                    
                    parse_block(lexer, &proc_defn->body);
                    RetIfError(null);
                    
                    proc_defn->body.preceding_whitespace_and_comments = token.preceding_whitespace_and_comments;
                }
                
                decl->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
                return decl;
            }
            auto expr = parse_subexpr(lexer, NO_PRECEDENCE);
            RetIfError(null);
            
            token = peek_next_token(lexer);
            auto assign_tag = get_assign_tag(token.tag);
            if (assign_tag != Assign_Tag::UNKNOWN) {
                auto assign = parse_assignment(lexer, expr, assign_tag);
                RetIfError(null);
                assign->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
                return assign;
            }
            expr->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
            return expr;
        }
        
        Case TOKEN_KEYWORD_IF: {
            auto if_token = token;
            eat_token(lexer);
            auto expr = parse_subexpr(lexer, NO_PRECEDENCE);
            RetIfError(null);
            if (!expr) {
                report_error(if_token, "Missing condition after 'if'.");
                return null;
            }
            
            token = peek_next_token(lexer);
            if (get_assign_tag(token.tag) != Assign_Tag::UNKNOWN) {
                report_error(token, "You can only use assignment at statement level. Did you mean '=='?");
                return null;
            }
            
            if (token.tag == TOKEN_IS_EQUAL) {
                eat_token(lexer);
                auto _switch = parse_switch (lexer, expr);
                RetIfError(null);
                _switch->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
                return _switch;
            }
            
            // @Incomplete: pointers and array should get auto-converted to a bool statement
            
            auto branch = NewAst(Ast_Branch);
            branch->condition = expr;
            branch->true_statement = parse_statement(lexer);
            RetIfError(null);
            if (!branch->true_statement) {
                report_error(if_token, "Missing statement after 'if' condition.");
                return null;
            }
            assert(branch->true_statement->preceding_whitespace_and_comments.count >= 0);
            token = peek_next_token(lexer);
            if (token.tag == TOKEN_KEYWORD_ELSE) {
                auto else_token = token;
                eat_token(lexer);
                branch->false_statement = parse_statement(lexer);
                RetIfError(null);
                if (!branch->false_statement) {
                    report_error(else_token, "Missing statement after 'else'.");
                    return null;
                }
                assert(branch->false_statement->preceding_whitespace_and_comments.count >= 0);
            }
            branch->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
            return branch;
        }
        
        Case TOKEN_KEYWORD_WHILE: {
            auto while_token = token;
            eat_token(lexer);
            auto loop = NewAst(Ast_Loop);
            loop->condition = parse_subexpr(lexer, NO_PRECEDENCE);
            RetIfError(null);
            if (!loop->condition) {
                report_error(while_token, "Missing condition after 'while'.");
                return null;
            }
            token = peek_next_token(lexer);
            if (get_assign_tag(token.tag) != Assign_Tag::UNKNOWN) {
                report_error(token, "You can only use assignment at statement level. Did you mean '=='?");
                return null;
            }
            
            loop->statement = parse_statement(lexer);
            RetIfError(null);
            assert(loop->statement->preceding_whitespace_and_comments.count >= 0);
            if (!loop->statement) {
                report_error(while_token, "Missing statement after 'while' condition.");
                return null;
            }
            loop->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
            return loop;
        }
        
        Case TOKEN_KEYWORD_FOR: {
            auto for_token = token;
            eat_token(lexer);
            
            bool reverse = false;
            bool pointer = false;
            
            Token star_token = {};
            token = peek_next_token(lexer);
            if (token.tag == '*') {
                eat_token(lexer);
                star_token = token;
                pointer = true;
            }
            
            token = peek_next_token(lexer);
            if (token.tag == '<') {
                eat_token(lexer);
                reverse = true;
            }
            
            Token it_token = {};
            Token it_index_token = {};
            token = peek_next_token(lexer);
            if (token.tag == TOKEN_IDENT) {
                auto next_token = peek_token(lexer, 1);
                if (next_token.tag == ':' || next_token.tag == ',') {
                    it_token = token;
                    eat_token(lexer);
                    eat_token(lexer);
                    if (next_token.tag == ',') {
                        it_index_token = get_token(lexer);
                        if (it_index_token.tag != TOKEN_IDENT) {
                            report_error(it_index_token, "Expected an identifier after ',' for 'it_index' name override, but found this:");
                            return null;
                        }
                        
                        token = get_token(lexer);
                        if (token.tag != ':') {
                            report_error(token, "Expected a ':' after 'it_index' name override, but found this:");
                            return null;
                        }
                    }
                }
            }
            
            if (it_token.tag == TOKEN_UNKNOWN) {
                it_token = for_token;
                it_token.text = sl("it");
            }
            
            auto first_expr = parse_subexpr(lexer, NO_PRECEDENCE);
            RetIfError(null);
            
            token = peek_next_token(lexer);
            if (get_assign_tag(token.tag) != Assign_Tag::UNKNOWN) {
                report_error(token, "You can only use assignment at statement level. Did you mean '=='?");
                return null;
            }
            
            if (token.tag == TOKEN_DOUBLE_DOT) {
                auto loop = NewAst(Ast_For_Count);
                loop->it.name = it_token.text;
                loop->it.loc = it_token.loc;
                loop->reverse = reverse;
                
                if (pointer) {
                    report_error(star_token, "Can't use '*' on a number-range 'for' loop.");
                    return null;
                }
                
                if (it_index_token.tag != TOKEN_UNKNOWN) {
                    report_error(it_index_token, "Can't override 'it_index' on a number-range 'for' loop.");
                    return null;
                }
                
                loop->start = first_expr;
                eat_token(lexer);
                
                loop->end = parse_subexpr(lexer, NO_PRECEDENCE);
                RetIfError(null);
                token = peek_next_token(lexer);
                if (get_assign_tag(token.tag) != Assign_Tag::UNKNOWN) {
                    report_error(token, "You can only use assignment at statement level. Did you mean '=='?");
                    return null;
                }
                
                loop->statement = parse_statement(lexer);
                RetIfError(null);
                
                assert(loop->statement->preceding_whitespace_and_comments.count >= 0);
                if (!loop->statement) {
                    report_error(for_token, "Missing statement after 'for' condition.");
                    return null;
                }
                loop->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
                return loop;
            } else {
                auto loop = NewAst(Ast_For_Each);
                loop->it.name = it_token.text;
                loop->it.loc = it_token.loc;
                loop->reverse = reverse;
                loop->pointer = pointer;
                
                if (it_index_token.tag == TOKEN_UNKNOWN) {
                    it_index_token = for_token;
                    it_index_token.text = sl("it_index");
                }
                loop->it_index.name = it_index_token.text;
                loop->it_index.loc  = it_index_token.loc;
                
                loop->range = first_expr;
                
                loop->statement = parse_statement(lexer);
                
                assert(loop->statement->preceding_whitespace_and_comments.count >= 0);
                if (!loop->statement) {
                    report_error(for_token, "Missing statement after 'for' condition.");
                    return null;
                }
                loop->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
                return loop;
            }
        }
        
        Case TOKEN_KEYWORD_RETURN: {
            auto return_token = token;
            eat_token(lexer);
            
            Ast_Return_Expr_Node *first_return_expr = null;
            token = peek_next_token(lexer);
            if (token.tag == ';') {
                eat_token(lexer);
            } else {
                first_return_expr = NewAst(Ast_Return_Expr_Node);
                auto node = first_return_expr;
                while (true) {
                    node->expr = parse_subexpr(lexer, NO_PRECEDENCE);
                    RetIfError(null);
                    token = peek_next_token(lexer);
                    if (get_assign_tag(token.tag) != Assign_Tag::UNKNOWN) {
                        report_error(token, "You can only use assignment at statement level. Did you mean '=='?");
                        return null;
                    }
                    if (token.tag != ',') break;
                    eat_token(lexer);
                    node->next = NewAst(Ast_Return_Expr_Node);
                    node = node->next;
                }
            }
            
            auto _return = NewAst(Ast_Return);
            _return->loc = return_token.loc;
            _return->first = first_return_expr;
            _return->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
            return _return;
        }
        
        Case TOKEN_KEYWORD_BREAK: {
            eat_token(lexer);
            auto _break = NewAst(Ast_Break);
            _break->loc = token.loc;
            _break->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
            return _break;
        }
        
        Case TOKEN_KEYWORD_CONTINUE: {
            eat_token(lexer);
            auto _continue = NewAst(Ast_Continue);
            _continue->loc = token.loc;
            _continue->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
            return _continue;
        }
        
        Case '{': {
            auto block = NewAst(Ast_Block);
            parse_block(lexer, block);
            RetIfError(null);
            block->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
            return block;
        }
        
        Case TOKEN_DIRECTIVE_IF: {
            if (token.tag == TOKEN_DIRECTIVE_IF) {
                report_error(token, "#if is disabled for now until I restructure the compiler.");
                return null;
            }
            auto if_token = token;
            eat_token(lexer);
            
            auto branch = NewAst(Ast_Static_Branch);
            
            branch->condition = parse_subexpr(lexer, NO_PRECEDENCE);
            RetIfError(null);
            if (!branch->condition) {
                report_error(if_token, "Missing 'if' condition expression.");
                return null;
            }
            
            branch->true_statement = parse_statement(lexer);
            RetIfError(null);
            if (!branch->true_statement) {
                report_error(if_token, "Missing statement after 'if' condition.");
                return null;
            }
            assert(branch->true_statement->preceding_whitespace_and_comments.count >= 0);
            token = peek_next_token(lexer);
            if (token.tag == TOKEN_KEYWORD_ELSE) {
                eat_token(lexer);
                branch->false_statement = parse_statement(lexer);
                RetIfError(null);
                if (!branch->false_statement) {
                    report_error(token, "Missing statement after 'else'.");
                    return null;
                }
                assert(branch->false_statement->preceding_whitespace_and_comments.count >= 0);
            }
            branch->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
            return branch;
        }
        
        Default: {
            auto unary_tag = get_unary_tag(token.tag);
            if (unary_tag != Unary_Tag::UNKNOWN) {
                auto expr = parse_subexpr(lexer, NO_PRECEDENCE);
                RetIfError(null);
                
                token = peek_next_token(lexer);
                auto assign_tag = get_assign_tag(token.tag);
                if (assign_tag != Assign_Tag::UNKNOWN) {
                    auto assign = parse_assignment(lexer, expr, assign_tag);
                    RetIfError(null);
                    assign->preceding_whitespace_and_comments = preceding_whitespace_and_comments;
                    return assign;
                }
            }
            report_error(token, "Unexpected token '%S' at start of statement. (token.tag = %d) This is a pretty bad error message, but it's just being used as a placeholder for now.", token.text, token.tag);
        }
    }
    
    return null;
}

internal void print_statement(Ast *ast);
//internal void write_c_file(s32 statement_count, Ast *statements[]);
internal Str read_entire_file(char *filename);

internal void
parse_file(Array<Ast *> &statements, Str fully_pathed_file_name) {
    auto file = read_entire_file(fully_pathed_file_name.data); // @Incomplete @Error
    if (!file.data) {
        report_error_noloc("Somehow failed to open file: %S", fully_pathed_file_name);
        return;
    }
    
    
    Lexer lexer = {};
    lexer.fully_pathed_source_file_name = fully_pathed_file_name;
    lexer.rest_of_file = file;
    lexer.current_line = consume_next_line(&lexer.rest_of_file);
    lexer.prev_line_start = lexer.current_line.data;
    lexer.prev_prev_line_start = lexer.current_line.data;
    lexer.previous_tokens_l1 = 1;
    lexer.relevant_lines = lexer.current_line;
    
    while (true) {
        Ast *statement = null;
        auto token = peek_next_token(&lexer);
        if (token.tag == TOKEN_DIRECTIVE_LOAD) {
            eat_token(&lexer);
            
            auto expr = parse_expr(&lexer);
            RetIfError();
            
            Ast_Load *load = null;
            if (expr->ast_tag == AST_LITERAL) {
                auto literal = cast(Ast_Literal *, expr);
                if (literal->tag == Ast_Literal::STRING) {
                    load = NewAst(Ast_Load);
                    load->file = literal;
                    load->preceding_whitespace_and_comments = token.preceding_whitespace_and_comments;
                }
            }
            if (!load) {
                report_error(expr, "Expected a string after #load directive, but found this.");
                return;
            }
            statement = load;
        } else {
            statement = parse_statement(&lexer);
            RetIfError();
        }
        if (!statement) break;
        
        assert(statement->preceding_whitespace_and_comments.count >= 0);
        
        statements.push(statement);
        
        if (statement->ast_tag == AST_LOAD) {
            Scoped_Arena scratch;
            auto load = cast(Ast_Load *, statement);
            auto fully_pathed_new_file_name = arena_print(scratch, "%S%S", chai.path_to_input_file, load->file->s);
            parse_file(statements, fully_pathed_new_file_name);
            //chai.fully_pathed_current_file_name = fully_pathed_file_name;
        }
    }
}

internal void
parse_everything(Array<Ast *> &statements, Str fully_pathed_first_file_name) {
    
    parse_file(statements, fully_pathed_first_file_name);
}