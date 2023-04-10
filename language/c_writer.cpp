#define C_WRITER_ENABLED 1

internal bool create_empty_file(char *filename);
internal bool append_to_existing_file(char *filename, Str data);


global Str g_string_of_8_tabs_worth_of_spaces = sl("                                ");
//                                                  12345678901234567890123456789012
global s32 g_nesting;

global int c_buffer_used;
global char c_buffer[8*STB_SPRINTF_MIN]; // STB_SPRINTF_MIN == 512 usually

internal s32
get_c_binary_precedence(Binary_Tag type) {
    Binary_Info result = {};
    using enum Binary_Tag;
    switch (type) {
        Case LOGICAL_OR : return 2;
        Case LOGICAL_AND: return 3;
        
        Case BITWISE_OR : return 4;
        Case BITWISE_XOR: return 5;
        Case BITWISE_AND: return 6;
        
        Case    IS_EQUAL: // through
        OrCase NOT_EQUAL: return 7;
        
        Case   GREATER_THAN         : // through
        OrCase    LESS_THAN         : // through
        OrCase GREATER_THAN_OR_EQUAL: // through
        OrCase    LESS_THAN_OR_EQUAL: return 8;
        
        Case    LEFT_SHIFT: // through
        OrCase RIGHT_SHIFT: return 9;
        
        Case   ADD: // through
        OrCase SUB: return 10;
        
        Case   MUL: // through
        OrCase DIV: // through
        OrCase MOD: return 11;
        
        // precedence 12 is for unary operators
        
        Case ARRAY_ACCESS: // through
        OrCase        DOT: return 13;
        
        InvalidDefaultCase("Unhandled binary type in get_c_binary_precedence") return -1;
    }
}

internal char *
get_next_c_buffer_and_flush_to_file_if_full(const char *, void *, int bytes_written_in_previously_returned_buffer) {
    assert(bytes_written_in_previously_returned_buffer >= 0);
    
    c_buffer_used   += bytes_written_in_previously_returned_buffer;
    auto next_buffer = c_buffer             + c_buffer_used;
    auto bytes_left  = ArrayCount(c_buffer) - c_buffer_used;
    assert(bytes_left >= 0);
    
    // Since stbsp_vsprintfcb needs at least STB_SPRINTF_MIN bytes,
    // the callback needs to always return a buffer of at least STB_SPRINTF_MIN bytes.
    // If there isn't STB_SPRINTF_MIN bytes left in c_buffer after this, then flush the buffer and start fresh.
    
    if (bytes_left < STB_SPRINTF_MIN) {
        auto data = Str{ .count = c_buffer_used, .data = c_buffer };
        bool success = append_to_existing_file(chai.fully_pathed_output_file_name.data, data);
        assert(success);
        
        c_buffer_used = 0;
        next_buffer   = c_buffer;
        bytes_left    = ArrayCount(c_buffer);
    }
    
    assert(bytes_left >= STB_SPRINTF_MIN);
    return next_buffer;
}

global int t_buffer_used;
global Str t_buffer;

global Str c_write_tprint_result;

// MASSIVE @Hack !!! What on earth was I thinking?
internal char *
tprint_callback(const char *, void *, int bytes_written_in_previously_returned_buffer) {
    assert(bytes_written_in_previously_returned_buffer >= 0);
    
    t_buffer_used   += bytes_written_in_previously_returned_buffer;
    auto next_buffer = t_buffer.data  + t_buffer_used;
    auto bytes_left  = t_buffer.count - t_buffer_used;
    assert(bytes_left >= 0);
    
    // Since stbsp_vsprintfcb needs at least STB_SPRINTF_MIN bytes,
    // the callback needs to always return a buffer of at least STB_SPRINTF_MIN bytes.
    
    if (bytes_left < STB_SPRINTF_MIN) {
        assert(!"Temparary storage buffer got full in c_write.");
        return null;
    }
    return next_buffer;
}

global char *(*c_write_callback)(const char *, void *, int) = get_next_c_buffer_and_flush_to_file_if_full;

internal int
c_write(char *format, ...) {
    if (c_write_callback == tprint_callback) {
        t_buffer = xxx_arena_get_free_memory(&g_frame_arena);
        t_buffer_used = 0;
    }
    
    auto start_buffer = c_write_callback(0, null, 0);
    
    va_list args;
    va_start(args, format);
    int bytes_written = stbsp_vsprintfcb(c_write_callback, null, start_buffer, format, args);
    va_end(args);
    
    if (c_write_callback == tprint_callback) {
        assert(bytes_written == t_buffer_used);
        xxx_arena_commit_used_bytes(&g_frame_arena, t_buffer_used);
        c_write_tprint_result = tprint("%S%S", c_write_tprint_result, Str{ .count = t_buffer_used, .data = t_buffer.data });
    }
    
    return bytes_written;
}

internal inline Str
tprint_ident(Ast_Ident *ident) {
    if (!ident) return {};
    //if (ident->variable_index < 0 || ident->variable_index == chai.main_proc_variable_index) return ident->name;
    if (ident->variable_index >= 0 && chai.variables[ident->variable_index].overloaded_proc_name.count > 0) {
        return chai.variables[ident->variable_index].overloaded_proc_name;
    }
    //if (ident->flags & Ast_Ident::NO_NAME_MANGLE)
    return ident->name;
    
    //return tprint("%S_%x", ident->name, ident->variable_index);
}

internal inline void
write_newline_and_indent() {
    c_write("\n");
    if (g_nesting <= 0) return;
    
    while (g_nesting > 8) {
        c_write("%.*s", 8*4, g_string_of_8_tabs_worth_of_spaces.data);
        g_nesting -= 8;
    }
    c_write("%.*s", g_nesting*4, g_string_of_8_tabs_worth_of_spaces.data);
}

internal inline void
write_indent() {
    if (g_nesting <= 0) return;
    
    while (g_nesting > 8) {
        c_write("%.*s", 8*4, g_string_of_8_tabs_worth_of_spaces.data);
        g_nesting -= 8;
    }
    c_write("%.*s", g_nesting*4, g_string_of_8_tabs_worth_of_spaces.data);
}

internal void
write_base_type_ident(Data_Type type) {
    if (type.user_type_ident) {
        c_write("%S", tprint_ident(type.user_type_ident));
    } else switch (type.tag) {
        using enum Data_Type_Tag;
        Case INT: {
            assert(type.number.is_solid);
            assert(type.number.size == 1 || type.number.size == 2 || type.number.size == 4 || type.number.size == 8);
            c_write("%c%d", type.number.is_signed ? 's' : 'u', type.number.size*8);
        }
        Case FLOAT: {
            assert(type.number.is_solid);
            assert(type.number.size == 4 || type.number.size == 8);
            c_write("f%d", type.number.size*8);
        }
        Case BOOL: {
            c_write("int");
        }
        Case VOID: {
            c_write("void");
        }
        Case STRING: {
            c_write("string");
        }
        InvalidDefaultCase("Unhandled data type");
    }
}

internal void write_statement(Ast *ast);
internal void write_type(Data_Type type, Ast_Ident *decl_ident);
internal void write_ast(Ast *ast);

internal void
write_return_type(Data_Type::Procedure proc) {
    auto node = proc.first_return_type_expr;
    if (node) {
        if (node->next) {
            assert(proc.multi_return_index > 0);
            
            c_write("_%x", proc.multi_return_index);
        } else {
            write_type(node->type, null);
        }
    } else {
        c_write("void");
    }
}

internal void
write_type(Data_Type type, Ast_Ident *decl_ident) {
    
    if (type.tag == Data_Type_Tag::PROC && !type.user_type_ident) {
        write_return_type(type.proc);
        c_write(" (*%S)(", tprint_ident(decl_ident));
        auto param = type.proc.first_param;
        while (true) {
            write_type(param->decl.ident.data_type, &param->decl.ident);
            if (!param->next) break;
            param = param->next;
            c_write(", ");
        }
        c_write(")");
    } else if (type.tag == Data_Type_Tag::STRUCT && !type.user_type_ident) {
        c_write("struct {");
        g_nesting += 1;
        
        Ast_Explicitly_Uninitalize uninit; // Gets default values
        auto member = type.structure.first_member;
        while (true) {
            write_newline_and_indent();
            auto old_init = member->decl.initialization;
            member->decl.initialization = &uninit; // @Hack
            write_statement(&member->decl);
            member->decl.initialization = old_init;
            if (!member->next) break;
            member = member->next;
        }
        g_nesting -= 1;
        write_newline_and_indent();
        c_write("}");
        if (decl_ident) c_write(" %S", tprint_ident(decl_ident));
    } else if (type.tag == Data_Type_Tag::TYPEOF && !type.user_type_ident) {
        
        assert(c_write_tprint_result.count == 0 && c_write_tprint_result.data == null);
        assert(c_write_callback != tprint_callback);
        auto saved = c_write_callback;
        c_write_callback = tprint_callback;
        write_ast(type.typeof.expr);
        c_write_callback = saved;
        
        auto expr = c_write_tprint_result;
        
        c_write("%S", expr);
        c_write_tprint_result = {};
    } else {
        auto last = tprint_ident(decl_ident);
        
        while (true) {
            if (type.tag == Data_Type_Tag::POINTER) {
                auto to_tag = type.pointer.to->tag;
                if (to_tag == Data_Type_Tag::POINTER) {
                    last = tprint("*%S", last);
                    type = *type.pointer.to;
                } else if (to_tag == Data_Type_Tag::STATIC_ARRAY) {
                    last = tprint("(*%S)", last);
                    type = *type.pointer.to;
                } else if (to_tag == Data_Type_Tag::PROC) {
                    assert(!"Trying to print a pointer to a procedure @Temporary");
                } else {
                    write_base_type_ident(*type.pointer.to);
                    c_write(" *%S", last);
                    return;
                }
            } else if (type.tag == Data_Type_Tag::STATIC_ARRAY) {
                auto of_tag = type.array.of->tag;
                
                assert(c_write_tprint_result.count == 0 && c_write_tprint_result.data == null);
                assert(c_write_callback != tprint_callback);
                auto saved = c_write_callback;
                c_write_callback = tprint_callback;
                write_ast(type.array.size_expr);
                c_write_callback = saved;
                
                auto size = c_write_tprint_result;
                
                last = tprint("%S[%S]", last, size);
                c_write_tprint_result = {};
                if (of_tag == Data_Type_Tag::POINTER) {
                    type = *type.array.of;
                } else if (of_tag == Data_Type_Tag::STATIC_ARRAY) {
                    type = *type.array.of;
                } else if (of_tag == Data_Type_Tag::PROC) {
                    assert(!"Trying to print an array of procedures @Temporary");
                } else {
                    write_base_type_ident(*type.pointer.to);
                    c_write(" %S", last);
                    return;
                }
            } else {
                write_base_type_ident(type);
                c_write("%s%S", decl_ident ? " " : "", last);
                return;
            }
        }
        assert(!"Shouldn't reach here");
    }
}

internal inline void
write_type_no_name(Data_Type type) {
    write_type(type, null);
}

internal void
write_ast(Ast *ast) {
#if C_WRITER_ENABLED
    if (!ast) {
        c_write("(***NULL_AST!!!***)");
        assert(!"Null ast");
        return;
    }
    switch (ast->ast_tag) {
        
        Case AST_CALL: {
            auto call = cast(Ast_Call *, ast);
            c_write("%S(", tprint_ident(call));
            auto arg = call->first_arg;
            if (arg) while (true) {
                write_ast(arg->expr);
                arg = arg->next;
                if (!arg) break;
                c_write(", ");
            }
            c_write(")");
        }
        
        Case AST_IDENT: {
            auto ident = cast(Ast_Ident *, ast);
            c_write("%S", tprint_ident(ident));
        }
        
        Case AST_LITERAL: {
            auto literal = cast(Ast_Literal *, ast);
            auto text = literal->text;
            
#if 0 // Disabled cause non-solid constants messed it up
            assert(!is_number_type(literal->data_type.tag) || literal->data_type.number.is_solid);
#endif
            
            if (literal->data_type.tag == Data_Type_Tag::STRING) {
                Str free_memory = xxx_arena_get_free_memory(&g_frame_arena);
                Str dest = free_memory;
                
                auto s = literal->s;
                auto src = Str{0, s.data};
                
                auto escaped_string = Str{ .count = 0, .data = free_memory.data };
                
                ForI (i, 0, s.count) {
                    Str esc = {};
                    switch (s.data[i]) {
                        Case '\\':   esc = sl("\\\\");
                        Case '\"':   esc = sl("\\\"");
                        Case '\n':   esc = sl("\\n");
                        Case '\r':   esc = sl("\\r");
                        Case '\0':   esc = sl("\\0");
                        Case '\x1b': esc = sl("\\x1b");
                    }
                    
                    if (esc.data) {
                        assert(dest.count >= src.count);
                        ForI (j, 0, src.count)  dest.data[j] = src.data[j];
                        dest = advance(dest, src.count);
                        
                        ForI (j, 0, esc.count)  dest.data[j] = esc.data[j];
                        dest = advance(dest, esc.count);
                        
                        escaped_string.count += src.count + esc.count;
                        src.count += 1; // Skip the unescaped character
                        src = advance(src, src.count);
                    } else {
                        src.count += 1;
                    }
                }
                ForI (j, 0, src.count)  dest.data[j] = src.data[j];
                escaped_string.count += src.count;
                xxx_arena_commit_used_bytes(&g_frame_arena, escaped_string.count);
                c_write("(string){%d, (u8*)\"%S\"}", s.count, escaped_string);
            } else if (starts_with(text, sl("0x"))) { // @Incomplete @Temporary @Hack
                assert(literal->data_type.tag == Data_Type_Tag::INT);
                ForI (i, 0, text.count) {
                    if (text.data[i] != '_') c_write("%c", text.data[i]);
                }
            } else if (starts_with(text, sl("0h"))) { // @Incomplete @Temporary @Hack
                assert(literal->data_type.tag == Data_Type_Tag::FLOAT);
                assert(literal->data_type.number.is_solid);
                if (literal->data_type.number.size == 4) {
                    c_write("%a", literal->_f32);
                } else { assert(literal->data_type.number.size == 8);
                    c_write("%a", literal->_f64);
                }
            } else if (literal->data_type.tag == Data_Type_Tag::FLOAT) {
                auto suffix = sl("f");
                if (!literal->data_type.number.is_solid || literal->data_type.number.size == 4) {
                    // f suffix
                } else { assert(literal->data_type.number.size == 8);
                    suffix.count = 0;
                }
                
                auto dot_zero = sl(".0");
                ForI (i, 0, text.count) {
                    if (text.data[i] == '.') {
                        dot_zero.count = 0;
                        break;
                    }
                }
                c_write("%S%S%S", text, dot_zero, suffix);
            } else if (literal->data_type.tag == Data_Type_Tag::BOOL) {
                c_write("%s", literal->_bool ? "true" : "false"); // Need to be added in the header
            } else if (literal->tag == Ast_Literal::NULL) {
                c_write("null");
            } else {
                assert(literal->data_type.tag == Data_Type_Tag::INT);
                if (literal->is_char) {
                    u32 c = literal->_u32;
                    assert((c >= ' ' && c <= '~') || c == '\\' || c == '\"' || c == '\n' || c == '\r' || c == '\0' || c == '\x1b');
                    
                    if (c >= ' ' && c <= '~') {
                        c_write("'%c'", c);
                    } else {
                        char esc = -1;
                        switch (c) {
                            Case '\\':   esc = '\\';
                            Case '\"':   esc = '"';
                            Case '\n':   esc = 'n';
                            Case '\r':   esc = 'r';
                            Case '\0':   esc = '0';
                        }
                        if (esc != -1) {
                            c_write("'\\%c'", esc);
                        } else {
                            assert(c == '\x1b');
                            c_write("'\\x%x'", c);
                        }
                    }
                } else {
                    c_write("%S", text);
                }
            }
        }
        
        
        Case AST_SIZEOF: {
            // wrapped in parens cause sizeof(a)[0] -> sizeof((a)[0]) in C when we'd expect (sizeof(a))[0]
            auto size_of = cast(Ast_Sizeof *, ast);
            c_write("(sizeof(");
            write_type_no_name(size_of->type_expr.type);
            c_write("))");
        }
        
        Case AST_UNARY: {
            auto unary = cast(Ast_Unary *, ast);
            //assert(!is_number_type(unary->data_type.kind) || unary->data_type.number.is_solid);
            
            switch (unary->tag) {
                using enum Unary_Tag;
                Case CAST: {
                    auto cast = cast(Ast_Cast *, ast);
                    c_write("(");
                    write_type_no_name(cast->type_expr.type);
                    c_write(")");
                }
                Case POINTER_TO:  c_write("&");
                Case DEREFERENCE: c_write("*");
                Case MINUS:       c_write("-");
                Case LOGICAL_NOT: c_write("!");
                Case BITWISE_NOT: c_write("~");
                InvalidDefaultCase("Unhandled unary type");
            }
            bool right_needs_parens = false;
            if (unary->right->ast_tag == AST_BINARY) {
                auto r_prec = get_c_binary_precedence(cast(Ast_Binary *, unary->right)->tag);
                s32 unary_prec = 12;
                if (r_prec < unary_prec) {
                    right_needs_parens = true;
                }
            }
            if (right_needs_parens) c_write("(");
            write_ast(unary->right);
            if (right_needs_parens) c_write(")");
        }
        
        Case AST_BINARY: {
            using enum Data_Type_Tag;
            auto binary = cast(Ast_Binary *, ast);
            
            auto left_type = get_data_type(binary->left);
            
            if (binary->tag == Binary_Tag::DOT && (left_type.tag == STATIC_ARRAY || (left_type.tag == POINTER && left_type.pointer.to->tag == STATIC_ARRAY))) {
                assert(binary->right->ast_tag == AST_IDENT);
                assert(cast(Ast_Ident *, binary->right)->name == sl("count"));
                bool is_pointer = left_type.tag == POINTER;
                c_write("(sizeof(%s", is_pointer ? "*(" : "");
                write_ast(binary->left);
                c_write("%s)/sizeof((%s", is_pointer ? ")" : "", is_pointer ? "*(" : "");
                write_ast(binary->left);
                c_write("%s)[0]))", is_pointer ? ")" : "");
                return;
            }
            
            s32 binary_prec = get_c_binary_precedence(binary->tag);
            
            bool left_needs_parens  = false;
            
            if (binary->left->ast_tag == AST_BINARY) {
                auto l_prec = get_c_binary_precedence(cast(Ast_Binary *, binary->left)->tag);
                if (l_prec < binary_prec) { // < on left cause left-associativity
                    left_needs_parens = true;
                }
            } else if (binary->left->ast_tag == AST_UNARY) {
                auto l_prec = 12;
                if (l_prec < binary_prec) {
                    left_needs_parens = true;
                }
            }
            
            if (left_needs_parens) c_write("(");
            write_ast(binary->left);
            if (left_needs_parens) c_write(")");
            
            //assert(!is_number_type(binary->data_type.kind) || binary->data_type.number.is_solid);
            
            switch (binary->tag) {
                using enum Binary_Tag;
                // 'after' operators
                Case DOT: {
                    if (left_type.tag == POINTER) {
                        assert(left_type.pointer.to->tag == STRUCT || left_type.pointer.to->tag == STRING);
                        c_write("->");
                    } else {
                        assert(left_type.tag == STRUCT || left_type.tag == STRING);
                        c_write(".");
                    }
                    assert(binary->right->ast_tag == AST_IDENT);
                    auto ident = cast(Ast_Ident *, binary->right);
                    c_write("%S", tprint_ident(ident));
                    return;
                }
                
                Case ARRAY_ACCESS: {
                    auto array_type = get_data_type(binary->left);
                    if (array_type.tag == Data_Type_Tag::STRING) {
                        c_write(".data");
                    }
                    c_write("[");
                    write_ast(binary->right);
                    c_write("]");
                    return;
                }
                
                // 'between' operators
                Case ADD:    c_write(" + ");
                Case SUB:    c_write(" - ");
                Case MUL:    c_write("*");
                Case DIV:    c_write("/");
                Case MOD:    c_write(" %% ");
                
                Case GREATER_THAN: c_write(" > ");
                Case LESS_THAN:    c_write(" < ");
                
                Case BITWISE_AND: c_write(" & ");
                Case BITWISE_OR:  c_write(" | ");
                Case BITWISE_XOR: c_write(" ^ ");
                
                Case LEFT_SHIFT:  c_write(" << ");
                Case RIGHT_SHIFT: c_write(" >> ");
                
                Case IS_EQUAL:              c_write(" == ");
                Case NOT_EQUAL:             c_write(" != ");
                Case GREATER_THAN_OR_EQUAL: c_write(" >= ");
                Case LESS_THAN_OR_EQUAL:    c_write(" <= ");
                Case LOGICAL_AND:           c_write(" && ");
                Case LOGICAL_OR:            c_write(" || ");
                
                InvalidDefaultCase("Unhandled binary type");
            }
            
            bool right_needs_parens = false;
            // Doesn't matter if it's array access ([] act as parens and it rets early) or dot (right is always ident)
            if (binary->right->ast_tag == AST_BINARY) {
                auto r_prec = get_c_binary_precedence(cast(Ast_Binary *, binary->right)->tag);
                if (r_prec <= binary_prec) { // <= on right cause left-associativity
                    right_needs_parens = true;
                }
            } else if (binary->right->ast_tag == AST_UNARY) {
                auto r_prec = 12;
                if (r_prec < binary_prec) {
                    right_needs_parens = true;
                }
            }
            
            if (right_needs_parens) c_write("(");
            write_ast(binary->right);
            if (right_needs_parens) c_write(")");;
        }
        
        InvalidDefaultCase("Unhandled ast type");
    }
#endif
}

internal void
write_block_or_case_statements(Ast_Block_Statement_Node *node) {
    if (!node) return;
    while (true) {
        ForI (j, 0, node->statement->preceding_whitespace_and_comments.count) {
            c_write("\n");
        }
        write_indent();
        //write_newline_and_indent();
        write_statement(node->statement);
        if (!node->next) break;
        node = node->next;
        //write_newline_and_indent();
    }
}

internal void
write_parameters(Ast_Parameter_Node *param) {
    if (param) while (true) {
        write_type(param->decl.ident.data_type, &param->decl.ident);
        if (param->decl.initialization) {
            c_write("/* = ");
            write_ast(param->decl.initialization);
            c_write("*/");
        }
        param = param->next;
        if (!param) break;
        c_write(", ");
    }
}

internal void
write_statement(Ast *ast) {
#if C_WRITER_ENABLED
    if (!ast) {
        c_write("(***NULL_AST!!!***)");
        assert(!"Null ast");
        return;
    }
#if 0
    if (ast->ast_tag == AST_CASE) {
    } else if (ast->ast_tag == AST_BLOCK_STATEMENT_NODE) {
        c_write("%S", (cast(Ast_Block_Statement_Node *, ast))->statement->preceding_whitespace_and_comments);
    } else {
        c_write("%S", ast->preceding_whitespace_and_comments);
    }
#endif
    switch (ast->ast_tag) {
        Case AST_DECL: {
            auto decl = cast(Ast_Decl *, ast);
            if (decl->using_kind == Ast_Decl::ANON_USING) {
                assert(decl->ident.data_type.tag == Data_Type_Tag::STRUCT);
                auto struct_union_enum = decl->ident.data_type.structure.struct_union_enum;
                if (struct_union_enum == Struct_Kind::STRUCT) {
                    c_write("struct {");
                } else if (struct_union_enum == Struct_Kind::UNION) {
                    c_write("union {");
                } else if (struct_union_enum == Struct_Kind::ENUM) {
                    assert(!"enum isn't implemented yet");
                    c_write("enum {");
                } else assert(!"Unhandled using type in c_writer");
                g_nesting += 1;
                
                Ast_Explicitly_Uninitalize uninit; // Gets default values
                auto member = decl->ident.data_type.structure.first_member;
                while (true) {
                    write_newline_and_indent();
                    auto old_init = member->decl.initialization;
                    member->decl.initialization = &uninit; // @Hack
                    write_statement(&member->decl);
                    member->decl.initialization = old_init;
                    if (!member->next) break;
                    member = member->next;
                }
                g_nesting -= 1;
                write_newline_and_indent();
                c_write("};");
                
            } else if (decl->initialization && decl->initialization->ast_tag == AST_PROC_DEFN) {
                assert(decl->ident.flags & Ast_Ident::CONSTANT);
                auto proc = cast(Ast_Proc_Defn *, decl->initialization);
                //if (decl->ident.variable_index != chai.main_proc_variable_index) c_write("static ");
                //write_return_type(proc->signature.type.proc.first_return_type_expr);
                Str linkage = (decl->ident.flags & Ast_Ident::IS_EXPORT) ? sl("") : sl("static ");
                auto node = proc->signature.type.proc.first_return_type_expr;
                if (node && node->next) {
                    c_write("typedef struct{");
                    int i = 0;
                    Ast_Ident ident; // :GetsDefaultValues
                    while (true) {
                        ident.name = tprint("_%x", i);
                        write_type(node->type, &ident);
                        c_write(";");
                        if (!node->next) break;
                        node = node->next;
                        i += 1;
                    }
                    assert(proc->signature.type.proc.multi_return_index > 0);
                    c_write("} _%x; %S_%x", proc->signature.type.proc.multi_return_index, linkage, proc->signature.type.proc.multi_return_index);
                } else if (node) {
                    c_write("%S", linkage);
                    write_type(node->type, null);
                } else {
                    c_write("%Svoid", linkage);
                }
                c_write(" %S(", tprint_ident(&decl->ident));
                write_parameters(proc->signature.type.proc.first_param);
                c_write(") ");
                
                write_statement(&proc->body);
            } else if (decl->initialization && decl->initialization->ast_tag == AST_TYPE_EXPR) {
                assert(decl->ident.flags & Ast_Ident::CONSTANT);
                if (decl->ident.flags & Ast_Ident::IS_C_DECL) {
                    auto signature = cast(Ast_Type_Expr *, decl->initialization);
                    assert(signature->type.tag == Data_Type_Tag::PROC);
                    assert(decl->ident.flags & Ast_Ident::CONSTANT);
                    
                    assert(signature->type.proc.multi_return_index == 0);
                    write_return_type(signature->type.proc);
                    c_write(" %S(", tprint_ident(&decl->ident));
                    write_parameters(signature->type.proc.first_param);
                    c_write(");");
                } else {
                    auto old_type_expr = cast(Ast_Type_Expr *, decl->initialization);
                    c_write("typedef ");
                    if (old_type_expr->type.tag == Data_Type_Tag::STRUCT) {
                        //write_type(old_type_expr->type, &decl->ident);
                        // @Copypaste: write_type
                        c_write("struct {");
                        g_nesting += 1;
                        
                        Ast_Explicitly_Uninitalize uninit; // Gets default values
                        auto member = old_type_expr->type.structure.first_member;
                        while (true) {
                            write_newline_and_indent();
                            auto old_init = member->decl.initialization;
                            member->decl.initialization = &uninit; // @Hack
                            write_statement(&member->decl);
                            member->decl.initialization = old_init;
                            if (!member->next) break;
                            member = member->next;
                        }
                        g_nesting -= 1;
                        write_newline_and_indent();
                        c_write("} %S", tprint_ident(&decl->ident));
                    } else if (old_type_expr->type.tag == Data_Type_Tag::PROC) {
                        write_type(old_type_expr->type, &decl->ident);
                    } else {
                        write_type_no_name(old_type_expr->type);
                        c_write(" %S", tprint_ident(&decl->ident));
                    }
                    c_write(";");
                }
            } else {
#if 0
                // :MultiReturnRefactor
                if (decl->ident.next) {
                    auto _return = cast(Ast_Return *, ast);
                    assert(decl->initialization && decl->initialization->ast_tag == AST_CALL);
                    auto call = cast(Ast_Call *, decl->initialization);
                    auto call_type = get_data_type(call);
                    assert(call_type.tag == Data_Type_Tag::PROC);
                    
                    assert(call_type.proc.multi_return_index > 0);
                    auto index = call_type.proc.multi_return_index;
                    c_write("_%x __%x = ", index, index);
                    write_ast(decl->initialization);
                    c_write(";");
                    
                    auto node = &decl->ident;
                    int i = 0;
                    while (true) {
                        write_type(node->data_type, node);
                        c_write("=__%x._%x;", index, i);
                        if (!node->next) break;
                        node = node->next;
                        i += 1;
                    }
                } else 
#endif
                {
                    if (decl->ident.flags & Ast_Ident::CONSTANT) {
                        auto data_type = chai.variables[decl->ident.variable_index].ident->data_type;
                        if (is_number_type(data_type.tag) && !data_type.number.is_solid) {
                            c_write("#define %S ", tprint_ident(&decl->ident));
                            assert(decl->initialization);
                            assert(decl->initialization->ast_tag != AST_EXPLICITLY_UNINITALIZE);
                            write_ast(decl->initialization);
                            break;
                        }
                        c_write("const ");
                    }
                    write_type(decl->ident.data_type, &decl->ident);
                    if (decl->initialization) {
                        if (decl->initialization->ast_tag != AST_EXPLICITLY_UNINITALIZE) {
                            c_write(" = ");
                            write_ast(decl->initialization);
                        }
                    } else {
                        c_write(" = {0}");
                    }
                    c_write(";");
                }
            }
        }
        
        Case AST_BRANCH: {
            auto branch = cast(Ast_Branch *, ast);
            c_write("if (");
            write_ast(branch->condition);
            c_write(") ");
            write_statement(branch->true_statement);
            
            if (branch->false_statement) {
                if (branch->true_statement->ast_tag == AST_BLOCK) {
                    c_write(" else ");
                } else {
                    write_newline_and_indent();
                    c_write("else ");
                }
                write_statement(branch->false_statement);
            }
        }
        
        Case AST_STATIC_BRANCH: {
            auto branch = cast(Ast_Static_Branch *, ast);
            if (branch->condition_is_true) {
                if (branch->true_statement->ast_tag == AST_BLOCK) {
                    write_block_or_case_statements(cast(Ast_Block *, branch->true_statement)->first_statement);
                } else {
                    write_statement(branch->true_statement);
                }
            } else if (branch->false_statement) {
                if (branch->false_statement->ast_tag == AST_BLOCK) {
                    write_block_or_case_statements(cast(Ast_Block *, branch->false_statement)->first_statement);
                } else {
                    write_statement(branch->false_statement);
                }
            }
        }
        
        Case AST_BLOCK: {
            auto block = cast(Ast_Block *, ast);
            c_write("{");
            if (block->first_statement) {
                g_nesting += 1;
                write_block_or_case_statements(block->first_statement);
                g_nesting -= 1;
            }
            write_newline_and_indent();
            c_write("}");
        }
        
        Case AST_SWITCH: {
            auto _switch = cast(Ast_Switch *, ast);
#if 0
            c_write("switch (");
            write_ast(_switch->switch_expr);
            c_write(") {");
            g_nesting += 1;
            //write_statement(_switch->first_case);
            
            auto node = _switch->first_case;
            while (true) {
                if (node->expr) {
                    write_newline_and_indent();
                    c_write("case ");
                    write_ast(node->expr);
                    c_write(":");
                } else {
                    write_newline_and_indent();
                    c_write("default:");
                }
                if (node->first_statement) {
                    c_write(" {");
                    g_nesting += 1;
                    write_block_or_case_statements(node->first_statement);
                    g_nesting -= 1;
                    write_newline_and_indent();
                    c_write("}");
                }
                if (node->fall_through) c_write(" // through");
                else                    c_write(" break;");
                if (!node->next) break;
                c_write("\n"); // not write_newline_and_indent cause it's just an empty line
                node = node->next;
            }
            
            g_nesting -= 1;
            write_newline_and_indent();
            c_write("}");
#else
            // weird way to check that the default case isn't the only one:
            // if default is first then there be another case after it
            assert(_switch->first_case->expr != null || _switch->first_case->next);
            
            Ast_Case *default_case = null;
            bool default_was_last_case = false;
            bool last_was_through = false;
            auto node = _switch->first_case;
            while (true) {
                if (node->expr) {
                    if (last_was_through) {
                    } else {
                        c_write("if (");
                    }
                    c_write("(");
                    write_ast(_switch->switch_expr);
                    c_write(") == (");
                    write_ast(node->expr);
                    if (node->fall_through) {
                        assert(!node->first_statement);
                        assert(node->next);
                        c_write(") ||");
                        write_newline_and_indent();
                        c_write("   ");
                    } else {
                        c_write(")) {");
                        if (node->first_statement) {
                            g_nesting += 1;
                            write_block_or_case_statements(node->first_statement);
                            g_nesting -= 1;
                            write_newline_and_indent();
                        }
                        c_write("}");
                        if (node->next) {
                            c_write(" else ");
                        }
                    }
                } else {
                    assert(!last_was_through);
                    assert(!default_case);
                    assert(!node->fall_through);
                    default_case = node;
                    if (!node->next) default_was_last_case = true;
                }
                last_was_through = node->fall_through;
                
                if (!node->next) break;
                node = node->next;
            }
            if (default_case) {
                if (default_was_last_case) {
                    c_write("{");
                } else {
                    c_write(" else {");
                }
                if (default_case->first_statement) {
                    g_nesting += 1;
                    write_block_or_case_statements(default_case->first_statement);
                    g_nesting -= 1;
                    write_newline_and_indent();
                }
                c_write("}");
            }
#endif
        }
        
        Case AST_LOOP: {
            auto loop = cast(Ast_Loop *, ast);
            c_write("while (");
            write_ast(loop->condition);
            c_write(") ");
            write_statement(loop->statement);
        }
        
        Case AST_FOR_COUNT: {
            auto loop = cast(Ast_For_Count *, ast);
            auto it_type = get_data_type(&loop->it);
            auto it_ident = tprint_ident(&loop->it);
            
            c_write("for (");
            write_type_no_name(it_type);
            c_write(" %S = ", it_ident);
            if (loop->reverse) {
                write_ast(loop->end);
                c_write("; %S >= ", it_ident);
                write_ast(loop->end);
                c_write("; %S-=1) ", it_ident);
            } else {
                write_ast(loop->start);
                c_write("; %S <= ", it_ident);
                write_ast(loop->end);
                c_write("; ++%S) ", it_ident);
            }
            write_statement(loop->statement);
        }
        
        Case AST_FOR_EACH: {
            auto loop = cast(Ast_For_Each *, ast);
            auto it_type = get_data_type(&loop->it);
            auto it_index_ident = tprint_ident(&loop->it_index);
            
            if (loop->reverse) {
                c_write("for (int %S = (sizeof(", it_index_ident);
                write_ast(loop->range);
                c_write(")/sizeof((");
                write_ast(loop->range);
                c_write(")[0]))-1; %S >= 0; --%S) {", it_index_ident, it_index_ident);
            } else {
                c_write("for (int %S = 0; %S < (sizeof(", it_index_ident, it_index_ident);
                write_ast(loop->range);
                c_write(")/sizeof((");
                write_ast(loop->range);
                c_write(")[0])); ++%S) {", it_index_ident);
            }
            g_nesting += 1;
            write_newline_and_indent();
            
            write_type(it_type, &loop->it);
            if (loop->pointer) {
                c_write(" = &(");
                write_ast(loop->range);
                c_write("[%S]);", tprint_ident(&loop->it_index));
            } else {
                c_write(" = ");
                write_ast(loop->range);
                c_write("[%S];", tprint_ident(&loop->it_index));
            }
            if (loop->statement->ast_tag == AST_BLOCK) {
                write_block_or_case_statements(cast(Ast_Block *, loop->statement)->first_statement);
            } else {
                write_newline_and_indent();
                write_statement(loop->statement);
            }
            
            g_nesting -= 1;
            write_newline_and_indent();
            c_write("}");
        }
        
        Case AST_RETURN: {
#if 1
            auto _return = cast(Ast_Return *, ast);
            if (_return->first) {
                c_write("return");
                auto node = _return->first;
                if (node->next) {
                    assert(_return->multi_return_index > 0);
                    c_write("(_%x){", _return->multi_return_index);
                    while (true) {
                        write_ast(node->expr);
                        if (!node->next) break;
                        c_write(", ");
                        node = node->next;
                    }
                    c_write("}");
                } else {
                    c_write(" ");
                    write_ast(node->expr);
                }
            }
            c_write(";");
#else
            auto _return = cast(Ast_Return *, ast);
            if (_return->first) {
                auto node = _return->first;
                if (node->next) {
                    assert(_return.multi_return_index > 0);
                    auto index = _return->multi_return_index;
                    c_write("{_%x __%x = {", index, index);
                    while (true) {
                        write_ast(node->expr);
                        if (!node->next) break;
                        c_write(", ");
                        node = node->next;
                    }
                    c_write("}; return __%x;}", index);
                } else {
                    c_write("return ");
                    write_ast(node->expr);
                    c_write(";");
                }
            } else {
                c_write("return;");
            }
#endif
        }
        
        Case AST_BREAK: {
            c_write("break;");
        }
        
        Case AST_CONTINUE: {
            c_write("continue;");
        }
        
        Case AST_ASSIGNMENT: {
            auto assign = cast(Ast_Assignment *, ast);
            write_ast(assign->left);
            
            switch (assign->tag) {
                using enum Assign_Tag;
                Case NORMAL: c_write(" = ");
                
                Case ADD: c_write(" += ");
                Case SUB: c_write(" -= ");
                Case MUL: c_write(" *= ");
                Case DIV: c_write(" /= ");
                Case MOD: c_write(" %%= ");
                
                Case BITWISE_AND: c_write(" &= ");
                Case BITWISE_OR:  c_write(" |= ");
                Case BITWISE_XOR: c_write(" ^= ");
                
                Case LEFT_SHIFT:  c_write(" <<= ");
                Case RIGHT_SHIFT: c_write(" >>= ");
                
                InvalidDefaultCase("Unhandled assign type");
            }
            
            write_ast(assign->right);
            c_write(";");
        }
        
        Case AST_LOAD: {
            c_write("// #load %S", cast(Ast_Load *, ast)->file->s);
        }
        
        Case AST_CALL: {
            write_ast(ast);
            c_write(";");
        }
        
        Case AST_UNARY: {
            write_ast(ast);
            c_write("; // Statement is just a unary operation");
        }
        
        Case AST_BINARY: {
            write_ast(ast);
            c_write("; // Statement is just a binary operation");
        }
        
        Case AST_SIZEOF: {
            write_ast(ast);
            c_write("; // Statement is just a sizeof");
        }
        
        Case AST_IDENT: {
            write_ast(ast);
            c_write("; // Statement is just an identifier");
        }
        
        Case AST_LITERAL: {
            write_ast(ast);
            c_write("; // Statement is just a literal");
        }
        
        InvalidDefaultCase("Unhandled ast type");
    }
#endif
}

global char C_FILE_HEADER[] = R"_CFILEHEADER_(
typedef   signed long long s64;
typedef   signed int       s32;
typedef   signed short     s16;
typedef   signed char      s8;

typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned short     u16;
typedef unsigned char      u8;

typedef double f64;
typedef float  f32;

typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(s64) == 8) ? 1 : -1];
typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(s32) == 4) ? 1 : -1];
typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(s16) == 2) ? 1 : -1];
           typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(s8 ) == 1) ? 1 : -1];

typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(u64) == 8) ? 1 : -1];
typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(u32) == 4) ? 1 : -1];
typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(u16) == 2) ? 1 : -1];
typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(u8 ) == 1) ? 1 : -1];

typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(f64) == 8) ? 1 : -1];
typedef char __________Internal_Compile_Time_Assert_Failed_________[(sizeof(f32) == 4) ? 1 : -1];

typedef struct {
    s64 count;
                 u8 *data;
} string;

#define false (0)
#define true (!false)
#define null (0)

)_CFILEHEADER_";

internal void
write_c_file(Array<Ast *> statements) {
    bool success = create_empty_file(chai.fully_pathed_output_file_name.data);
    assert(success);
    
    Str c_file_header = {};
    c_file_header.count = ArrayCount(C_FILE_HEADER)-1;
    c_file_header.data = C_FILE_HEADER;
    //success = append_to_existing_file(C_FILE_PATH, c_file_header);
    //assert(success);
    c_write("%s", c_file_header.data);
    
    ForI (i, 0, statements.count) {
        Ast *statement = statements[i];
        ForI (j, 0, statement->preceding_whitespace_and_comments.count) {
            c_write("\n");
        }
        write_statement(statement);
    }
    
    // Flush what's left in the buffer to the file
    auto data = Str{c_buffer_used, c_buffer};
    success = append_to_existing_file(chai.fully_pathed_output_file_name.data, data);
    assert(success);
}