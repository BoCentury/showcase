
internal inline void
add_variable(Ast_Ident *ident) {
    assert(chai.variable_count < ArrayCount(chai.variables));
    
    ident->variable_index = chai.variable_count;
    chai.variables[chai.variable_count] = {};
    chai.variables[chai.variable_count].tag   = VARIABLE_NORMAL;
    chai.variables[chai.variable_count].ident = ident;
    chai.variable_count += 1;
}

internal inline s32
add_push_scope_variable() {
    assert(chai.variable_count < ArrayCount(chai.variables));
    
    s32 push_index = chai.variable_count;
    chai.variables[chai.variable_count].tag = VARIABLE_PUSH_SCOPE;
    chai.variable_count += 1;
    return push_index;
}

internal inline void
add_pop_scope_variable(s32 push_index) {
    assert(chai.variable_count < ArrayCount(chai.variables));
    
    chai.variables[chai.variable_count].tag = VARIABLE_POP_SCOPE;
    chai.variables[chai.variable_count].matching_push_index = push_index;
    chai.variable_count += 1;
}

internal s32
find_variable_index(Str name, s32 start_index = -1) {
    if (start_index < 0) start_index = chai.variable_count-1;
    ForIIR (i, start_index, 0) {
        auto it = chai.variables[i];
        if (it.tag == VARIABLE_PUSH_SCOPE) continue;
        if (it.tag == VARIABLE_POP_SCOPE) {
            assert(it.matching_push_index >= 0);
            assert(it.matching_push_index < i);
            i = it.matching_push_index;
            continue;
        }
        
        if (name == it.ident->name) {
            return i;
        }
    }
    return -1;
}

internal Data_Type
get_data_type(Ast_Expr *expr) {
    Data_Type data_type = {};
    switch (expr->ast_tag) {
        Case   AST_UNARY:     { data_type = cast(Ast_Unary     *, expr)->data_type; }
        Case   AST_BINARY:    { data_type = cast(Ast_Binary    *, expr)->data_type; }
        Case   AST_LITERAL:   { data_type = cast(Ast_Literal   *, expr)->data_type; }
        Case   AST_SIZEOF:    { data_type = cast(Ast_Sizeof    *, expr)->data_type; }
        Case   AST_TYPE_EXPR: { data_type = cast(Ast_Type_Expr *, expr)->type;      }
        
        Case AST_IDENT: {
            auto ident = cast(Ast_Ident *, expr);
            data_type = chai.variables[ident->variable_index].ident->data_type;
        }
        
        Case AST_CALL: {
            auto call = cast(Ast_Call *, expr);
            data_type = chai.variables[call->variable_index].ident->data_type;
            assert(data_type.tag == Data_Type_Tag::PROC);
            if (    data_type.proc.first_return_type_expr
                && !data_type.proc.first_return_type_expr->next) {
                data_type = data_type.proc.first_return_type_expr->type;
            }
        }
        
        Case AST_PROC_DEFN: {
            auto proc = cast(Ast_Proc_Defn *, expr);
            assert(proc->signature.type.tag == Data_Type_Tag::PROC);
            data_type = proc->signature.type;
        }
        
        InvalidDefaultCase("Unhandled ast type");
    }
    return data_type;
}

// :StringBuilder
internal Str
__append_type_and_advance(Str at, Data_Type type) {
    using enum Data_Type_Tag;
    
#define AddString(s, ...) at = advance(at, snprint(at.data, cast_s32(at.count), s, ##__VA_ARGS__));
    while (true) {
        if (type.tag == POINTER) {
            AddString("*");
            type = *type.pointer.to;
        } else if (type.tag == STATIC_ARRAY) {
            AddString("[n]"); // @Incomplete: :Constexpr
            type = *type.array.of;
        } else {
            break;
        }
    }
    
    if (type.user_type_ident) {
        AddString("%S", type.user_type_ident->name);
    } else switch (type.tag) {
        Case INT: {
            if (type.number.is_solid) {
                assert(type.number.size == 1 || type.number.size == 2 || type.number.size == 4 || type.number.size == 8);
                AddString("%c%d", type.number.is_signed ? 's' : 'u', type.number.size*8);
            } else {
                assert(type.number.size == 0);
                AddString("UNSOLIDIFIED_INT"); // @Incomplete
            }
        }
        Case FLOAT: {
            if (type.number.is_solid) {
                assert(type.number.size == 4 || type.number.size == 8);
                AddString("f%d", type.number.size*8);
            } else {
                assert(type.number.size == 0);
                AddString("UNSOLIDIFIED_FLOAT"); // @Incomplete
            }
        }
        Case BOOL: {
            AddString("bool");
        }
        Case VOID: {
            AddString("void");
        }
        Case STRING: {
            AddString("string");
        }
        Case USER_TYPE: {
            AddString("Type");
        }
        Case PROC: {
            AddString("(");
            auto param = type.proc.first_param;
            // :MultiReturnRefactor assert((type.proc.multi_return_index > 0) == (param->decl.ident.next != null));
            while (true) {
                AddString("%S: ", param->decl.ident.name);
                at = __append_type_and_advance(at, get_data_type(&param->decl.ident));
                if (!param->next) break;
                param = param->next;
                AddString(", ");
            }
            AddString(")");
            if (type.proc.first_return_type_expr) {
                if (type.proc.first_return_type_expr->next) {
                    AddString(" -> (");
                } else {
                    AddString(" -> ");
                }
                auto return_type_expr = type.proc.first_return_type_expr;
                while (true) {
                    at = __append_type_and_advance(at, return_type_expr->type);
                    if (!return_type_expr->next) break;
                    AddString(", ");
                    return_type_expr = return_type_expr->next;
                }
                if (type.proc.first_return_type_expr->next) {
                    AddString(")");
                }
            }
        }
        Case STRUCT: {
            AddString("***STRUCT***");
        }
        InvalidDefaultCase("Unhandled data type");
    }
#undef AddString
    return at;
}

internal Str
tprint_type(Data_Type type) {
    // :StringBuilder
    auto free_memory = xxx_arena_get_free_memory(&g_frame_arena);
    assert(free_memory.count > 0);
    if (free_memory.count > S32_MAX)  free_memory.count = S32_MAX;
    
    auto whats_left = __append_type_and_advance(free_memory, type);
    assert(whats_left.count < free_memory.count);
    
    auto result = Str{ .count = free_memory.count-whats_left.count, .data = free_memory.data };
    assert(result.data[result.count] == '\0');
    
    xxx_arena_commit_used_bytes(&g_frame_arena, result.count);
    
    return result;
}

internal void
set_loose_number_type_expr_data_type_another_loose_type(Ast_Expr *expr, Data_Type new_type) {
    assert(is_number_type(new_type.tag));
    assert(!new_type.number.is_solid);
    switch (expr->ast_tag) {
        // @Copypaste: from get_data_type
        Case AST_LITERAL: {
            auto literal = cast(Ast_Literal *, expr);
            assert(is_number_type(literal->data_type.tag));
            assert(!literal->data_type.number.is_solid);
            assert(literal->data_type.number.size <= new_type.number.size);
            literal->data_type = new_type;
        }
        Case AST_IDENT: {
            // Do nothing. Should only come here if you're a non-solid constant.
#if _INTERNAL_BUILD
            auto ident = cast(Ast_Ident *, expr);
            assert(ident->variable_index >= 0);
            auto variable = chai.variables[ident->variable_index];
            assert(is_number_type(variable.ident->data_type.tag));
            assert(variable.ident->flags & Ast_Ident::CONSTANT);
#endif
        }
        Case AST_SIZEOF: {
            auto size_of = cast(Ast_Sizeof *, expr);
            size_of->data_type = new_type;
        }
        Case AST_BINARY: {
            auto binary = cast(Ast_Binary *, expr);
            assert(is_number_type(binary->data_type.tag));
            if (binary->tag == Binary_Tag::DOT) { // right is always an ident
                assert(!"Shouldn't be here cause all . operations should be solid type. A");
            } else if (binary->tag == Binary_Tag::ARRAY_ACCESS) { // right is in [] and doesn't contribute
                assert(!"Idk if it makes any sense to reach here. A");
            } else {
                set_loose_number_type_expr_data_type_another_loose_type(binary->right, new_type);
            }
            set_loose_number_type_expr_data_type_another_loose_type(binary->left,  new_type);
            binary->data_type = new_type;
        }
        Case AST_UNARY: {
            auto unary = cast(Ast_Unary *, expr);
            assert(is_number_type(unary->data_type.tag));
            set_loose_number_type_expr_data_type_another_loose_type(unary->right, new_type);
            unary->data_type = new_type;
        }
        Default: {
            report_error(expr, "Internal compiler error: Trying to set data type of an unhandled expression.");
        }
    }
}

internal void
set_loose_number_type_expr_data_type_to_new_solid_type(Ast_Subexpr *expr, Data_Type new_type, bool literal_is_negated = false) {
    assert(is_number_type(new_type.tag));
    assert(new_type.number.is_solid);
    switch (expr->ast_tag) {
        // @Copypaste: from get_data_type
        Case AST_LITERAL: {
            auto literal = cast(Ast_Literal *, expr);
            assert(is_number_type(literal->data_type.tag));
            assert(!literal->data_type.number.is_solid);
            assert(!literal->is_char);
            
            using enum Data_Type_Tag;
            auto text = literal->text;
            
            if (literal->data_type.tag == FLOAT) {
                assert(new_type.tag == FLOAT);
                /*auto saved = literal->text.data[literal->text.count];
                literal->text.data[literal->text.count] = 0;
                if (new_type.number.size == 4) {
                    literal->_f32 = strtof(literal->text.data, null);
                } else {
                    assert(new_type.number.size == 8);
                    literal->_f64 = strtod(literal->text.data, null);
                }
                literal->text.data[literal->text.count] = saved;*/
            } else {
                if (starts_with(text, sl("0x"))) {
                    assert(literal->data_type.number.size <= new_type.number.size);
                } else {
                    u64 value = 0;
                    ForI (i, 0, text.count) {
                        assert(is_numeric(text.data[i]));
                        auto old = value;
                        value *= 10;
                        if (value/10 != old) {
                            report_error(literal, "Decimal literal is too big and can't fit into a u64.");
                            return;
                        }
                        value += (text.data[i] - '0');
                    }
                    
                    s32 min_literal_size = 0;
                    if (new_type.number.is_signed) {
                        auto test = value;
                        if (literal_is_negated && value > 0) test -= 1;
                        
                        if (test <= S8_MAX) {
                            min_literal_size = 1;
                        } else if (test <= S16_MAX) {
                            min_literal_size = 2;
                        } else if (test <= S32_MAX) {
                            min_literal_size = 4;
                        } else if (test <= S64_MAX) {
                            min_literal_size = 8;
                        }
                    } else {
                        if (value <= U8_MAX) {
                            min_literal_size = 1;
                        } else if (value <= U16_MAX) {
                            min_literal_size = 2;
                        } else if (value <= U32_MAX) {
                            min_literal_size = 4;
                        } else if (value <= U64_MAX) {
                            min_literal_size = 8;
                        } else ShouldntReachHere;
                    }
                    
                    if (min_literal_size == 0 || new_type.number.size < min_literal_size) {
                        report_error(literal, "Integer literal is too big to fit into type %S.", tprint_type(new_type));
                        return;
                    }
                    literal->_u64 = value;
                }
            }
            literal->data_type = new_type;
        }
        Case AST_IDENT: {
            // Do nothing.
            // Should only come here if you're a non-solid constant.
            // Don't need to solidify constant number cause they're just #defines in the C code.
#if _INTERNAL_BUILD
            auto ident = cast(Ast_Ident *, expr);
            assert(ident->variable_index >= 0);
            auto variable = chai.variables[ident->variable_index];
            assert(is_number_type(variable.ident->data_type.tag));
            assert(variable.ident->flags & Ast_Ident::CONSTANT);
#endif
        }
        Case AST_SIZEOF: {
            assert(new_type.tag == Data_Type_Tag::INT);
            auto size_of = cast(Ast_Sizeof *, expr);
            size_of->data_type = new_type;
        }
        Case AST_BINARY: {
            auto binary = cast(Ast_Binary *, expr);
            assert(is_number_type(binary->data_type.tag));
            if (binary->tag == Binary_Tag::DOT) { // right is always an ident
                assert(!"Shouldn't be here cause all . operations should be solid type. B");
            } else if (binary->tag == Binary_Tag::ARRAY_ACCESS) { // right is in [] and doesn't contribute
                assert(!"Idk if it makes any sense to reach here. B");
            } else if (binary->tag == Binary_Tag::LEFT_SHIFT) {
                // right is just some int it doesn't matter
            } else {
                set_loose_number_type_expr_data_type_to_new_solid_type(binary->right, new_type);
            }
            set_loose_number_type_expr_data_type_to_new_solid_type(binary->left,  new_type);
            binary->data_type = new_type;
        }
        Case AST_UNARY: {
            auto unary = cast(Ast_Unary *, expr);
            assert(is_number_type(unary->data_type.tag));
            set_loose_number_type_expr_data_type_to_new_solid_type(unary->right, new_type, unary->tag == Unary_Tag::MINUS);
            unary->data_type = new_type;
        }
        Default: {
            report_error(expr, "Internal compiler error: Trying to set data type of an unhandled expression.");
        }
    }
}

internal Data_Type
solidify_number_expr_data_type_to_default(Ast_Subexpr *expr, Data_Type type) {
    assert(is_number_type(type.tag));
    assert(!type.number.is_solid);
    
    type.number.is_solid = true;
    if (type.tag == Data_Type_Tag::INT) {
        type.number.is_signed = true;
        type.number.size = 8;
    } else {
        type.number.size = 4;
    }
    set_loose_number_type_expr_data_type_to_new_solid_type(expr, type);
    
    return type;
}

enum struct Typecheck_Result {
    ALL_GOOD,
    
    // Errors:
    TYPE_MISMATCH,
    //SIGN_MISMATCH, // :Signed
    SIZE_MISMATCH,
    LOOSE_R_TOO_BIG_FOR_L,
    LOOSE_L_TOO_BIG_FOR_R, // Not used in _one_way
    
    // Action needed:
    SET_R_TO_L,
    SET_L_TO_R, // Not used in _one_way
};

internal Typecheck_Result
_solidify_number_expressions(Ast_Subexpr *left, Data_Type left_type, Ast_Subexpr *right, Data_Type right_type, Data_Type *resulting_type) {
    assert(is_number_type(left_type.tag));
    assert(is_number_type(right_type.tag));
    using enum Data_Type_Tag;
    using enum Typecheck_Result;
    bool should_copy_right = false;
    
    // :Signed
    if (left_type.tag == right_type.tag) {
        if (left_type.number.is_solid && right_type.number.is_solid) {
            if (left_type.number.size != right_type.number.size) return SIZE_MISMATCH;
            if (left_type.tag == INT) {
                if (left_type.number.is_signed != right_type.number.is_signed) return TYPE_MISMATCH; //SIGN_MISMATCH;
            }
        } else if (left_type.number.is_solid) {
            if (left_type.number.size < right_type.number.size) return LOOSE_R_TOO_BIG_FOR_L;
            if (left_type.tag == INT) {
                if (!left_type.number.is_signed && right_type.number.is_signed) return TYPE_MISMATCH; //SIGN_MISMATCH;
            }
            set_loose_number_type_expr_data_type_to_new_solid_type(right, left_type);
        } else if (right_type.number.is_solid) {
            if (left_type.number.size > right_type.number.size) return LOOSE_L_TOO_BIG_FOR_R;
            if (left_type.tag == INT) {
                if (left_type.number.is_signed && !right_type.number.is_signed) return TYPE_MISMATCH; //SIGN_MISMATCH;
            }
            set_loose_number_type_expr_data_type_to_new_solid_type(left, right_type);
            should_copy_right = true;
        } else {
            if (left_type.number.size < right_type.number.size) {
                set_loose_number_type_expr_data_type_another_loose_type(left, right_type);
                should_copy_right = true;
            } else if (left_type.number.size > right_type.number.size) {
                set_loose_number_type_expr_data_type_another_loose_type(right, left_type);
            }
        }
    } else {
        if (left_type.number.is_solid && right_type.number.is_solid) {
            return TYPE_MISMATCH;
        } else if (left_type.number.is_solid) {
            if (left_type.tag != FLOAT) return TYPE_MISMATCH;
            set_loose_number_type_expr_data_type_to_new_solid_type(right, left_type);
        } else if (right_type.number.is_solid) {
            if (right_type.tag != FLOAT) return TYPE_MISMATCH;
            set_loose_number_type_expr_data_type_to_new_solid_type(left, right_type);
            should_copy_right = true;
        } else {
            if (left_type.tag == FLOAT) {
                set_loose_number_type_expr_data_type_another_loose_type(right, left_type);
            } else {
                set_loose_number_type_expr_data_type_another_loose_type(left, right_type);
                should_copy_right = true;
            }
        }
    }
    if (resulting_type) {
        if (should_copy_right) *resulting_type = right_type;
        else                  *resulting_type =  left_type;
    }
    return ALL_GOOD;
}

internal Typecheck_Result
typecheck_number_types(Data_Type left_type, Data_Type right_type) {
    assert(is_number_type(left_type.tag));
    assert(is_number_type(right_type.tag));
    using enum Data_Type_Tag;
    using enum Typecheck_Result;
    
    // :Signed
    if (left_type.tag == right_type.tag) {
        if (left_type.number.is_solid && right_type.number.is_solid) {
            if (left_type.number.size != right_type.number.size) return SIZE_MISMATCH;
            if (left_type.tag == INT) {
                if (left_type.number.is_signed != right_type.number.is_signed) return TYPE_MISMATCH; //SIGN_MISMATCH;
            }
            return ALL_GOOD;
        } else if (left_type.number.is_solid) {
            if (left_type.number.size < right_type.number.size) return LOOSE_R_TOO_BIG_FOR_L;
            if (left_type.tag == INT) {
                if (!left_type.number.is_signed && right_type.number.is_signed) return TYPE_MISMATCH; //SIGN_MISMATCH;
            }
            return SET_R_TO_L;
        } else if (right_type.number.is_solid) {
            if (left_type.number.size > right_type.number.size) return LOOSE_L_TOO_BIG_FOR_R;
            if (left_type.tag == INT) {
                if (left_type.number.is_signed && !right_type.number.is_signed) return TYPE_MISMATCH; //SIGN_MISMATCH;
            }
            return SET_L_TO_R;
        } else {
            if (left_type.number.size < right_type.number.size) {
                return SET_L_TO_R; // Copy right size to left
            } else if (left_type.number.size > right_type.number.size) {
                return SET_R_TO_L; // Copy left size to right
            }
            return ALL_GOOD;
        }
    } else {
        if (left_type.number.is_solid && right_type.number.is_solid) {
            return TYPE_MISMATCH;
        } else if (left_type.number.is_solid) {
            if (left_type.tag != FLOAT) return TYPE_MISMATCH;
            return SET_R_TO_L;
        } else if (right_type.number.is_solid) {
            if (right_type.tag != FLOAT) return TYPE_MISMATCH;
            return SET_L_TO_R;
        } else {
            if (left_type.tag == FLOAT) {
                return SET_R_TO_L;
            } else {
                return SET_L_TO_R;
            }
        }
    }
}

internal Typecheck_Result
typecheck_number_types_one_way(Data_Type solid_type, Data_Type other_type) {
    assert(is_number_type(solid_type.tag));
    assert(is_number_type(other_type.tag));
    assert(solid_type.number.is_solid);
    using enum Data_Type_Tag;
    using enum Typecheck_Result;
    // :Signed
    if (solid_type.tag == other_type.tag) {
        if (other_type.number.is_solid) {
            if (solid_type.number.size != other_type.number.size) return SIZE_MISMATCH;
            if (solid_type.tag == INT) {
                if (solid_type.number.is_signed != other_type.number.is_signed) return TYPE_MISMATCH; //SIGN_MISMATCH;
            }
            return ALL_GOOD;
        } else {
            if (solid_type.number.size < other_type.number.size) return LOOSE_R_TOO_BIG_FOR_L;
            if (solid_type.tag == INT) {
                if (!solid_type.number.is_signed && other_type.number.is_signed) return TYPE_MISMATCH; //SIGN_MISMATCH;
            }
            return SET_R_TO_L;
        }
    } else {
        if (other_type.number.is_solid) {
            return TYPE_MISMATCH;
        } else {
            if (solid_type.tag != Data_Type_Tag::FLOAT) return TYPE_MISMATCH;
            return SET_R_TO_L;
        }
    }
}

internal bool
is_lvalue(Ast_Expr *expr) { // @Cleanup
    if (expr->ast_tag == AST_IDENT) {
        auto ident = cast(Ast_Ident *, expr);
        return !(ident->flags & Ast_Ident::CONSTANT);
    } else if (expr->ast_tag == AST_BINARY) {
        auto binary = cast(Ast_Binary *, expr);
        if (binary->tag == Binary_Tag::DOT) {
            return is_lvalue(binary->left);
        } else if (binary->tag == Binary_Tag::ARRAY_ACCESS) {
            assert(get_data_type(binary->left).tag == Data_Type_Tag::STATIC_ARRAY || get_data_type(binary->left).tag == Data_Type_Tag::POINTER || get_data_type(binary->left).tag == Data_Type_Tag::STRING);
            return is_lvalue(binary->left);
        }
    } else if (expr->ast_tag == AST_UNARY) {
        auto unary = cast(Ast_Unary *, expr);
        if (unary->tag == Unary_Tag::DEREFERENCE) {
            assert(get_data_type(unary->right).tag == Data_Type_Tag::POINTER);
            return true;
        }
    }
    return false;
}

internal Typecheck_Result
typecheck_assign(Data_Type left_type, Data_Type right_type) {
    using enum Data_Type_Tag;
    using enum Typecheck_Result;
    
    // @Copypaste: :CopySolidifyAssign
    // @Copypaste: :PointerChecking
    bool mismatch = false;
    if (left_type.tag == POINTER || left_type.tag == STATIC_ARRAY) {
        auto left_seek = left_type;
        auto right_seek = right_type;
        bool last_was_pointer = false;
        while (true) {
            if (right_seek.tag == STATIC_ARRAY) {
                if (left_seek.tag != STATIC_ARRAY) {
                    mismatch = true;
                    break;
                }
                right_seek = *right_seek.array.of;
                left_seek  = *left_seek.array.of;
                last_was_pointer = false;
            } else if (right_seek.tag == POINTER) {
                if (left_seek.tag != POINTER) {
                    mismatch = true;
                    break;
                }
                right_seek = *right_seek.pointer.to;
                left_seek  = *left_seek.pointer.to;
                last_was_pointer = true;
            } else {
                if (left_seek.tag == right_seek.tag) {
                    if (is_number_type(left_seek.tag)) {
                        assert(left_seek.number.is_solid && right_seek.number.is_solid);
                        if (left_seek.number.size != right_seek.number.size) {
                            mismatch = true;
                        } else if (left_seek.tag == INT && left_seek.number.is_signed != right_seek.number.is_signed) {
                            mismatch = true;
                        }
                    }
                } else {
                    mismatch = true;
                    if (last_was_pointer) {
                        if (right_seek.tag == VOID) {
                            if (right_seek.void_is_null) {
                                mismatch = false;
                            }
                        }
                    }
                }
                break;
            }
        }
    } else if (is_number_type(left_type.tag) && is_number_type(right_type.tag)) {
        return typecheck_number_types_one_way(left_type, right_type);
    } else if (left_type.tag != right_type.tag) {
        mismatch = true;
    }
    if (mismatch) {
        return TYPE_MISMATCH;
    }
    return ALL_GOOD;
}

internal void
typecheck_and_maybe_solidify_binary(Ast_Binary *binary) {
    auto left  = binary->left;
    auto right = binary->right;
    auto left_type  = get_data_type(left);
    auto right_type = get_data_type(right);
    
    if (left_type.tag == Data_Type_Tag::USER_TYPE) {
        report_error(left, "Found a type identifer to left of '%S'.", to_string(binary->tag));
        return;
    }
    if (right_type.tag == Data_Type_Tag::USER_TYPE) {
        report_error(left, "Found a type identifer to right of '%S'.", to_string(binary->tag));
        return;
    }
    
    // Type-check and maybe solidify operands :Solidify
    
    using enum Binary_Tag;
    switch (binary->tag) {
        using enum Data_Type_Tag;
        Case   LOGICAL_AND:
        OrCase LOGICAL_OR: { // :TypeInference
            if (left_type.tag != BOOL) {
                if (left_type.tag == Data_Type_Tag::POINTER) {
                    auto _null = NewAst(Ast_Literal);
                    _null->tag = Ast_Literal::NULL;
                    _null->loc = left->loc;
                    
                    _null->data_type.tag = Data_Type_Tag::POINTER;
                    _null->data_type.pointer.to = AllocDataTypeUninitialized();
                    *_null->data_type.pointer.to = {};
                    _null->data_type.pointer.to->tag = Data_Type_Tag::VOID;
                    _null->data_type.pointer.to->void_is_null = true;
                    
                    auto new_binary = NewAst(Ast_Binary);
                    new_binary->tag = Binary_Tag::NOT_EQUAL;
                    new_binary->data_type.tag = Data_Type_Tag::BOOL;
                    new_binary->left = left;
                    new_binary->right = _null;
                    
                    left = new_binary;
                } else {
                    report_error(left, "Expected bool type for left side of '%S', but found type %S.", to_string(binary->tag), tprint_type(left_type));
                    return;
                }
            }
            if (right_type.tag != BOOL) {
                if (right_type.tag == Data_Type_Tag::POINTER) {
                    auto _null = NewAst(Ast_Literal);
                    _null->tag = Ast_Literal::NULL;
                    _null->loc = right->loc;
                    
                    _null->data_type.tag = Data_Type_Tag::POINTER;
                    _null->data_type.pointer.to = AllocDataTypeUninitialized();
                    *_null->data_type.pointer.to = {};
                    _null->data_type.pointer.to->tag = Data_Type_Tag::VOID;
                    _null->data_type.pointer.to->void_is_null = true;
                    
                    auto new_binary = NewAst(Ast_Binary);
                    new_binary->tag = Binary_Tag::NOT_EQUAL;
                    new_binary->data_type.tag = Data_Type_Tag::BOOL;
                    new_binary->left = right;
                    new_binary->right = _null;
                    
                    right = new_binary;
                } else {
                    report_error(right, "Expected bool type for right side of '%S', but found type %S.", to_string(binary->tag), tprint_type(right_type));
                    return;
                }
            }
            binary->data_type.tag = BOOL;
        }
        
        Case   LEFT_SHIFT:
        OrCase RIGHT_SHIFT: {
            if (left_type.tag != INT) {
                report_error(left, "Left expression isn't a valid type for '%S': %S.", to_string(binary->tag), tprint_type(left_type));
                return;
            }
            if (right_type.tag != INT) {
                report_error(right, "Right expression isn't a valid type for '%S': %S.", to_string(binary->tag), tprint_type(right_type));
                return;
            }
            if (!right_type.number.is_solid)  solidify_number_expr_data_type_to_default(right, right_type);
            
            binary->data_type = left_type;
        }
        
        Case ARRAY_ACCESS: {
            if (left_type.tag != STATIC_ARRAY && left_type.tag != STRING && left_type.tag != POINTER) {
                report_error(left, "Trying to array-access a non-array, non-string type %S.", tprint_type(left_type));
                return;
            }
            if (right_type.tag != INT) {
                report_error(left, "Expected integer type for array-access expression, but found type %S.", tprint_type(left_type));
                return;
            }
            if (left_type.tag == STATIC_ARRAY) {
                binary->data_type = *left_type.array.of;
            } else if (left_type.tag == POINTER) {
                binary->data_type = *left_type.pointer.to;
            } else { assert(left_type.tag == STRING);
                binary->data_type = *chai.string_data_type.pointer.to;
            }
        }
        
        Case DOT: {
            // Already type-checked from above
            //binary->data_type = member->decl->type_expr.type;
            ShouldntReachHere;
        }
        
        Default: {
            auto tag = binary->tag;
            bool need_int_operands = tag == MOD || tag == BITWISE_AND || tag == BITWISE_XOR || tag == BITWISE_OR;
            bool binary_is_bool_type = tag == GREATER_THAN || tag == LESS_THAN || tag == GREATER_THAN_OR_EQUAL || tag == LESS_THAN_OR_EQUAL || tag == IS_EQUAL || tag == NOT_EQUAL;
            assert(need_int_operands || binary_is_bool_type || tag == ADD || tag == SUB || tag == MUL || tag == DIV);
            
            if (need_int_operands) {
                if (left_type.tag != INT) {
                    report_error(left, "Expected integer type for left side of '%S', but found type %S.", to_string(binary->tag), tprint_type(left_type));
                    return;
                }
                if (right_type.tag != INT) {
                    report_error(right, "Expected integer type for right side of '%S', but found type %S.", to_string(binary->tag), tprint_type(right_type));
                    return;
                }
            } else {
                if (left_type.tag == POINTER || right_type.tag == POINTER) {
                    if (tag == ADD) {
                        if (left_type.tag == POINTER && right_type.tag == POINTER) {
                            report_error(binary, "Left and right sides of '+' can't both be pointers. Left: %S, Right: %S", tprint_type(left_type), tprint_type(right_type));
                            return;
                        } else if (left_type.tag == POINTER) {
                            if (right_type.tag != INT) {
                                report_error(binary, "A pointer type on the left side of '+' needs an integer type on the right. Left: %S, Right: %S", tprint_type(left_type), tprint_type(right_type));
                                return;
                            }
                            binary->data_type = left_type;
                            return;
                        } else { assert(right_type.tag == POINTER);
                            if (left_type.tag != INT) {
                                report_error(binary, "A pointer type on the right side of '+' needs an integer type on the left. Left: %S, Right: %S", tprint_type(left_type), tprint_type(right_type));
                                return;
                            }
                            binary->data_type = right_type;
                            return;
                        }
                    } else if (tag == SUB) {
                        if (left_type.tag == POINTER && right_type.tag == POINTER) {
                            // @Copypaste: :PointerChecking
                            bool mismatch = false;
                            auto l_seek  = left_type;
                            auto r_seek = right_type;
                            bool last_was_pointer = false;
                            while (true) {
                                if (r_seek.tag == STATIC_ARRAY) {
                                    if (l_seek.tag != STATIC_ARRAY) {
                                        mismatch = true;
                                        break;
                                    }
                                    r_seek = *r_seek.array.of;
                                    l_seek = *l_seek.array.of;
                                    last_was_pointer = false;
                                } else if (r_seek.tag == POINTER) {
                                    if (l_seek.tag != POINTER) {
                                        mismatch = true;
                                        break;
                                    }
                                    r_seek = *r_seek.pointer.to;
                                    l_seek  = *l_seek.pointer.to;
                                    last_was_pointer = true;
                                } else {
                                    if (l_seek.tag == r_seek.tag) {
                                        if (is_number_type(l_seek.tag)) {
                                            assert(l_seek.number.is_solid && r_seek.number.is_solid);
                                            if (l_seek.number.size != r_seek.number.size) {
                                                mismatch = true;
                                            } else if (l_seek.tag == INT && l_seek.number.is_signed != r_seek.number.is_signed) {
                                                mismatch = true;
                                            }
                                        }
                                    } else {
                                        mismatch = true;
                                        if (last_was_pointer) {
                                            if (l_seek.tag == VOID) {
                                                if (l_seek.void_is_null) {
                                                    mismatch = false;
                                                }
                                            } else if (r_seek.tag == VOID) {
                                                if (r_seek.void_is_null) {
                                                    mismatch = false;
                                                }
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                            if (mismatch) {
                                report_error(binary, "Left and right sides of '%S' doesn't have matching types. Left: %S, Right: %S", to_string(tag), tprint_type(left_type), tprint_type(right_type));
                                return;
                            }
                            binary->data_type.tag = INT;
                            binary->data_type.number.is_solid  = true;
                            binary->data_type.number.size      = 8;
                            binary->data_type.number.is_signed = true;
                            return;
                        } else if (left_type.tag == POINTER) {
                            if (right_type.tag != INT) {
                                report_error(binary, "A pointer type on the left side of '-' needs an integer or pointer on the right. Left: %S, Right: %S", tprint_type(left_type), tprint_type(right_type));
                                return;
                            }
                            binary->data_type = left_type;
                            return;
                        } else { assert(right_type.tag == POINTER);
                            report_error(binary, "A pointer type on the right side of '-' needs a pointer type on the left. Left: %S, Right: %S", tprint_type(left_type), tprint_type(right_type));
                            return;
                        }
                    } else if (tag == IS_EQUAL || tag == NOT_EQUAL) {
                        bool mismatch = false;
                        if (left_type.tag == POINTER && right_type.tag == POINTER) {
                            // @Copypaste: :PointerChecking
                            auto l_seek  = left_type;
                            auto r_seek = right_type;
                            bool last_was_pointer = false;
                            while (true) {
                                if (r_seek.tag == STATIC_ARRAY) {
                                    if (l_seek.tag != STATIC_ARRAY) {
                                        mismatch = true;
                                        break;
                                    }
                                    r_seek = *r_seek.array.of;
                                    l_seek = *l_seek.array.of;
                                    last_was_pointer = false;
                                } else if (r_seek.tag == POINTER) {
                                    if (l_seek.tag != POINTER) {
                                        mismatch = true;
                                        break;
                                    }
                                    r_seek = *r_seek.pointer.to;
                                    l_seek  = *l_seek.pointer.to;
                                    last_was_pointer = true;
                                } else {
                                    if (l_seek.tag == r_seek.tag) {
                                        if (is_number_type(l_seek.tag)) {
                                            assert(l_seek.number.is_solid && r_seek.number.is_solid);
                                            if (l_seek.number.size != r_seek.number.size) {
                                                mismatch = true;
                                            } else if (l_seek.tag == INT && l_seek.number.is_signed != r_seek.number.is_signed) {
                                                mismatch = true;
                                            }
                                        }
                                    } else {
                                        mismatch = true;
                                        if (last_was_pointer) {
                                            if (l_seek.tag == VOID) {
                                                if (l_seek.void_is_null) {
                                                    mismatch = false;
                                                }
                                            } else if (r_seek.tag == VOID) {
                                                if (r_seek.void_is_null) {
                                                    mismatch = false;
                                                }
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                            if (!mismatch) {
                                binary->data_type.tag = BOOL;
                                return;
                            }
                        } else {
                            mismatch = true;
                        }
                        if (mismatch) {
                            report_error(binary, "Left and right sides of '%S' doesn't have matching types. Left: %S, Right: %S", to_string(tag), tprint_type(left_type), tprint_type(right_type));
                            return;
                        }
                    }
                }
                if (!is_number_type(left_type.tag)) {
                    report_error(left, "Left expression isn't a valid type for '%S': %S.", to_string(tag), tprint_type(left_type));
                    return;
                }
                if (!is_number_type(right_type.tag)) {
                    report_error(right, "Right expression isn't a valid type for '%S': %S.", to_string(tag), tprint_type(right_type));
                    return;
                }
            }
            
            bool use_right_type = false;
            using enum Typecheck_Result;
            auto t = typecheck_number_types(left_type, right_type);
            if (t != ALL_GOOD) {
                if (t == SET_L_TO_R) {
                    if (right_type.number.is_solid) {
                        set_loose_number_type_expr_data_type_to_new_solid_type(left, right_type);
                    } else {
                        set_loose_number_type_expr_data_type_another_loose_type(left, right_type);
                    }
                    use_right_type = true;
                } else if (t == SET_R_TO_L) {
                    if (left_type.number.is_solid) {
                        set_loose_number_type_expr_data_type_to_new_solid_type(right, left_type);
                    } else {
                        set_loose_number_type_expr_data_type_another_loose_type(right, left_type);
                    }
                } else {
                    if (t == TYPE_MISMATCH) {
                        report_error(binary, "Left and right sides of '%S' doesn't have matching types. Left: %S, Right: %S", to_string(tag), tprint_type(left_type), tprint_type(right_type));
                    } else if (t == SIZE_MISMATCH) {
                        report_error(binary, "Left and right sides of '%S' doesn't have matching size. Left: %S (%d bytes), Right: %S (%d bytes)", to_string(tag), tprint_type(left_type), left_type.number.size, tprint_type(right_type), right_type.number.size);
                    } else if (t == LOOSE_R_TOO_BIG_FOR_L) {
                        report_error(binary, "Right side of '%S' (%d bytes) is too big for the left side (%d bytes).", to_string(tag), right_type.number.size, left_type.number.size);
                    } else { assert(t == LOOSE_L_TOO_BIG_FOR_R);
                        report_error(binary, "Left side of '%S' (%d bytes) is too big for the right side (%d bytes).", to_string(tag), left_type.number.size, right_type.number.size);
                    }
                    return;
                }
            }
            
            if (binary_is_bool_type) {
                binary->data_type.tag = BOOL;
            } else {
                binary->data_type = use_right_type ? right_type : left_type;
            }
        }
        //InvalidDefaultCase("Unhandled binary type");
    }
    return;
}

internal void typecheck_decl(Ast_Decl *decl);
internal void typecheck_statement(Ast *ast);
internal void typecheck_subexpr(Ast_Subexpr *subexpr);

internal bool
is_const_subexpr(Ast_Subexpr *expr) {
    if (expr->ast_tag == AST_BINARY) {
        auto binary = cast(Ast_Binary *, expr);
        using enum Binary_Tag;
        if (binary->tag == ARRAY_ACCESS) {
            return false; // @Incomplete: const arrays?
        } else if (binary->tag == DOT) {
            return false; // @Incomplete: const/compile-time evaluatable structs/struct members
        } else {
            bool left_is_constexpr  = is_const_subexpr(binary->left);
            bool right_is_constexpr = is_const_subexpr(binary->right);
            return left_is_constexpr && right_is_constexpr;
        }
    } else if (expr->ast_tag == AST_UNARY) {
        auto unary = cast(Ast_Unary *, expr);
        using enum Unary_Tag;
        if (unary->tag == POINTER_TO) {
            return false;
        } else if (unary->tag == DEREFERENCE) {
            return false;
        } else {
            bool right_is_constexpr = is_const_subexpr(unary->right);
            return right_is_constexpr;
        }
    } else if (expr->ast_tag == AST_LITERAL) {
        return true;
    } else if (expr->ast_tag == AST_IDENT) {
        auto ident = cast(Ast_Ident *, expr);
        assert(ident->flags == chai.variables[ident->variable_index].ident->flags);
        return ident->flags & Ast_Ident::CONSTANT;
    } else if (expr->ast_tag == AST_SIZEOF) {
        return true;
    }
    
    return false;
}

internal bool
is_constexpr(Ast_Expr *expr) {
    if (expr->ast_tag == AST_BINARY) {
        auto binary = cast(Ast_Binary *, expr);
        using enum Binary_Tag;
        if (binary->tag == ARRAY_ACCESS) {
            return false; // @Incomplete: const arrays?
        } else if (binary->tag == DOT) {
            return false; // @Incomplete: const/compile-time evaluatable structs/struct members
        } else {
            bool left_is_constexpr  = is_constexpr(binary->left);
            bool right_is_constexpr = is_constexpr(binary->right);
            return left_is_constexpr && right_is_constexpr;
        }
    } else if (expr->ast_tag == AST_UNARY) {
        auto unary = cast(Ast_Unary *, expr);
        using enum Unary_Tag;
        if (unary->tag == POINTER_TO) {
            return false;
        } else if (unary->tag == DEREFERENCE) {
            return false;
        } else {
            bool right_is_constexpr = is_constexpr(unary->right);
            return right_is_constexpr;
        }
    } else if (expr->ast_tag == AST_IDENT) {
        auto ident = cast(Ast_Ident *, expr);
        assert(ident->flags == chai.variables[ident->variable_index].ident->flags);
        return ident->flags & Ast_Ident::CONSTANT;
    } else if (expr->ast_tag == AST_LITERAL) {
        return true;
    } else if (expr->ast_tag == AST_SIZEOF) {
        return true;
    } else if (expr->ast_tag == AST_PROC_DEFN) {
        return true;
    }
    
    return false;
}


struct Find_Member_Result {
    Ast_Struct_Member_Node *end_member;
    Dot_Chain_Node         *dot_chain_of_usings_that_lead_to_member;
};

internal Find_Member_Result
find_matching_member(Data_Type::Struct structure, Str name) {
    assert(structure.first_member);
    auto member = structure.first_member;
    while (member) {
        auto using_kind = member->decl.using_kind;
        if (using_kind != Ast_Decl::ANON_USING) {
            if (member->decl.ident.name == name) {
                assert(!(using_kind == Ast_Decl::NORMAL_USING) || !find_matching_member(member->decl.ident.data_type.structure, name).end_member);
                Find_Member_Result result = {};
                result.end_member = member;
                return result;
            }
        } 
        if (using_kind != Ast_Decl::NO_USING) {
            assert(member->decl.ident.data_type.tag == Data_Type_Tag::STRUCT);
            
            auto using_struct = member->decl.ident.data_type.structure;
            
            auto result = find_matching_member(using_struct, name);
            if (result.end_member) {
                auto node  = NewAst(Dot_Chain_Node);
                node->decl = &member->decl;
                node->next = result.dot_chain_of_usings_that_lead_to_member;
                result.dot_chain_of_usings_that_lead_to_member = node;
                return result;
            }
        }
        member = member->next;
    }
    return {};
}

internal bool // bool name_conflict_found = 
check_for_name_conflict_in_struct(Data_Type::Struct _struct, Ast_Ident *ident_to_check) {
    assert(_struct.first_member);
    auto it = _struct.first_member;
    while (it) {
        auto using_kind = it->decl.using_kind;
        
        if (using_kind != Ast_Decl::ANON_USING) {
            
            if (&it->decl.ident != ident_to_check) {
                if (it->decl.ident.name == ident_to_check->name) {
                    report_error(ident_to_check, "struct member has a name conflict.");
                    report_info(&it->decl.ident, "... here's the conflicting member.");
                    return true;
                }
            }
        }
        
        if (using_kind != Ast_Decl::NO_USING) {
            assert(it->decl.ident.data_type.tag == Data_Type_Tag::STRUCT);
            
            auto nested_struct = it->decl.ident.data_type.structure;
            
            auto name_conflict_found = check_for_name_conflict_in_struct(nested_struct, ident_to_check);
            if (name_conflict_found) {
                if (using_kind == Ast_Decl::NORMAL_USING) {
                    report_info(&it->decl.ident, "... in this using.");
                }
                return true;
            }
        }
        it = it->next;
    }
    return false;
}

internal bool // bool name_conflict_found = 
check_for_name_conflict_in_using(Data_Type::Struct using_struct, Data_Type::Struct base_struct) {
    assert(using_struct.first_member && base_struct.first_member);
    auto using_member = using_struct.first_member;
    while (using_member) {
        auto using_kind = using_member->decl.using_kind;
        if (using_kind != Ast_Decl::ANON_USING) {
            auto newly_introduced_name = using_member->decl.ident.name;
            auto find_result = find_matching_member(base_struct, newly_introduced_name);
            if (find_result.end_member) {
                report_error(&find_result.end_member->decl.ident, "struct member has a name conflict.");
                report_info(&using_member->decl.ident, "... here's the conflicting member.");
                return true;
            }
        }
        if (using_kind != Ast_Decl::NO_USING) {
            assert(using_member->decl.ident.data_type.tag == Data_Type_Tag::STRUCT);
            
            auto nested_using_struct = using_member->decl.ident.data_type.structure;
            
            auto name_conflict_found = check_for_name_conflict_in_using(nested_using_struct, base_struct);
            if (name_conflict_found) {
                if (using_kind == Ast_Decl::NORMAL_USING) {
                    report_info(&using_member->decl.ident, "... in this using.");
                }
                return true;
            }
        }
        using_member = using_member->next;
    }
    return false;
}

internal Data_Type
typecheck_type_expr(Ast_Type_Expr *type_expr) {
    Data_Type result = {};
    Data_Type *r = &result;
    auto type = type_expr->type;
    
    while (true) {
        if (type.tag == Data_Type_Tag::STATIC_ARRAY) {
            typecheck_subexpr(type.array.size_expr);
            RetIfError({});
            
            auto expr_type = get_data_type(type.array.size_expr);
            if (expr_type.tag != Data_Type_Tag::INT) {
                report_error(type.array.size_expr, "Expected integer type for a array size expression, but found type %S.", tprint_type(expr_type));
                return {};
            }
            if (!is_constexpr(type.array.size_expr)) {
                report_error(type.array.size_expr, "Array size expression be constant.");
                return {};
            }
            
            r->tag = Data_Type_Tag::STATIC_ARRAY;
            // @Incomplete: r->array.size = evaluate_constexpr(type.array.size_expr);
            // RetIfError();
            r->array.size_expr = type.array.size_expr;
            r->array.of = AllocDataTypeUninitialized();
            r = r->array.of;
            *r = {};
            
            type = *type.array.of;
        } else if (type.tag == Data_Type_Tag::POINTER) {
            r->tag = Data_Type_Tag::POINTER;
            r->pointer.to = AllocDataTypeUninitialized();
            r = r->pointer.to;
            *r = {};
            type = *type.pointer.to;
        } else {
            break;
        }
    }
    
    if (type.tag == Data_Type_Tag::UNKNOWN) {
        if (type.user_type_ident) {
            auto ident = type.user_type_ident;
            auto found_index = find_variable_index(ident->name);
            if (found_index < 0) {
                report_error(ident, "Undeclared identifier '%S'.", ident->name);
                return {};
            }
            auto variable_data_type = chai.variables[found_index].ident->data_type;
            
            if (variable_data_type.tag != Data_Type_Tag::USER_TYPE) {
                report_error(ident, "Expected a type identifer, but found a variable of type %S", tprint_type(variable_data_type));
                return {};
            }
            assert(!variable_data_type.user_type_ident);
            type = variable_data_type.user_type.type_expr->type;
            type.user_type_ident = ident;
            assert(type.tag != Data_Type_Tag::VOID);
            
            ident->variable_index = found_index;
            ident->flags = chai.variables[found_index].ident->flags;
            
            *r = variable_data_type.user_type.type_expr->type;
            r->user_type_ident = ident;
        } else {
            report_error(type_expr, "Internal Compiler Error: Unhandled unknown type tag in " __FUNCTION__ ".");
            return {};
        }
    } else if (type.tag == Data_Type_Tag::STRUCT) {
        r->tag = Data_Type_Tag::STRUCT;
        r->structure.struct_union_enum = type.structure.struct_union_enum;
        r->structure.first_member      = type.structure.first_member; // @Temporary
        
        // @Hack: Just for now so struct members have a place to live
        auto push_index = add_push_scope_variable();
        
        auto member = r->structure.first_member;
        while (member) {
            if (member->decl.using_kind == Ast_Decl::ANON_USING) {
                member->decl._type_expr = NewAst(Ast_Type_Expr);
                auto member_type = typecheck_type_expr(member->decl._type_expr);
                RetIfError({});
                
                assert(member_type.tag == Data_Type_Tag::STRUCT);
                auto using_struct = member_type.structure;
                bool name_conflict_found = check_for_name_conflict_in_using(using_struct, r->structure);
                assert(name_conflict_found == chai.error_was_reported);
                if (name_conflict_found) {
                    return {};
                }
                // @Hack: This is horrible. Deal with anonymous structs properly.
                member->decl.ident.data_type = member_type;
            } else {
                typecheck_decl(&member->decl);
                RetIfError({});
                
                auto member_type = member->decl.ident.data_type;
                
                if (member->decl.using_kind == Ast_Decl::NORMAL_USING) {
                    if (member->decl.ident.data_type.tag != Data_Type_Tag::STRUCT) {
                        report_error(&member->decl.ident, "'using' can only be used if the type of the declaration is a struct/union. Found type %S.", tprint_type(member->decl.ident.data_type));
                        return {};
                    }
                }
                
                if (member->decl.using_kind != Ast_Decl::NO_USING) {
                    assert(member->decl.using_kind == Ast_Decl::NORMAL_USING);
                    assert(member->decl.ident.data_type.tag == Data_Type_Tag::STRUCT);
                    
                    auto using_struct = member->decl.ident.data_type.structure;
                    auto newly_added_base_struct_using_member = &member;
                    bool name_conflict_found = check_for_name_conflict_in_using(using_struct, r->structure);
                    if (name_conflict_found) {
                        report_info(&member->decl.ident, "... while checking in this using.");
                        return {};
                    }
                } else {
                    auto ident_to_check = &member->decl.ident;
                    bool name_conflict_found = check_for_name_conflict_in_struct(r->structure, ident_to_check);
                    assert(name_conflict_found == chai.error_was_reported);
                    if (name_conflict_found) {
                        return {};
                    }
                }
            }
            
            member = member->next;
        }
        add_pop_scope_variable(push_index);
    } else if (type.tag == Data_Type_Tag::PROC) {
        r->tag = Data_Type_Tag::PROC;
        r->proc.first_param            = type.proc.first_param;
        r->proc.first_return_type_expr = type.proc.first_return_type_expr;
        
        auto param = r->proc.first_param;
        while (param) {
            auto found_index = find_variable_index(param->decl.ident.name);
            if (found_index >= 0) {
                report_error(&param->decl.ident, "Parameter name is conflicting with an identifier in an outer scope. (@Incomplete: doesn't really check for globals only yet)");
                report_info(chai.variables[found_index].ident, "... here's the outer scope identifier.");
                return {};
            }
            
            typecheck_decl(&param->decl);
            RetIfError({});
            
            param = param->next;
        }
        
        auto return_type_expr = r->proc.first_return_type_expr;
        while (return_type_expr) {
            return_type_expr->type = typecheck_type_expr(return_type_expr);
            RetIfError({});
            
            return_type_expr = return_type_expr->next;
        }
    } else if (type.tag == Data_Type_Tag::TYPEOF) {
        typecheck_subexpr(type.typeof.expr);
        RetIfError({});
        
        *r = get_data_type(type.typeof.expr);
        
        if (is_number_type(r->tag)) {
            if (!r->number.is_solid) {
                solidify_number_expr_data_type_to_default(type.typeof.expr, *r);
                *r = get_data_type(type.typeof.expr);
            }
        }
    } else if (type.tag == Data_Type_Tag::INT) {
        *r = type;
    } else if (type.tag == Data_Type_Tag::FLOAT) {
        *r = type;
    } else if (type.tag == Data_Type_Tag::BOOL) {
        *r = type;
    } else if (type.tag == Data_Type_Tag::STRING) {
        *r = type;
    } else if (type.tag == Data_Type_Tag::VOID) {
        *r = type;
    } else {
        report_error(type_expr, "Internal Compiler Error: Unhandled type tag in " __FUNCTION__ ".");
        return {};
    }
    return result;
}

internal void
typecheck_and_maybe_solidify_unary(Ast_Unary *unary) {
    using enum Unary_Tag;
    auto right_type = get_data_type(unary->right);
    switch (unary->tag) {
        using enum Data_Type_Tag;
        Case POINTER_TO: {
            if (!is_lvalue(unary->right)) {
                report_error(unary->right, "Expected an lvalue on the right side of '*'."); // @Error
                return;
            }
            if (is_number_type(right_type.tag) && !right_type.number.is_solid) {
                report_error(unary->right, "Internal compiler error: Parsing a pointer to an unsolidified type.");
                return;
            }
            // @Cleanup: Why can't this .to just point to the data_type of unary->right?
            // if ofc we change get_data_type to return a pointer or make get_data_type_pointer()
            unary->data_type.tag = POINTER;
            unary->data_type.pointer.to = AllocDataTypeUninitialized();
            *unary->data_type.pointer.to = right_type;
        }
        Case MINUS: {
            if (right_type.tag == INT) {
                if (!right_type.number.is_signed) {
                    if (right_type.number.is_solid) {
                        report_error(unary->right, "You can't use unary minus '-' on an unsigned int expression.");
                        return;
                    } else {
                        right_type.number.is_signed = true;
                        set_loose_number_type_expr_data_type_another_loose_type(unary->right, right_type);
                    }
                }
                
            } else if (right_type.tag != FLOAT) {
                report_error(unary->right, "You can only use the unary minus operator '-' on int or float type expressions.");
                return;
            }
            unary->data_type = right_type;
        }
        Case LOGICAL_NOT: {
            if (right_type.tag != BOOL) {
                // @Incomplete: !pointer and !int should become pointer == null and int == 0 etc.
                report_error(unary->right, "You can only use the logical NOT operator '!' on bool type expressions.");
                return;
            }
            unary->data_type = right_type;
        }
        Case BITWISE_NOT: {
            if (right_type.tag != INT) {
                report_error(unary->right, "You can only use the bitwise NOT operator '~' on integer type expressions.");
                return;
            }
            // :Solidify ?
            unary->data_type = right_type;
        }
        Case DEREFERENCE: {
            if (right_type.tag != POINTER) {
                report_error(unary, "Tried to dereference a non-pointer: %S.", tprint_type(right_type));
                return;
            }
            if (right_type.pointer.to->tag == VOID) {
                report_error(unary, "You can't dereference a void pointer directly, you need to cast is first.");
                return;
            }
            if (right_type.pointer.to->tag == PROC) {
                report_error(unary, "You can't dereference a procedure pointer directly.");
                return;
            }
            unary->data_type = *right_type.pointer.to;
        }
        Case CAST: {
            auto cast = cast(Ast_Cast *, unary);
            auto cast_type = typecheck_type_expr(&cast->type_expr);
            RetIfError();
            assert(!is_number_type(cast_type.tag) || cast_type.number.is_solid);
            
            // typecheck_assign is the implicit conversion to some extent
            auto typecheck_result = typecheck_assign(cast_type, right_type);
            using enum Typecheck_Result;
            if (typecheck_result != ALL_GOOD) {
                if (is_number_type(cast_type.tag) && is_number_type(right_type.tag)) {
                    if (typecheck_result == SET_R_TO_L) {
                        assert(!right_type.number.is_solid);
                        set_loose_number_type_expr_data_type_to_new_solid_type(unary->right, cast_type);
                    } else if (typecheck_result == TYPE_MISMATCH) {
                        // this is fine cause it's a cast.
                    } else if (typecheck_result == SIZE_MISMATCH || typecheck_result == LOOSE_R_TOO_BIG_FOR_L) {
                        // this is fine cause it's a cast.
                        // @Incomplete: use this if we want truc checks later
                    } else ShouldntReachHere;
                } else if (cast_type.tag == POINTER && right_type.tag == INT) {
                    if (right_type.number.is_signed) {
                        report_error(unary, "Can't cast a signed integer to a pointer.");
                        return;
                    }
                    // all good
                } else if (cast_type.tag == INT && right_type.tag == POINTER) {
                    if (cast_type.number.is_signed || (cast_type.number.is_solid && cast_type.number.size != 8)) {
                        report_error(unary, "u64 is the only integer type you can cast a pointer to.");
                        return;
                    }
                    // all good
                } else if (cast_type.tag == POINTER && right_type.tag == STATIC_ARRAY) {
                    // all good
                } else if (cast_type.tag == POINTER && right_type.tag == POINTER) {
                    // all good
                } else if (cast_type.tag == INT && right_type.tag == BOOL) {
                    // all good
                } else if (cast_type.tag == BOOL && right_type.tag == INT) {
                    // all good
                    // this is basically just the same as adding '!= 0' at the end
                } else {
                    if (cast_type.tag == PROC) {
                        report_error(unary, "You can't cast anything to a procedure.");
                    } else if (right_type.tag == PROC) {
                        report_error(unary, "You can't cast a procedure to anything.");
                    } else {
                        report_error(unary, "Can't cast %S to %S.", tprint_type(right_type), tprint_type(cast_type));
                    }
                    return;
                }
            }
            unary->data_type = cast_type;
        }
        InvalidDefaultCase("Unhandled unary type");
    }
    
}

internal void
typecheck_binary(Ast_Binary *binary) {
    using enum Binary_Tag;
    
    typecheck_subexpr(binary->left);
    RetIfError();
    
    if (binary->tag != DOT) {
        typecheck_subexpr(binary->right);
        RetIfError();
        
        typecheck_and_maybe_solidify_binary(binary);
        RetIfError();
    } else {
        auto left_type = get_data_type(binary->left);
        
        bool left_is_pointer = left_type.tag == Data_Type_Tag::POINTER;
        if (left_is_pointer) left_type = *left_type.pointer.to;
        
        bool left_is_struct       = left_type.tag == Data_Type_Tag::STRUCT;
        bool left_is_string       = left_type.tag == Data_Type_Tag::STRING;
        bool left_is_static_array = left_type.tag == Data_Type_Tag::STATIC_ARRAY;
        if (!left_is_struct && !left_is_string && !left_is_static_array) {
            report_error(binary->left, "Left side of '.' doesn't have members. It's of type %S.", tprint_type(left_type));
            return;
        }
        if (!is_lvalue(binary->left)) {
            report_error(binary->left, "Expected an lvalue on the left side of '.'."); // @Error print_nonlvalueness
            return;
        }
        
        // @Incomplete: Need to be able to handle 'using' a Str/static array in a struct
        assert(binary->right->ast_tag == AST_IDENT);
        auto ident = cast(Ast_Ident *, binary->right);
        if (left_is_struct) {
            auto find_result = find_matching_member(left_type.structure, ident->name);
            if (!find_result.end_member) {
                report_error(ident, "'%S' isn't a member of '%S'.", ident->name, tprint_type(left_type));
                if (left_type.tag == Data_Type_Tag::POINTER) left_type = *left_type.pointer.to;
                assert(left_type.user_type_ident);
                auto type_ident = chai.variables[left_type.user_type_ident->variable_index].ident;
                report_info(type_ident, "... here's the struct definition");
                return;
            }
            
            //ident->variable_index should be default
            ident->flags      = find_result.end_member->decl.ident.flags;
            binary->data_type = find_result.end_member->decl.ident.data_type;
            binary->dot_chain_of_usings_that_lead_to_member = find_result.dot_chain_of_usings_that_lead_to_member;
        } else if (left_is_string) {
            if (ident->name == sl("count")) {
                binary->data_type = chai.string_count_type;
            } else if (ident->name == sl("data")) {
                binary->data_type = chai.string_data_type;
            } else {
                report_error(ident, "'%S' isn't a member of 'string'.", ident->name);
                return;
            }
        } else { assert(left_is_static_array);
            if (ident->name == sl("count")) {
                binary->data_type = chai.array_count_type;
            } else {
                report_error(ident, "'%S' isn't a member of a static array.", ident->name);
                return;
            }
        }
    }
}

struct Hexlit_Result {
    u64 value;
    s32 digit_count;
    s32 significant_digit_count;
};

internal Hexlit_Result
string_to_hexlit(Str text) {
    assert(starts_with(text, sl("0x")) || starts_with(text, sl("0h")));
    Hexlit_Result result = {};
    
    s32 starting_index = 2; // Skip 0x/0h
    while (starting_index < text.count) {
        auto c = text.data[starting_index];
        if (c != '_' || c != '0') break;
        if (c == '0') result.digit_count += 1;
        starting_index += 1;
    }
    
    ForI (i, starting_index, text.count) {
        auto c = text.data[i];
        if (c == '_') continue;
        
        result.significant_digit_count += 1;
        result.value <<= 4;
        if (is_numeric(c)) {
            result.value |= (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            result.value |= (c - 'a') + 10;
        } else if (c >= 'A' && c <= 'F') {
            result.value |= (c - 'A') + 10;
        } else assert(!"Found a non-hex character");
    }
    result.digit_count += result.significant_digit_count;
    return result;
}

internal Str
string_literal_to_real_string(Arena *arena, Str s) {
    s = advance(s, 1); // trim the surrounding double quotes
    s.count -= 1;
    
    String_Builder sb = {};
    defer { sb.resize(0); };
    sb.resize(s.count / 2);
    
    auto window = Str{ .count = 0, .data = s.data };
    
    ForI (read, 0, s.count) {
        if (s.data[read] == '\\') {
            read += 1;
            assert(read < s.count);
            
            char c = -1;
            switch (s.data[read]) {
                Case '\\': c = '\\';
                Case '\"': c = '\"';
                Case 'n':  c = '\n';
                Case 'r':  c = '\r';
                Case 'e':  c = '\x1b';
                Case '0':  c = '\0';
                InvalidDefaultCase("Unhandled escape character");
            }
            
            s64 write = sb.count;
            sb.add_new_elements(window.count + 1);
            
            ForI (i, 0, window.count) sb.data[write + i] = window.data[i];
            
            sb.data[write + window.count] = c;
            
            window.count += 2; // Skip backslash and escape character
            advance(&window, window.count);
        } else {
            window.count += 1;
        }
    }
    
    s64 write = sb.count;
    sb.add_new_elements(window.count);
    
    ForI (i, 0, window.count) sb.data[write + i] = window.data[i];
    
    Str real_string;
    real_string.count = sb.count;
    real_string.data  = arena_alloc_array(arena, char, sb.count + 1);
    
    ForI (i, 0, sb.count) real_string.data[i] = sb.data[i];
    real_string.data[sb.count] = 0;
    
    return real_string;
}

internal void
typecheck_subexpr(Ast_Subexpr *subexpr) {
    switch (subexpr->ast_tag) {
        Case AST_LITERAL: {
            auto literal = cast(Ast_Literal *, subexpr);
            
            if (literal->tag == Ast_Literal::NULL) {
                literal->data_type.tag = Data_Type_Tag::POINTER;
                literal->data_type.pointer.to = AllocDataTypeUninitialized();
                *literal->data_type.pointer.to = {};
                literal->data_type.pointer.to->tag = Data_Type_Tag::VOID;
                literal->data_type.pointer.to->void_is_null = true;
            } else if (literal->tag == Ast_Literal::TRUE) {
                literal->data_type.tag = Data_Type_Tag::BOOL;
                literal->_bool = true; // @Incomplete: do we want this?
            } else if (literal->tag == Ast_Literal::FALSE) {
                literal->data_type.tag = Data_Type_Tag::BOOL;
                literal->_bool = false; // @Incomplete: do we want this?
            } else if (literal->tag == Ast_Literal::NUMBER) {
                if (literal->is_char) {
                    Scoped_Arena scratch;
                    Str char_string = string_literal_to_real_string(scratch, literal->text);
                    
                    if (char_string.count != 1) {
                        report_error(literal, "String after #char directive contain exactly one ascii character. Contains %d.", char_string.count);
                        return;
                    }
                    
                    literal->_u32 = char_string.data[0];
                    literal->data_type.tag = Data_Type_Tag::INT;
                    literal->data_type.number.is_solid  = true;
                    literal->data_type.number.is_signed = false;
                    literal->data_type.number.size      = 1;
                    literal->is_char = true;
                    
                } else if (starts_with(literal->text, sl("0x"))) {
                    auto hexlit = string_to_hexlit(literal->text);
                    if (hexlit.significant_digit_count > (64/4)) {
                        report_error(literal, "A hex literal can have a maxiumum of 16 significant digits. Found %d significant digits.", hexlit.significant_digit_count);
                        return;
                    }
                    
                    literal->_u64 = hexlit.value;
                    literal->data_type.tag = Data_Type_Tag::INT;
                    literal->data_type.number.is_signed = false;
                    literal->data_type.number.is_solid  = false;
                    
                    if (hexlit.value > U32_MAX) {
                        literal->data_type.number.size = 8;
                    } else if (hexlit.value > U16_MAX) {
                        literal->data_type.number.size = 4;
                    } else if (hexlit.value > U8_MAX) {
                        literal->data_type.number.size = 2;
                    } else {
                        literal->data_type.number.size = 1;
                    }
                } else if (starts_with(literal->text, sl("0h"))) {
                    auto hexlit = string_to_hexlit(literal->text);
                    
                    literal->data_type.tag = Data_Type_Tag::FLOAT;
                    literal->data_type.number.is_solid = true;
                    
                    if (hexlit.digit_count == (32/4)) {
                        assert((hexlit.value & 0xffffFFFF00000000) == 0);
                        literal->data_type.number.size = 4;
                        literal->_f32 = *(f32 *)&hexlit.value;
                    } else if (hexlit.digit_count == (64/4)) {
                        literal->data_type.number.size = 8;
                        literal->_f64 = *(f64 *)&hexlit.value;
                    } else {
                        report_error(literal, "A hexfloat literal must have either 8 or 16 digits. Found %d digits.", hexlit.digit_count);
                        return;
                    }
                } else {
                    bool dot_found = false;
                    ForI (i, 0, literal->text.count) {
                        if (!is_numeric(literal->text.data[i])) {
                            assert(literal->text.data[i] == '.');
                            dot_found = true;
                            break;
                        }
                    }
                    literal->data_type.number.is_solid = false;
                    if (dot_found) {
                        literal->data_type.tag = Data_Type_Tag::FLOAT;
                    } else {
                        literal->data_type.tag = Data_Type_Tag::INT;
                    }
                }
            } else if (literal->tag == Ast_Literal::STRING) {
                literal->s = string_literal_to_real_string(&chai.string_arena, literal->text);
                literal->data_type.tag = Data_Type_Tag::STRING;
            }
        }
        
        Case AST_IDENT: {
            auto ident = cast(Ast_Ident *, subexpr);
            
            auto found_index = find_variable_index(ident->name);
            if (found_index < 0) {
                report_error(ident, "Undeclared identifier '%S'.", ident->name);
                return;
            }
            
            auto data_type = chai.variables[found_index].ident->data_type;
            if (data_type.tag == Data_Type_Tag::USER_TYPE) {
                report_error(ident, "Unexpected user type in an expression.");
                return;
            }
            
            ident->flags = chai.variables[found_index].ident->flags;
            ident->variable_index = found_index;
        }
        
        Case AST_CALL: {
            auto call = cast(Ast_Call *, subexpr);
            
            auto found_index = find_variable_index(call->name);
            if (found_index < 0) {
                report_error(call, "Undeclared identifier '%S'.", call->name);
                return;
            }
            
            auto data_type = chai.variables[found_index].ident->data_type;
            if (data_type.tag != Data_Type_Tag::PROC) {
                report_error(call, "Tried to call a non-procedure identifier.");
                report_info(chai.variables[found_index].ident, "... here's the declaration.");
                return;
            }
            
            while (true) {
                bool call_args_match = true;
                auto param = data_type.proc.first_param;
                auto arg = call->first_arg;
                while (true) {
                    if (param && arg) {
                    } else if (!param && !arg) {
                        break;
                    } else {
                        call_args_match = false;
                        break;
                    }
                    
                    typecheck_subexpr(arg->expr);
                    RetIfError();
                    
                    auto param_type = param->decl.ident.data_type;
                    auto   arg_type = get_data_type(arg->expr);
                    
                    auto typecheck_result = typecheck_assign(param_type, arg_type);
                    using enum Typecheck_Result;
                    if (typecheck_result != ALL_GOOD) {
                        if (typecheck_result == SET_R_TO_L) {
                        } else if (typecheck_result == TYPE_MISMATCH && param_type.tag == Data_Type_Tag::BOOL && arg_type.tag == Data_Type_Tag::POINTER) {
                            
                            auto _null = NewAst(Ast_Literal);
                            _null->tag = Ast_Literal::NULL;
                            _null->loc = arg->expr->loc;
                            
                            _null->data_type.tag = Data_Type_Tag::POINTER;
                            _null->data_type.pointer.to = AllocDataTypeUninitialized();
                            *_null->data_type.pointer.to = {};
                            _null->data_type.pointer.to->tag = Data_Type_Tag::VOID;
                            _null->data_type.pointer.to->void_is_null = true;
                            
                            auto binary = NewAst(Ast_Binary);
                            binary->tag = Binary_Tag::NOT_EQUAL;
                            binary->data_type.tag = Data_Type_Tag::BOOL;
                            binary->left = arg->expr;
                            binary->right = _null;
                            
                            arg->expr = binary;
                        } else {
                            assert(typecheck_result == TYPE_MISMATCH || typecheck_result == SIZE_MISMATCH || typecheck_result == LOOSE_R_TOO_BIG_FOR_L);
                            
                            call_args_match = false;
                            break;
                        }
                    }
                    
                    arg = arg->next;
                    param = param->next;
                }
                
                if (call_args_match) {
                    call->flags = chai.variables[found_index].ident->flags;
                    call->variable_index = found_index;
                    break;
                }
                
                found_index = find_variable_index(call->name, found_index-1);
                if (found_index < 0) {
                    report_error(call, "Call arguments don't match the parameters of any version of '%S'.", call->name);
                    
                    Str arg_types = {};
                    if (call->first_arg) {
                        arg = call->first_arg;
                        arg_types = tprint_type(get_data_type(arg->expr));
                        arg = arg->next;
                        while (arg) {
                            arg_types = tprint("%S, %S", arg_types, tprint_type(get_data_type(arg->expr)));
                            arg = arg->next;
                        }
                    }
                    
                    report_info_noloc("Argument types: (%S)", arg_types);
                    
                    found_index = find_variable_index(call->name);
                    while (found_index >= 0) {
                        report_info(chai.variables[found_index].ident, "... matched against this.");
                        found_index = find_variable_index(call->name, found_index-1);
                    }
                    return;
                }
                
                data_type = chai.variables[found_index].ident->data_type;
            }
        }
        
        Case AST_SIZEOF: {
            auto size_of = cast(Ast_Sizeof *, subexpr);
            
            size_of->type_expr.type = typecheck_type_expr(&size_of->type_expr);
            RetIfError();
            
            size_of->data_type.tag = Data_Type_Tag::INT;
            size_of->data_type.number.is_signed = false;
            size_of->data_type.number.is_solid = false;
            size_of->data_type.number.size = 0;
        }
        
        Case AST_UNARY: {
            auto unary = cast(Ast_Unary *, subexpr);
            
            typecheck_subexpr(unary->right);
            RetIfError();
            typecheck_and_maybe_solidify_unary(unary);
            //RetIfError();
        }
        
        Case AST_BINARY: {
            auto binary = cast(Ast_Binary *, subexpr);
            
            typecheck_binary(binary);
            //RetIfError();
        }
        
        Default: {
            report_error(subexpr, "Internal compiler error: Unhandled subexpression.");
        }
    }
}

internal void
typecheck_block(Ast_Block *block) {
    auto node = block->first_statement;
    while (node) {
        typecheck_statement(node->statement);
        RetIfError();
        node = node->next;
    }
}

internal Data_Type
typecheck_expr(Ast_Expr *expr) {
    switch (expr->ast_tag) {
        Case AST_TYPE_EXPR: {
            return typecheck_type_expr(cast(Ast_Type_Expr *, expr));
        }
        Case AST_PROC_DEFN: {
            auto proc_defn = cast(Ast_Proc_Defn *, expr);
            return typecheck_type_expr(&proc_defn->signature);
            // @Hack @Cleanup: ->body gets handled in typecheck_statement AST_DECL
        }
        
        Default: {
            auto subexpr = cast(Ast_Subexpr *, expr);
            
            typecheck_subexpr(subexpr);
            RetIfError({});
            return get_data_type(subexpr);
        }
    } // switch (token.tag)
}

internal void
typecheck_switch(Ast_Switch *_switch) {
    typecheck_subexpr(_switch->switch_expr);
    RetIfError();
    
    auto switch_type = get_data_type(_switch->switch_expr);
    
    if (switch_type.tag != Data_Type_Tag::INT) {
        report_error(_switch->switch_expr, "Expected integer type for a switch expression, but found type %S.", tprint_type(switch_type));
        return;
    }
    
    if (!switch_type.number.is_solid) {
        switch_type = solidify_number_expr_data_type_to_default(_switch->switch_expr, switch_type);
    }
    
    auto c = _switch->first_case;
    while (c) {
        bool is_default_case = c->expr == null;
        if (!is_default_case) {
            typecheck_subexpr(c->expr);
            RetIfError();
            
            auto case_type = get_data_type(c->expr);
            
            // @Copypaste: :CopySolidifyInt
            if (case_type.tag != Data_Type_Tag::INT) {
                report_error(c->expr, "Expected integer type for case expression, but found type %S.", tprint_type(case_type));
                return;
            }
            
            auto result = typecheck_number_types_one_way(switch_type, case_type);
            using enum Typecheck_Result;
            if (result != ALL_GOOD) {
                if (result == SET_R_TO_L) {
                    assert(!case_type.number.is_solid);
                    set_loose_number_type_expr_data_type_to_new_solid_type(c->expr, switch_type);
                } else {
                    if (result == TYPE_MISMATCH) {
                        ShouldntReachHere;
                    } else if (result == SIZE_MISMATCH) {
                        report_error(c->expr, "Case expression type size (%d bytes) doesn't match the type size of the switch (%d bytes)", case_type.number.size, switch_type.number.size);
                    } else if (result == LOOSE_R_TOO_BIG_FOR_L) {
                        report_error(c->expr, "Case expression type size (%d bytes) is too big for the type size of the switch (%d bytes)", case_type.number.size, switch_type.number.size);
                    } else ShouldntReachHere;
                    return;
                }
            }
        }
        
        if (c->first_statement) {
            auto push_index = add_push_scope_variable();
            auto node = c->first_statement;
            while (node) {
                typecheck_statement(node->statement);
                RetIfError();
                node = node->next;
            }
            add_pop_scope_variable(push_index);
        }
        c = c->next;
    }
}

internal void
typecheck_assignment(Ast_Assignment *assign) {
    auto tag   = assign->tag;
    auto left  = assign->left;
    auto right = assign->right;
    
    typecheck_subexpr(left);
    typecheck_subexpr(right);
    
    if (!is_lvalue(left)) {
        report_error(left, "The expression to the left of an assignment operator be an lvalue.");
        return;
    }
    
    auto left_type  = get_data_type(left);
    auto right_type = get_data_type(right);
    
    switch (tag) {
        using enum Data_Type_Tag;
        Case Assign_Tag::NORMAL: {
            auto typecheck_result = typecheck_assign(left_type, right_type);
            using enum Typecheck_Result;
            if (typecheck_result != ALL_GOOD) {
                if (typecheck_result == SET_R_TO_L) {
                    assert(!right_type.number.is_solid);
                    set_loose_number_type_expr_data_type_to_new_solid_type(right, left_type);
                } else {
                    if (typecheck_result == TYPE_MISMATCH) {
                        report_error(assign->op_loc, "Can't assign a value of type %S to a variable of type %S.", tprint_type(right_type), tprint_type(left_type));
                    } else if (typecheck_result == SIZE_MISMATCH) {
                        report_error(assign->op_loc, "Left and right sides of '%S' doesn't have matching size. Left: %S (%d bytes), Right: %S (%d bytes)", to_string(tag), tprint_type(left_type), left_type.number.size, tprint_type(right_type), right_type.number.size);
                    } else if (typecheck_result == LOOSE_R_TOO_BIG_FOR_L) {
                        report_error(assign->op_loc, "Right side of '%S' (%d bytes) is too big for the left side (%d bytes).", to_string(tag), right_type.number.size, left_type.number.size);
                    } else ShouldntReachHere;
                    return;
                }
            }
        }
        
        Case   Assign_Tag::ADD:
        OrCase Assign_Tag::SUB:
        OrCase Assign_Tag::MUL:
        OrCase Assign_Tag::DIV:{
            // @Copypaste: :CopySolidifyNumber
            if (!is_number_type(left_type.tag)) {
                report_error(left, "Expected integer or float type for left side of '%S', but found type %S.", to_string(tag), tprint_type(left_type));
            } else if (!is_number_type(right_type.tag)) {
                report_error(right, "Expected integer or float type for right side of '%S', but found type %S.", to_string(tag), tprint_type(right_type));
            } else {
                auto result = typecheck_number_types_one_way(left_type, right_type);
                using enum Typecheck_Result;
                if (result != ALL_GOOD) {
                    if (result == SET_R_TO_L) {
                        assert(!right_type.number.is_solid);
                        set_loose_number_type_expr_data_type_to_new_solid_type(right, left_type);
                    } else if (result == TYPE_MISMATCH) {
                        report_error(assign->op_loc, "Left and right sides of '%S' doesn't have matching types. Left: %S, Right: %S", to_string(tag), tprint_type(left_type), tprint_type(right_type));
                    } else if (result == SIZE_MISMATCH) {
                        report_error(assign->op_loc, "Left and right sides of '%S' doesn't have matching size. Left: %S (%d bytes), Right: %S (%d bytes)", to_string(tag), tprint_type(left_type), left_type.number.size, tprint_type(right_type), right_type.number.size);
                    } else if (result == LOOSE_R_TOO_BIG_FOR_L) {
                        report_error(assign->op_loc, "Right side of '%S' (%d bytes) is too big for the left side (%d bytes).", to_string(tag), right_type.number.size, left_type.number.size);
                    } else ShouldntReachHere;
                }
            }
        }
        
        Case   Assign_Tag::MOD:
        OrCase Assign_Tag::BITWISE_AND:
        OrCase Assign_Tag::BITWISE_OR:
        OrCase Assign_Tag::BITWISE_XOR: {
            // @Copypaste: :CopySolidifyInt
            if (left_type.tag != INT) {
                report_error(left, "Expected integer type for left side of '%S', but found type %S.", to_string(tag), tprint_type(left_type));
            } else if (right_type.tag != INT) {
                report_error(right, "Expected integer type for right side of '%S', but found type %S.", to_string(tag), tprint_type(right_type));
            } else {
                auto result = typecheck_number_types_one_way(left_type, right_type);
                using enum Typecheck_Result;
                if (result != ALL_GOOD) {
                    if (result == SET_R_TO_L) {
                        assert(!right_type.number.is_solid);
                        set_loose_number_type_expr_data_type_to_new_solid_type(right, left_type);
                    } else if (result == TYPE_MISMATCH) {
                        ShouldntReachHere;
                    } else if (result == SIZE_MISMATCH) {
                        report_error(assign->op_loc, "Left and right sides of '%S' doesn't have matching size. Left: %S (%d bytes), Right: %S (%d bytes)", to_string(tag), tprint_type(left_type), left_type.number.size, tprint_type(right_type), right_type.number.size);
                    } else if (result == LOOSE_R_TOO_BIG_FOR_L) {
                        report_error(assign->op_loc, "Right side of '%S' (%d bytes) is too big for the left side (%d bytes).", to_string(tag), right_type.number.size, left_type.number.size);
                    } else ShouldntReachHere;
                }
            }
        }
        
        Case   Assign_Tag::LEFT_SHIFT:
        OrCase Assign_Tag::RIGHT_SHIFT: {
            // @Copypaste: :CopySolidifyInt
            if (left_type.tag != INT) {
                report_error(left, "Expected integer type for left side of '%S', but found type %S.", to_string(tag), tprint_type(left_type));
            } else if (right_type.tag != INT) {
                report_error(right, "Expected integer type for right side of '%S', but found type %S.", to_string(tag), tprint_type(right_type));
            } else {
                if (!right_type.number.is_solid)  solidify_number_expr_data_type_to_default(right, right_type);
            }
        }
        
        Default: {
            bool need_int_operands = tag == Assign_Tag::MOD || tag == Assign_Tag::BITWISE_AND || tag == Assign_Tag::BITWISE_XOR || tag == Assign_Tag::BITWISE_OR;
            assert(need_int_operands || tag == Assign_Tag::ADD || tag == Assign_Tag::SUB || tag == Assign_Tag::MUL || tag == Assign_Tag::DIV);
            
            if (need_int_operands) {
                if (left_type.tag != INT) {
                    report_error(left, "Expected integer type for left side of '%S', but found type %S.", to_string(tag), tprint_type(left_type));
                    return;
                }
                if (right_type.tag != INT) {
                    report_error(right, "Expected integer type for right side of '%S', but found type %S.", to_string(tag), tprint_type(right_type));
                    return;
                }
            } else {
                if (tag == Assign_Tag::ADD) {
                    if (left_type.tag == POINTER && right_type.tag == INT) {
                        if (!right_type.number.is_solid)  solidify_number_expr_data_type_to_default(right, right_type);
                        break;
                    }
                } else if (tag == Assign_Tag::SUB) {
                    if (left_type.tag == POINTER && right_type.tag == INT) {
                        if (!right_type.number.is_solid)  solidify_number_expr_data_type_to_default(right, right_type);
                        break;
                    }
                }
                if (!is_number_type(left_type.tag)) {
                    report_error(left, "Left expression isn't a valid type for '%S': %S.", to_string(tag), tprint_type(left_type));
                    return;
                }
                if (!is_number_type(right_type.tag)) {
                    report_error(right, "Right expression isn't a valid type for '%S': %S.", to_string(tag), tprint_type(right_type));
                    return;
                }
            }
            
            auto result = typecheck_number_types_one_way(left_type, right_type);
            using enum Typecheck_Result;
            if (result != ALL_GOOD) {
                if (result == SET_R_TO_L) {
                    assert(!right_type.number.is_solid);
                    set_loose_number_type_expr_data_type_to_new_solid_type(right, left_type);
                } else if (result == TYPE_MISMATCH) {
                    report_error(assign->op_loc, "Left and right sides of '%S' doesn't have matching types. Left: %S, Right: %S", to_string(tag), tprint_type(left_type), tprint_type(right_type));
                } else if (result == SIZE_MISMATCH) {
                    report_error(assign->op_loc, "Left and right sides of '%S' doesn't have matching size. Left: %S (%d bytes), Right: %S (%d bytes)", to_string(tag), tprint_type(left_type), left_type.number.size, tprint_type(right_type), right_type.number.size);
                } else if (result == LOOSE_R_TOO_BIG_FOR_L) {
                    report_error(assign->op_loc, "Right side of '%S' (%d bytes) is too big for the left side (%d bytes).", to_string(tag), right_type.number.size, left_type.number.size);
                } else ShouldntReachHere;
            }
            
        }
    }
}

internal void
typecheck_decl(Ast_Decl *decl) {
    // @Cleanup: This function is a mess.
    
    bool is_constant = decl->ident.flags & Ast_Ident::CONSTANT;
    
#if 0
    // :MultiReturnRefactor Disabling multidecls for now until I refactor it.
    // Make it a completely seperate code path since it's only for proc calls.
    bool multi = decl->ident.next != null;
    if (multi) {
        if (decl->_type_expr) {
            report_info(decl->loc, "Putting a type expression when declaring multiple identifiers doesn't work for now. Use :=");
            return;
        }
        if (!decl->initialization) {
            report_info(decl->loc, "Declaraing multiple identifiers requires initialization.");
            return;
        }
        auto init_type = get_data_type(decl->initialization);
        if (init_type.tag != Data_Type_Tag::PROC || !init_type.proc.first_return_type_expr->next) {
            report_info(decl->initialization, "Initialization isn't a procedure call that has multiple return values.");
            return;
        }
        assert(decl->initialization->ast_tag == AST_CALL);
        
        auto ident_node = &decl->ident;
        auto type_node = init_type.proc.first_return_type_expr;
        while (true) {
            if (type_node && ident_node) {
            } else if (!type_node && !ident_node) {
                break;
            } else {
                if (type_node) {
                    // This is fine cause you're allowed to ignore return values
                    // Maybe make ignoring return values explict by requiring underscore
                    // :Must
                    break;
                } else {
                    report_error(ident_node, "Comma-separated list has too many identifiers.");
                    report_info(decl->initialization, "... here's the procedure.");
                    return;
                }
            }
            
            ident_node->data_type = type_node->type;
            
            ident_node = ident_node->next;
            type_node  = type_node->next;
        }
        
        decl->ident.data_type.tag = Data_Type_Tag::MULTI_RETURN_PROC_DEFN;
        return;
    }
#endif
    
    Data_Type type_expr_data_type = {};
    if (decl->_type_expr) {
        type_expr_data_type = typecheck_type_expr(decl->_type_expr);
        RetIfError();
    }
    
    if (decl->initialization) {
        if (decl->initialization->ast_tag == AST_EXPLICITLY_UNINITALIZE) {
            if (is_constant) {
                report_error(decl->loc, "You can't explicitly uninitialize a constant variable.");
                return;
            }
            if (!decl->_type_expr) {
                report_error(decl->loc, "An explicitly uninitialized variable must have a type.");
                return;
            }
            decl->ident.data_type = type_expr_data_type;
        } else {
            auto init_type = typecheck_expr(decl->initialization);
            RetIfError();
            
            if (decl->_type_expr) {
                //assert(decl->initialization->ast_tag != AST_TYPE_EXPR); // @Incomplete
                decl->ident.data_type = type_expr_data_type;
                auto decl_type = decl->ident.data_type;
                auto typecheck_result = typecheck_assign(decl_type, init_type);
                using enum Typecheck_Result;
                if (typecheck_result != ALL_GOOD) {
                    if (typecheck_result == SET_R_TO_L) {
                        assert(!init_type.number.is_solid);
                        // @Hack this subexpr cast...
                        set_loose_number_type_expr_data_type_to_new_solid_type(cast(Ast_Subexpr *, decl->initialization), decl_type);
                    } else {
                        if (typecheck_result == TYPE_MISMATCH) {
                            report_error(decl->initialization, "Initialization of type %S doesn't match the declared type of %S.", tprint_type(init_type), tprint_type(decl_type));
                        } else if (typecheck_result == SIZE_MISMATCH) {
                            report_error(decl->initialization, "Initialization type size (%d bytes) doesn't match the declared type size (%d bytes).", init_type.number.size, decl_type.number.size);
                        } else if (typecheck_result == LOOSE_R_TOO_BIG_FOR_L) {
                            report_error(decl->initialization, "Initialization type size (%d bytes) is too big for the declared type size (%d bytes).", init_type.number.size, decl_type.number.size);
                        } else ShouldntReachHere;
                        return;
                    }
                }
                RetIfError();
            } else {
                if (decl->initialization->ast_tag == AST_TYPE_EXPR) {
                    if (decl->ident.flags & Ast_Ident::IS_C_DECL) {
                        decl->ident.flags |= Ast_Ident::NO_NAME_MANGLE;
                        auto signature = cast(Ast_Type_Expr *, decl->initialization);
                        if (signature->type.tag != Data_Type_Tag::PROC) {
                            report_error(decl->loc, "Unexpected #c_decl directive. #c_decl is usually only used after procedure signature.");
                            return;
                        }
                        decl->ident.data_type = signature->type;
                    } else {
                        decl->ident.data_type.tag = Data_Type_Tag::USER_TYPE;
                        decl->ident.data_type.user_type.type_expr = cast(Ast_Type_Expr *, decl->initialization);
                    }
                } else {
                    if (is_constant && !is_constexpr(decl->initialization)) { // :Constexpr
                        report_error(decl->initialization, "Trying to initialize a constant variable to a non-constant expression.");
                        return;
                    }
                    if (is_number_type(init_type.tag)) {
                        if (!is_constant && !init_type.number.is_solid) {
                            init_type = solidify_number_expr_data_type_to_default(cast(Ast_Subexpr *, decl->initialization), init_type);
                        }
                    } else if (init_type.tag == Data_Type_Tag::POINTER || init_type.tag == Data_Type_Tag::STATIC_ARRAY) {
                        auto seek = init_type;
                        auto last_data_type_type = Data_Type_Tag::UNKNOWN;
                        while (true) {
                            if (seek.tag == Data_Type_Tag::STATIC_ARRAY) {
                                last_data_type_type = Data_Type_Tag::STATIC_ARRAY;
                                seek = *seek.array.of;
                            } else if (seek.tag == Data_Type_Tag::POINTER) {
                                last_data_type_type = Data_Type_Tag::POINTER;
                                seek = *seek.pointer.to;
                            } else {
                                break;
                            }
                        }
                        assert(!is_number_type(seek.tag) || seek.number.is_solid);
                        if (seek.tag == Data_Type_Tag::VOID) {
                            if (last_data_type_type == Data_Type_Tag::STATIC_ARRAY) {
                                report_error(decl->initialization, "Internal compiler error: Declaration initialization inferred to an array of type void.");
                                return;
                            }
                            if (last_data_type_type != Data_Type_Tag::POINTER) {
                                // @Cleanup: This should never happen.
                                report_error(decl->initialization, "Internal compiler error: Declaration initialization inferred to a base type of void but isn't a void pointer.");
                                return;
                            }
                        }
                    } else if (init_type.tag == Data_Type_Tag::VOID) {
                        report_error(decl->initialization, "Internal compiler error: Declaration initialization inferred to type void.");
                        return;
                    }
                    decl->ident.data_type = init_type;
                }
                
            }
        }
    } else { // no initializer
        assert(decl->_type_expr);
        decl->ident.data_type = type_expr_data_type;
    }
    //assert(!multi);
    assert(   decl->ident.data_type.tag != Data_Type_Tag::UNKNOWN
           || decl->ident.data_type.user_type_ident);
}


internal void
typecheck_statement(Ast *statement) {
    assert(statement);
    switch (statement->ast_tag) {
        Case AST_DECL: {
            auto decl = cast(Ast_Decl *, statement);
            
            typecheck_decl(decl);
            RetIfError();
            
            auto found_index = find_variable_index(decl->ident.name);
            bool is_redeclaration = found_index >= 0;
            
            if (decl->initialization && decl->initialization->ast_tag == AST_PROC_DEFN) {
                
                auto proc_defn = cast(Ast_Proc_Defn *, decl->initialization);
                assert(proc_defn && proc_defn->signature.type.tag == Data_Type_Tag::PROC);
                
                if (is_redeclaration) {
                    int overload_count = 1;
                    while (true) {
                        auto found_type = chai.variables[found_index].ident->data_type;
                        auto  decl_type = proc_defn->signature.type;
                        assert(found_type.tag == Data_Type_Tag::PROC && decl_type.tag == Data_Type_Tag::PROC);
                        
                        auto found_param = found_type.proc.first_param;
                        auto  decl_param =  decl_type.proc.first_param;
                        while (true) {
                            if (found_param && decl_param) {
                            } else if (!found_param && !decl_param) {
                                break;
                            } else {
                                is_redeclaration = false;
                                break;
                            }
                            
                            if (found_param->decl.ident.data_type.tag != decl_param->decl.ident.data_type.tag) {
                                is_redeclaration = false;
                                break;
                            }
                            found_param = found_param->next;
                            decl_param  =  decl_param->next;
                        }
                        if (is_redeclaration) {
                            report_error(&decl->ident, "Redeclared procedure of same type.");
                            report_info(chai.variables[found_index].ident, "... here's the previous declaration.");
                            return;
                        }
                        found_index = find_variable_index(decl->ident.name, found_index-1);
                        if (found_index >= 0) {
                            is_redeclaration = true;
                        } else {
                            break;
                        }
                        overload_count += 1;
                    }
                    
                    add_variable(&decl->ident);
                    
                    int overload_index = overload_count-1;
                    auto temp_name = tprint("%S_%x", decl->ident.name, overload_index);
                    auto var = &chai.variables[decl->ident.variable_index];
                    var->overloaded_proc_name.data = alloc_string(temp_name.count);
                    var->overloaded_proc_name.count = temp_name.count;
                    
                    ForI (i, 0, temp_name.count)  var->overloaded_proc_name.data[i] = temp_name.data[i];
                } else {
                    add_variable(&decl->ident);
                }
                
                assert(decl->ident.data_type.tag == Data_Type_Tag::PROC);
                assert(proc_defn->signature.type.tag == Data_Type_Tag::PROC);
                if (proc_defn->signature.type.proc.first_return_type_expr && proc_defn->signature.type.proc.first_return_type_expr->next) {
                    local_persist int multi_return_index = 0;
                    multi_return_index += 1;
                    decl->ident.data_type.proc.multi_return_index = multi_return_index;
                    proc_defn->signature.type.proc.multi_return_index = multi_return_index;
                }
                
                auto push_index = add_push_scope_variable();
                
                if (proc_defn->signature.type.proc.first_param) {
                    auto param = proc_defn->signature.type.proc.first_param;
                    while (param) {
                        add_variable(&param->decl.ident);
                        param = param->next;
                    }
                }
                
                auto parent_proc = chai.current_proc;
                chai.current_proc = proc_defn;
                
                Ast_Block_Statement_Node *node = proc_defn->body.first_statement;
                while (node) {
                    typecheck_statement(node->statement);
                    RetIfError();
                    node = node->next;
                }
                
                chai.current_proc = parent_proc;
                
                add_pop_scope_variable(push_index);
                RetIfError();
            } else { // not a procedure
#if 1
                if (is_redeclaration) {
                    report_error(&decl->ident, "Redeclared variable.");
                    report_info(chai.variables[found_index].ident, "... here's the previous declaration.");
                    return;
                }
                add_variable(&decl->ident);
#else
                // :MultiReturnRefactor
                auto node = &decl->ident;
                while (true) {
                    if (is_redeclaration) {
                        report_error(node, "Redeclared variable.");
                        report_info(chai.variables[found_index].ident, "... here's the previous declaration.");
                        return;
                    }
                    add_variable(node);
                    
                    if (!node->next) break;
                    node = node->next;
                    found_index = find_variable_index(node->name, found_index-1);
                    is_redeclaration = found_index >= 0;
                }
#endif
            }
            
            assert(   decl->ident.data_type.tag != Data_Type_Tag::UNKNOWN
                   || decl->ident.data_type.user_type_ident);
        }
        
        Case AST_ASSIGNMENT: {
            auto assign = cast(Ast_Assignment *, statement);
            typecheck_assignment(assign);
        }
        
        Case AST_SWITCH: {
            auto _switch = cast(Ast_Switch *, statement);
            typecheck_switch(_switch);
        }
        
        Case AST_BRANCH: {
            auto branch = cast(Ast_Branch *, statement);
            
            typecheck_subexpr(branch->condition);
            
            // @Incomplete: arrays should also get auto-converted to a bool statement
            auto cond_type = get_data_type(branch->condition);
            if (cond_type.tag != Data_Type_Tag::BOOL) {
                // @Incomplete: pull this out into typecheck_condition()
                if (cond_type.tag == Data_Type_Tag::POINTER) {
                    // @Hack
                    auto _null = NewAst(Ast_Literal);
                    _null->tag = Ast_Literal::NULL;
                    _null->loc = branch->condition->loc;
                    
                    _null->data_type.tag = Data_Type_Tag::POINTER;
                    _null->data_type.pointer.to = AllocDataTypeUninitialized();
                    *_null->data_type.pointer.to = {};
                    _null->data_type.pointer.to->tag = Data_Type_Tag::VOID;
                    _null->data_type.pointer.to->void_is_null = true;
                    
                    auto binary = NewAst(Ast_Binary);
                    binary->tag = Binary_Tag::NOT_EQUAL;
                    binary->data_type.tag = Data_Type_Tag::BOOL;
                    binary->left = branch->condition;
                    binary->right = _null;
                    branch->condition = binary;
                } else {
                    report_error(branch->condition, "Expected an 'if' condition expresssion of type bool, but found type %S.",
                                 tprint_type(cond_type));
                    return;
                }
            }
            
            assert(branch->true_statement);
            typecheck_statement(branch->true_statement);
            RetIfError();
            
            if (branch->false_statement) {
                typecheck_statement(branch->false_statement);
                RetIfError();
            }
        }
        
        Case AST_LOOP: {
            auto loop = cast(Ast_Loop *, statement);
            
            typecheck_subexpr(loop->condition);
            RetIfError();
            
            // @Incomplete: pointers and array should get auto-converted to a bool statement
            auto cond_type = get_data_type(loop->condition);
            if (cond_type.tag != Data_Type_Tag::BOOL) {
                report_error(loop->condition, "Expected a 'while' condition expresssion of type bool, but found type %S.", tprint_type(cond_type));
                return;
            }
            auto saved = chai.current_loop;
            chai.current_loop = loop;
            typecheck_statement(loop->statement);
            chai.current_loop = saved;
        }
        
        Case AST_FOR_COUNT: {
            // @Copypaste AST_FOR_EACH
            Data_Type index_type = {};
            index_type.tag = Data_Type_Tag::INT;
            index_type.number.size = 8;
            index_type.number.is_solid  = true;
            index_type.number.is_signed = true;
            
            auto loop = cast(Ast_For_Count *, statement);
            
            typecheck_subexpr(loop->start);
            RetIfError();
            
            typecheck_subexpr(loop->end);
            RetIfError();
            
            auto start_expr_type = get_data_type(loop->start);
            auto   end_expr_type = get_data_type(loop->end);
            
            // @Copypaste: :CopySolidifyInt
            auto result = typecheck_number_types_one_way(index_type, start_expr_type);
            using enum Typecheck_Result;
            if (result != ALL_GOOD) {
                if (result == SET_R_TO_L) {
                    assert(!start_expr_type.number.is_solid);
                    set_loose_number_type_expr_data_type_to_new_solid_type(loop->start, index_type);
                } else {
                    if (result == TYPE_MISMATCH || result == SIZE_MISMATCH) {
                        report_error(loop->start, "Expected type %S for start expression of a 'for' loop, but found type %S.", tprint_type(index_type), tprint_type(start_expr_type));
                    } else if (result == LOOSE_R_TOO_BIG_FOR_L) {
                        assert(!"Shouldn't reach here unless index_type gets changed");
                    } else ShouldntReachHere;
                    return;
                }
            }
            
            // @Copypaste: :CopySolidifyInt
            result = typecheck_number_types_one_way(index_type, end_expr_type);
            if (result != ALL_GOOD) {
                if (result == SET_R_TO_L) {
                    assert(!end_expr_type.number.is_solid);
                    set_loose_number_type_expr_data_type_to_new_solid_type(loop->end, index_type);
                } else {
                    if (result == TYPE_MISMATCH || result == SIZE_MISMATCH) {
                        report_error(loop->end, "Expected type %S for end expression of a 'for' loop, but found type %S.", tprint_type(index_type), tprint_type(end_expr_type));
                    } else if (result == LOOSE_R_TOO_BIG_FOR_L) {
                        assert(!"Shouldn't reach here unless index_type gets changed");
                    } else ShouldntReachHere;
                    return;
                }
            }
            
            // @Copypaste AST_FOR_EACH
            loop->it.data_type = index_type;
            
            auto push_index = add_push_scope_variable();
            add_variable(&loop->it);
            
            auto saved = chai.current_loop;
            chai.current_loop = loop;
            typecheck_statement(loop->statement);
            RetIfError();
            chai.current_loop = saved;
            
            add_pop_scope_variable(push_index);
        }
        
        Case AST_FOR_EACH: {
            // @Copypaste AST_FOR_COUNT
            Data_Type index_type = {};
            index_type.tag = Data_Type_Tag::INT;
            index_type.number.size = 8;
            index_type.number.is_solid  = true;
            index_type.number.is_signed = true;
            
            auto loop = cast(Ast_For_Each *, statement);
            
            typecheck_subexpr(loop->range);
            RetIfError();
            
            auto range_type = get_data_type(loop->range);
            if (range_type.tag != Data_Type_Tag::STATIC_ARRAY) {
                report_error(loop->range, "We don't currently have a range 'for' loop for non-array types.");
                return;
            }
            
            if (loop->pointer) {
                loop->it.data_type.tag = Data_Type_Tag::POINTER;
                loop->it.data_type.pointer.to = range_type.array.of;
            } else {
                loop->it.data_type = *range_type.array.of;
            }
            add_variable(&loop->it);
            
            // @Copypaste AST_FOR_COUNT
            loop->it_index.data_type = index_type;
            
            auto push_index = add_push_scope_variable();
            add_variable(&loop->it_index);
            
            auto saved = chai.current_loop;
            chai.current_loop = loop;
            typecheck_statement(loop->statement);
            RetIfError();
            chai.current_loop = saved;
            
            add_pop_scope_variable(push_index);
        }
        
        Case AST_RETURN: {
            auto _return = cast(Ast_Return *, statement);
            
            if (!chai.current_proc) {
                report_error(_return->loc, "'return' statement isn't in a procedure.");
                return;
            }
            
            auto first_return_type_expr = chai.current_proc->signature.type.proc.first_return_type_expr;
            if (!_return->first) {
                if (first_return_type_expr) {
                    report_error(_return->loc, "Missing return expression.");
                    report_info(first_return_type_expr, "... here's the return type.");
                    return;
                }
            } else {
                if (!first_return_type_expr) {
                    report_error(_return->first->expr, "Procedure doesn't have a return value.");
                    report_info(&chai.current_proc->signature, "... here's the procedure.");
                    return;
                }
                
                auto type_node = first_return_type_expr;
                auto expr_node = _return->first;
                
                while (true) {
                    if (type_node && expr_node) {
                    } else if (!type_node && !expr_node) {
                        break;
                    } else {
                        if (type_node) {
                            report_error(_return->loc, "Missing return expression.");
                            report_info(type_node, "... here's the return type.");
                        } else {
                            report_error(expr_node->expr, "Too many return expressions.");
                            report_info(&chai.current_proc->signature, "... here's the procedure.");
                        }
                        return;
                    }
                    typecheck_subexpr(expr_node->expr);
                    RetIfError();
                    
                    auto return_type = type_node->type;
                    auto expr_type   = get_data_type(expr_node->expr);
                    
                    auto typecheck_result = typecheck_assign(return_type, expr_type);
                    using enum Typecheck_Result;
                    if (typecheck_result != ALL_GOOD) {
                        if (typecheck_result == SET_R_TO_L) {
                            assert(!expr_type.number.is_solid);
                            set_loose_number_type_expr_data_type_to_new_solid_type(expr_node->expr, return_type);
                        } else {
                            assert((typecheck_result == TYPE_MISMATCH || typecheck_result == SIZE_MISMATCH || typecheck_result == LOOSE_R_TOO_BIG_FOR_L));
                            report_error(expr_node->expr, "Return expression type %S doesn't match procedure return type.", tprint_type(expr_type));
                            report_info(type_node, "... here's the return type.");
                            return;
                        }
                    }
                    type_node = type_node->next;
                    expr_node = expr_node->next;
                }
            }
            
            _return->multi_return_index = chai.current_proc->signature.type.proc.multi_return_index;
        }
        
        Case AST_BREAK: {
            auto _break = cast(Ast_Break *, statement);
            
            if (!chai.current_loop) {
                report_error(_break->loc, "'break' statement isn't in a loop.");
                return;
            }
        }
        
        Case AST_CONTINUE: {
            auto _continue = cast(Ast_Continue *, statement);
            
            if (!chai.current_loop) {
                report_error(_continue->loc, "'continue' statement isn't in a loop.");
                return;
            }
        }
        
        Case AST_BLOCK: {
            auto block = cast(Ast_Block *, statement);
            if (block->first_statement) {
                auto push_index = add_push_scope_variable();
                auto node = block->first_statement;
                while (node) {
                    typecheck_statement(node->statement);
                    node = node->next;
                }
                add_pop_scope_variable(push_index);
            }
        }
        
        Case AST_LOAD: {
            // do nothing for now
        }
        
        Case AST_STATIC_BRANCH: {
            assert_m(false, "#if should be disabled in the parser.");
            
            auto branch = cast(Ast_Static_Branch *, statement);
            
            // evaluate_constexpr(branch);
            
            if (branch->condition_is_true) {
                typecheck_statement(branch->true_statement);
            } else if (branch->false_statement) {
                typecheck_statement(branch->false_statement);
            }
        }
        Default: {
            typecheck_expr(cast(Ast_Expr *, statement));
        }
    }
}

internal void
typecheck_everything(Array<Ast *> statements) {
    
    chai.string_count_type.tag = Data_Type_Tag::INT;
    chai.string_count_type.number.is_signed = true;
    chai.string_count_type.number.is_solid = true;
    chai.string_count_type.number.size = 8;
    
    chai.string_data_type.tag = Data_Type_Tag::POINTER;
    chai.string_data_type.pointer.to = AllocDataTypeUninitialized();
    *chai.string_data_type.pointer.to = {};
    chai.string_data_type.pointer.to->tag = Data_Type_Tag::INT;
    chai.string_data_type.pointer.to->number.is_signed = false;
    chai.string_data_type.pointer.to->number.is_solid = true;
    chai.string_data_type.pointer.to->number.size = 1;
    
    chai.array_count_type = chai.string_count_type;
    
    ForI (i, 0, statements.count) {
        typecheck_statement(statements[i]);
    }
    
}


#if 0

internal Ast_Literal
evaluate_const_subexpr_to_literal(Ast_Subexpr *expr) {
    assert(is_const_subexpr(expr));
    Ast_Literal literal; // :GetsDefaultValues
    
    if (expr->ast_tag == AST_BINARY) {
        auto binary = cast(Ast_Binary *, expr);
        assert(is_constexpr(binary->left));
        assert(is_constexpr(binary->right));
        auto  left_const = evaluate_const_subexpr_to_literal(binary->left);
        auto right_const = evaluate_const_subexpr_to_literal(binary->right);
        auto type = binary->data_type;
        assert(!is_number_type(type.tag) || type.number.is_solid);
        
#define IntToIntOp(op) \
if (type.number.is_signed) { \
if (type.number.size == 8) { \
literal._s64 = left_const._s64 op right_const._s64; \
} else if (type.number.size == 4) { \
literal._s32 = left_const._s32 op right_const._s32; \
} else if (type.number.size == 2) { \
literal._s16 = left_const._s16 op right_const._s16; \
} else { assert(type.number.size == 1); \
literal._s8  = left_const._s8  op right_const._s8; \
} assert(literal._s64 == (left_const._s64 op right_const._s64)); \
} else { \
if (type.number.size == 8) { \
literal._u64 = left_const._u64 op right_const._u64; \
} else if (type.number.size == 4) { \
literal._u32 = left_const._u32 op right_const._u32; \
} else if (type.number.size == 2) { \
literal._u16 = left_const._u16 op right_const._u16; \
} else { assert(type.number.size == 1); \
literal._u8  = left_const._u8  op right_const._u8; \
} assert(literal._u64 == (left_const._u64 op right_const._u64)); \
}
#define IntToBoolOp(op) \
if (type.number.is_signed) { \
if (type.number.size == 8) { \
literal._bool = left_const._s64 op right_const._s64; \
} else if (type.number.size == 4) { \
literal._bool = left_const._s32 op right_const._s32; \
} else if (type.number.size == 2) { \
literal._bool = left_const._s16 op right_const._s16; \
} else { assert(type.number.size == 1); \
literal._bool = left_const._s8  op right_const._s8; \
} assert(literal._bool == (left_const._s64 op right_const._s64)); \
} else { \
if (type.number.size == 8) { \
literal._bool = left_const._u64 op right_const._u64; \
} else if (type.number.size == 4) { \
literal._bool = left_const._u32 op right_const._u32; \
} else if (type.number.size == 2) { \
literal._bool = left_const._u16 op right_const._u16; \
} else { assert(type.number.size == 1); \
literal._bool = left_const._u8  op right_const._u8; \
} assert(literal._bool == (left_const._u64 op right_const._u64)); \
}
        
        using enum Binary_Tag;
        switch (binary->tag) {
            using enum Data_Type_Tag;
            Case MUL: {
                if (type.tag == INT) {
                    IntToIntOp(*);
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._f64 = left_const._f64 * right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._f32 = left_const._f32 * right_const._f32;
                    }
                }
            }
            
            Case DIV: {
                if (type.tag == INT) {
                    IntToIntOp(/);
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._f64 = left_const._f64 / right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._f32 = left_const._f32 / right_const._f32;
                    }
                }
            }
            
            Case MOD: {
                assert(type.tag == INT);
                IntToIntOp(%);
            }
            
            Case ADD: {
                if (type.tag == INT) {
                    IntToIntOp(+);
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._f64 = left_const._f64 + right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._f32 = left_const._f32 + right_const._f32;
                    }
                }
                // @Incomplete: does pointer+int/int+pointer need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case SUB: {
                if (type.tag == INT) {
                    IntToIntOp(-);
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._f64 = left_const._f64 - right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._f32 = left_const._f32 - right_const._f32;
                    }
                }
                // @Incomplete: does pointer-int/pointer-pointer need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case BITWISE_AND: {
                assert(type.tag == INT);
                IntToIntOp(&);
            }
            
            Case BITWISE_XOR: {
                assert(type.tag == INT);
                IntToIntOp(^);
            }
            
            Case BITWISE_OR: {
                assert(type.tag == INT);
                IntToIntOp(|);
            }
            
            Case LEFT_SHIFT: {
                assert(type.tag == INT);
                assert(right_const.data_type.tag == INT);
                assert(right_const.data_type.number.is_solid);
                assert(!right_const.data_type.number.is_signed);
                assert(right_const._u8  == right_const._u64);
                assert(right_const._u16 == right_const._u64);
                assert(right_const._u32 == right_const._u64);
                if (type.number.is_signed) {
                    // just _u/s64 should be enough right? maybe not cause casts @Incomplete
                    if (type.number.size == 8) {
                        literal._s64 = left_const._s64 << right_const._u64;
                    } else if (type.number.size == 4) {
                        literal._s32 = left_const._s32 << right_const._u64;
                    } else if (type.number.size == 2) {
                        literal._s16 = left_const._s16 << right_const._u64;
                    } else { assert(type.number.size == 1);
                        literal._s8  = left_const._s8  << right_const._u64;
                    } assert(literal._s64 == (left_const._s64 << right_const._u64));
                } else {
                    if (type.number.size == 8) {
                        literal._u64 = left_const._u64 << right_const._u64;
                    } else if (type.number.size == 4) {
                        literal._u32 = left_const._u32 << right_const._u64;
                    } else if (type.number.size == 2) {
                        literal._u16 = left_const._u16 << right_const._u64;
                    } else { assert(type.number.size == 1);
                        literal._u8  = left_const._u8  << right_const._u64;
                    }
                } assert(literal._u64 == (left_const._u64 << right_const._u64));
            }
            
            Case RIGHT_SHIFT: {
                assert(type.tag == INT);
                assert(right_const.data_type.tag == INT);
                assert(right_const.data_type.number.is_solid);
                assert(!right_const.data_type.number.is_signed);
                assert(right_const._u8  == right_const._u64);
                assert(right_const._u16 == right_const._u64);
                assert(right_const._u32 == right_const._u64);
                if (type.number.is_signed) {
                    // just _u/s64 should be enough right? maybe not cause casts @Incomplete
                    if (type.number.size == 8) {
                        literal._s64 = left_const._s64 >> right_const._u64;
                    } else if (type.number.size == 4) {
                        literal._s32 = left_const._s32 >> right_const._u64;
                    } else if (type.number.size == 2) {
                        literal._s16 = left_const._s16 >> right_const._u64;
                    } else { assert(type.number.size == 1);
                        literal._s8  = left_const._s8  >> right_const._u64;
                    } assert(literal._s64 == (left_const._s64 >> right_const._u64));
                } else {
                    if (type.number.size == 8) {
                        literal._u64 = left_const._u64 >> right_const._u64;
                    } else if (type.number.size == 4) {
                        literal._u32 = left_const._u32 >> right_const._u64;
                    } else if (type.number.size == 2) {
                        literal._u16 = left_const._u16 >> right_const._u64;
                    } else { assert(type.number.size == 1);
                        literal._u8  = left_const._u8  >> right_const._u64;
                    } assert(literal._u64 == (left_const._u64 >> right_const._u64));
                }
            }
            
            
            Case LESS_THAN: {
                assert(!"not type.tag, the type of left and right need to be checked instead");
                if (type.tag == INT) {
                    IntToBoolOp(<)
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._bool = left_const._f64 < right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._bool = left_const._f32 < right_const._f32;
                    }
                }
                // @Incomplete: do pointers need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case GREATER_THAN: {
                assert(!"not type.tag, the type of left and right need to be checked instead");
                if (type.tag == INT) {
                    IntToBoolOp(>)
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._bool = left_const._f64 > right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._bool = left_const._f32 > right_const._f32;
                    }
                }
                // @Incomplete: do pointers need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case LESS_THAN_OR_EQUAL: {
                assert(!"not type.tag, the type of left and right need to be checked instead");
                if (type.tag == INT) {
                    IntToBoolOp(<=)
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._bool = left_const._f64 <= right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._bool = left_const._f32 <= right_const._f32;
                    }
                }
                // @Incomplete: do pointers need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case GREATER_THAN_OR_EQUAL: {
                assert(!"not type.tag, the type of left and right need to be checked instead");
                if (type.tag == INT) {
                    IntToBoolOp(>=)
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._bool = left_const._f64 >= right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._bool = left_const._f32 >= right_const._f32;
                    }
                }
                // @Incomplete: do pointers need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case IS_EQUAL: {
                assert(!"not type.tag, the type of left and right need to be checked instead");
                if (type.tag == INT) {
                    IntToBoolOp(==);
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._bool = left_const._f64 == right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._bool = left_const._f32 == right_const._f32;
                    }
                }
                // @Incomplete: do pointers/procs/strings need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case NOT_EQUAL: {
                assert(!"not type.tag, the type of left and right need to be checked instead");
                if (type.tag == INT) {
                    IntToBoolOp(==);
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._bool = left_const._f64 != right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._bool = left_const._f32 != right_const._f32;
                    }
                }
                // @Incomplete: do pointers/procs/strings need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case LOGICAL_AND: {
                assert(type.tag == BOOL);
                literal._bool = left_const._bool && right_const._bool;
                // @Incomplete: do pointers/procs/strings need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case LOGICAL_OR: {
                assert(type.tag == BOOL);
                literal._bool = left_const._bool || right_const._bool;
                // @Incomplete: do pointers/procs/strings need to be handled here? or will the typecheck just give it cleanly to us?
            }
#undef IntToBoolOp
#undef IntToIntOp
            InvalidDefaultCase("Unexpectd binary operation in evaluate_const_subexpr_to_literal");
        }
    } else if (expr->ast_tag == AST_UNARY) {
        auto unary = cast(Ast_Unary *, expr);
        assert(is_constexpr(unary->right));
        auto right_const = evaluate_const_subexpr_to_literal(unary->right);
        auto type = unary->data_type;
        assert(!is_number_type(type.tag) || type.number.is_solid);
        
        using enum Unary_Tag;
        switch (unary->tag) {
            using enum Data_Type_Tag;
            Case MINUS: {
                if (type.tag == INT) {
                    assert(type.number.is_signed);
                    if (type.number.size == 8) {
                        literal._s64 = -right_const._s64;
                    } else if (type.number.size == 4) {
                        literal._s32 = -right_const._s32;
                    } else if (type.number.size == 2) {
                        literal._s16 = -right_const._s16;
                    } else { assert(type.number.size == 1);
                        literal._s8  = -right_const._s8;
                    } assert(literal._s64 == -right_const._s64);
                    //} else {
                    //if (type.number.size == 8) {
                    //literal._u64 = -right_const._u64;
                    //} else if (type.number.size == 4) {
                    //literal._u32 = -right_const._u32;
                    //} else if (type.number.size == 2) {
                    //literal._u16 = -right_const._u16;
                    //} else { assert(type.number.size == 1);
                    //literal._u8  = -right_const._u8;
                    //} assert(literal._u64 == -right_const._u64);
                    //}
                } else { assert(type.tag == FLOAT);
                    if (type.number.size == 8) {
                        literal._bool = -right_const._f64;
                    } else { assert(type.number.size == 4);
                        literal._bool = -right_const._f32;
                    }
                }
            }
            
            Case LOGICAL_NOT: {
                assert(type.tag == BOOL);
                literal._bool = !right_const._bool;
                // @Incomplete: do pointers/procs/strings need to be handled here? or will the typecheck just give it cleanly to us?
            }
            
            Case BITWISE_NOT: {
                assert(type.tag == INT);
                if (type.number.is_signed) { // just _u/s64 should be enough right? maybe not cause casts @Incomplete
                    if (type.number.size == 8) {
                        literal._s64 = ~right_const._s64;
                    } else if (type.number.size == 4) {
                        literal._s32 = ~right_const._s32;
                    } else if (type.number.size == 2) {
                        literal._s16 = ~right_const._s16;
                    } else { assert(type.number.size == 1);
                        literal._s8  = ~right_const._s8;
                    } assert(literal._s64 == ~right_const._s64);
                } else {
                    if (type.number.size == 8) {
                        literal._u64 = ~right_const._u64;
                    } else if (type.number.size == 4) {
                        literal._u32 = ~right_const._u32;
                    } else if (type.number.size == 2) {
                        literal._u16 = ~right_const._u16;
                    } else { assert(type.number.size == 1);
                        literal._u8  = ~right_const._u8;
                    } assert(literal._u64 == ~right_const._u64);
                } assert(literal._u64 == ~right_const._u64);
            }
            
            Case CAST: {
                assert(false); // @Incomplete
            }
        }
    } else if (expr->ast_tag == AST_LITERAL) {
        //return true;
    } else if (expr->ast_tag == AST_IDENT) {
        auto ident = cast(Ast_Ident *, expr);
        assert(ident->flags == chai.variables[ident->variable_index].ident->flags);
        //return ident->flags & Ast_Ident::CONSTANT;
    } else if (expr->ast_tag == AST_SIZEOF) {
        //return true;
    }
    
    return literal;
}
#endif
