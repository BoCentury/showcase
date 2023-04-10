
enum Report_Kind {
    REPORT_NOLABEL,
    REPORT_ERROR,
    REPORT_INFO,
};

internal void
__report_code_lines(Str relevant_lines, s32 l0, s32 c0, s32 l1, s32 c1) {
    assert(l0 > 0 && l1 > 0 && c0 > 0 && c1 > 0);
    assert((relevant_lines.count == 0 || (relevant_lines.count > 0 && relevant_lines.data != null)));
    auto line_1 = consume_next_line(&relevant_lines);
    auto line_2 = consume_next_line(&relevant_lines);
    auto line_3 = relevant_lines;
    if (line_3.count == 0) {
        line_3 = line_2;
        line_2 = line_1;
        line_1 = {};
        if (line_3.count == 0) {
            line_3 = line_2;
            line_2 = {};
        }
    }
    assert(line_3.data && line_3.count > 0);
    assert(index_of_first_char(line_3, '\n') == -1);
    
    if (chai.console_can_output_colors) {
        assert(c0 > 0 && c0 <= line_3.count);
        assert(c1 > 0 && c1 <= line_3.count);
        auto before = Str{ .count = c0-1, .data = line_3.data };
        line_3 = advance(line_3, before.count);
        
        auto highlight = Str{ .count = c1 - c0 + 1, .data = line_3.data };
        auto after = advance(line_3, highlight.count);
        
        print("%S\n"
              "%S\n"
              "%S" "\x1b[31m" "%S" "\x1b[0m" "%S\n\n\n", line_1, line_2, before, highlight, after);
    } else {
        print("%S\n%S\n%S\n", line_1, line_2, line_3);
        ForII (i, 1, c0-1) print(" ");
        ForII (i, c0, c1)  print("^");
        print("\n\n");
    }
}

internal void
report_info_noloc(char *format, ...) {
    Scoped_Arena scratch;
    va_list args;
    va_start(args, format);
    auto message = arena_vprint(scratch, format, args);
    va_end(args);
    
    print("Info: %S\n\n", message);
}

internal void
report_error_noloc(char *format, ...) {
    if (chai.error_was_reported) return;
    Scoped_Arena scratch;
    va_list args;
    va_start(args, format);
    auto message = arena_print(scratch, format, args);
    va_end(args);
    
    print("Info: %S\n\n", message);
    
    chai.error_was_reported = true;
}

internal void
__report_base_proc(Report_Kind kind, Str fully_pathed_source_file, Str relevant_lines, s32 l0, s32 c0, s32 l1, s32 c1, char *format, va_list args) {
    assert(l0 > 0 && l1 > 0 && c0 > 0 && c1 > 0);
    assert((relevant_lines.count == 0 || (relevant_lines.count > 0 && relevant_lines.data != null)));
    Str label = {};
    if (kind == REPORT_INFO) {
        label = sl("Info: ");
    } else if (kind == REPORT_ERROR) {
        if (chai.error_was_reported) return;
        chai.error_was_reported = true;
        label = sl("Error: ");
    }
    
    Scoped_Arena scratch;
    auto message = arena_vprint(scratch, format, args);
    
    print("%S(%d,%d): %S%S\n\n", fully_pathed_source_file, l0, c0, label, message);
    
    if (relevant_lines.count > 0) {
        __report_code_lines(relevant_lines, l0, c0, l1, c1);
    }
}

internal inline void
__report_base_proc(Report_Kind kind, Code_Location loc, char *format, va_list args) {
    __report_base_proc(kind, loc.fully_pathed_source_file, loc.relevant_lines, loc.l0, loc.c0, loc.l1, loc.c1, format, args);
}

internal inline void
report_info(Str fully_pathed_source_file, Str relevant_lines, s32 l0, s32 c0, s32 l1, s32 c1, char *format, ...) {
    va_list args;
    va_start(args, format);
    __report_base_proc(REPORT_INFO, fully_pathed_source_file, relevant_lines, l0, c0, l1, c1, format, args);
    va_end(args);
}

internal inline void
report_error(Str fully_pathed_source_file, Str relevant_lines, s32 l0, s32 c0, s32 l1, s32 c1, char *format, ...) {
    va_list args;
    va_start(args, format);
    __report_base_proc(REPORT_ERROR, fully_pathed_source_file, relevant_lines, l0, c0, l1, c1, format, args);
    va_end(args);
}

internal inline void
report_info(Code_Location loc, char *format, ...) {
    va_list args;
    va_start(args, format);
    __report_base_proc(REPORT_INFO, loc, format, args);
    va_end(args);
}

internal inline void
report_error(Code_Location loc, char *format, ...) {
    va_list args;
    va_start(args, format);
    __report_base_proc(REPORT_ERROR, loc, format, args);
    va_end(args);
}

internal inline void
report_info(Token token, char *format, ...) {
    assert(token.text.count == (token.loc.c1 - (token.loc.c0-1)));
    va_list args;
    va_start(args, format);
    __report_base_proc(REPORT_INFO, token.loc, format, args);
    va_end(args);
}

internal inline void
report_error(Token token, char *format, ...) {
    assert(token.text.count == (token.loc.c1 - (token.loc.c0-1)));
    va_list args;
    va_start(args, format);
    __report_base_proc(REPORT_ERROR, token.loc, format, args);
    va_end(args);
}

internal inline void
report_error(Str fully_pathed_source_file, Str relevant_lines, s32 l0, s32 c0, char *format, ...) {
    va_list args;
    va_start(args, format);
    __report_base_proc(REPORT_ERROR, fully_pathed_source_file, relevant_lines, l0, c0, l0, c0, format, args);
    va_end(args);
}

internal bool
is_c_keyword(Str s) {
    if (s.count > 0 && s.data[0] == '_') {
        auto mse = advance(s, (s.count > 1 && s.data[1] == '_') ? 2 : 1);
        // Microsoft extensions, the _ version expands to the __ version
        switch (mse.count) {
            Case 3: {
                if (strings_match_unchecked(mse, sl("asm"))) return true;
                if (strings_match_unchecked(mse, sl("try"))) return true;
            }
            Case 4: {
                if (strings_match_unchecked(mse, sl("int8"))) return true;
                
            }
            Case 5: {
                if (strings_match_unchecked(mse, sl("based"))) return true;
                if (strings_match_unchecked(mse, sl("cdecl"))) return true;
                if (strings_match_unchecked(mse, sl("int16"))) return true;
                if (strings_match_unchecked(mse, sl("int32"))) return true;
                if (strings_match_unchecked(mse, sl("int64"))) return true;
                if (strings_match_unchecked(mse, sl("leave"))) return true;
            }
            Case 6: {
                if (strings_match_unchecked(mse, sl("except"))) return true;
                if (strings_match_unchecked(mse, sl("inline"))) return true;
            }
            Case 7: {
                if (strings_match_unchecked(mse, sl("finally"))) return true;
                if (strings_match_unchecked(mse, sl("stdcall"))) return true;
            }
            Case 8: {
                if (strings_match_unchecked(mse, sl("declspec"))) return true;
                if (strings_match_unchecked(mse, sl("fastcall"))) return true;
                if (strings_match_unchecked(mse, sl("restrict"))) return true;
            }
        }
    }
    
    // unmarked: base C keywords
    switch (s.count) {
        //Case 2: {
        //if (strings_match_unchecked(s, sl("if"))) return true;
        //if (strings_match_unchecked(s, sl("do"))) return true; // moved to default
        //}
        //Case 3: {
        //if (strings_match_unchecked(s, sl("for"))) return true;
        //if (strings_match_unchecked(s, sl("int"))) return true;
        //}
        Case 4: {
            //if (strings_match_unchecked(s, sl("case"))) return true;
            //if (strings_match_unchecked(s, sl("else"))) return true;
            //if (strings_match_unchecked(s, sl("enum"))) return true;
            //if (strings_match_unchecked(s, sl("void"))) return true;
            if (strings_match_unchecked(s, sl("auto"))) return true;
            if (strings_match_unchecked(s, sl("char"))) return true;
            if (strings_match_unchecked(s, sl("goto"))) return true;
            if (strings_match_unchecked(s, sl("long"))) return true;
        }
        Case 5: {
            //if (strings_match_unchecked(s, sl("break"))) return true;
            //if (strings_match_unchecked(s, sl("union"))) return true;
            //if (strings_match_unchecked(s, sl("while"))) return true;
            if (strings_match_unchecked(s, sl("const"))) return true;
            if (strings_match_unchecked(s, sl("float"))) return true;
            if (strings_match_unchecked(s, sl("short"))) return true;
            if (strings_match_unchecked(s, sl("_Bool"))) return true; // [C11][MSe]
        }
        Case 6: {
            //if (strings_match_unchecked(s, sl("inline"))) return true;
            //if (strings_match_unchecked(s, sl("return"))) return true;
            //if (strings_match_unchecked(s, sl("struct"))) return true;
            if (strings_match_unchecked(s, sl("double"))) return true;
            if (strings_match_unchecked(s, sl("extern"))) return true;
            if (strings_match_unchecked(s, sl("signed"))) return true;
            if (strings_match_unchecked(s, sl("sizeof"))) return true;
            if (strings_match_unchecked(s, sl("static"))) return true;
            if (strings_match_unchecked(s, sl("switch"))) return true;
        }
        Case 7: {
            if (strings_match_unchecked(s, sl("default"))) return true;
            if (strings_match_unchecked(s, sl("typedef"))) return true;
            if (strings_match_unchecked(s, sl("_Pragma"))) return true; // [MSe]
        }
        Case 8: {
            //if (strings_match_unchecked(s, sl("continue"))) return true;
            if (strings_match_unchecked(s, sl("register"))) return true;
            if (strings_match_unchecked(s, sl("restrict"))) return true;
            if (strings_match_unchecked(s, sl("unsigned"))) return true;
            if (strings_match_unchecked(s, sl("volatile"))) return true;
            if (strings_match_unchecked(s, sl("_Alignof"))) return true; // [C11][MSe]
        }
        Default: {
            if (strings_match(s, sl("do"))) return true;
            if (strings_match(s, sl("static_assert"))) return true; // [MSe][C11]
        }
    }
    return false;
    
    /*
    
        // -- base C keywords, can't use:
    
        do if
        for int
        auto case char else enum goto long void
        break const float short union while
        double extern inline return signed sizeof static struct switch
        default typedef
        continue register restrict unsigned volatile
        
xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx    
xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx    
xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx
xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx
xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx
xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx
xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx
xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx
xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx
xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
xxxxxxx xxxxxxx xxxxxxx xxxxxxx xxxxxxx xxxxxxx xxxxxxx
xxxxxx xxxxxx xxxxxx xxxxxx xxxxxx xxxxxx xxxxxx
xxxx xxxx xxxx xxxx xxxx xxxx xxxx
xxx xxx xxx xxx xxx xxx xxx
  xx xx xx xx xx xx xx
x x x x x x x
xx xx xx xx xx xx xx
xxx xxx xxx xxx xxx xxx xxx
xxxx xxxx xxxx xxxx xxxx xxxx xxxx
  xxxxx xxxxx xxxxx xxxxx xxxxx xxxxx xxxxx
xxxxxx xxxxxx xxxxxx xxxxxx xxxxxx xxxxxx xxxxxx
xxxxxxx xxxxxxx xxxxxxx xxxxxxx xxxxxxx xxxxxxx xxxxxxx
xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx
xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx xxxxxxxxxx
xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx xxxxxxxxxxx
xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx xxxxxxxxxxxx
xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx xxxxxxxxxxxxx
xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx xxxxxxxxxxxxxx
xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx xxxxxxxxxxxxxxx
xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxx    
xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxx    
xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxx
        
        // -- C macros:
    
        // only exist if we #include the corresponding headers, can use as identifiers (except static_assert cuz MS extensions):
        bool
        alignas alignof complex
        noreturn
        imaginary
        atomic_bool
        thread_local
        static_assert
        
        
        // -- C99/11/17 keywords, most of them aren't used expect for the ones added with MS extensions
    
        // MS extensions, can't use as identifiers:
        _Bool
        _Alignof
        
        // can use as identifiers:
        _Atomic
        _Alignas _Complex _Generic
        _Noreturn
        _Imaginary _Decimal32 _Decimal64
        _Decimal128
        _Thread_local
        _Static_assert
        
        // C23 keywords that are definitely not gonna be implemented anytime soon, can use as identifiers:
        _Decimal128 _Decimal32 _Decimal64
        
        
        // -- Microsoft specific: [MSe]
    
        // For compatibility with previous versions, these keywords are available both with two leading underscores and a single leading underscore when Microsoft extensions are enabled: (they are for us, so can't use as identifiers)
        __asm __try
        __int8
        __based __cdecl __int16 __int32 __int64 __leave
        __except __inline
        __finally __stdcall
        __declspec __fastcall __restrict
        static_assert
        
        _asm _try
        _int8
        _based _cdecl _int16 _int32 _int64 _leave
        _except _inline
        _finally _stdcall
        _declspec _fastcall _restrict
        
        // these are only used in __declspec so they're free game, can use as identifiers:
        naked
        thread
        dllexport dllimport
        
        // The following tokens are recognized by the preprocessor when they are used outside the context of a preprocessor directive:
        // can't use as identifier:
        _Pragma
        
        // The following additional keywords are classified as extensions and conditionally-supported:
        // They don't conflict on this compiler, can use as identifiers:
        asm
        fortran
        
        // The Microsoft C compiler allows 247 characters in an internal or external identifier name. If you aren't concerned with ANSI compatibility, you can modify this default to a smaller or larger number using the /H (restrict length of external names) option.
    
        // Also, each name that begins with a double underscore __ or an underscore followed by an uppercase letter is reserved
    
    */
}

internal Token_Tag
get_directive_tag(Str s) {
    switch (s.count) {
        Case 2: {
            if (strings_match_unchecked(s, sl("if"))) return TOKEN_DIRECTIVE_IF;
        }
        Case 4: {
            if (strings_match_unchecked(s, sl("type"))) return TOKEN_DIRECTIVE_TYPE;
            if (strings_match_unchecked(s, sl("load"))) return TOKEN_DIRECTIVE_LOAD;
            if (strings_match_unchecked(s, sl("must"))) return TOKEN_DIRECTIVE_MUST;
            if (strings_match_unchecked(s, sl("char"))) return TOKEN_DIRECTIVE_CHAR;
        }
        Case 6: {
            if (strings_match_unchecked(s, sl("c_decl"))) return TOKEN_DIRECTIVE_C_DECL;
            if (strings_match_unchecked(s, sl("expand"))) return TOKEN_DIRECTIVE_EXPAND;
            if (strings_match_unchecked(s, sl("export"))) return TOKEN_DIRECTIVE_EXPORT;
        }
        Case 7: {
            if (strings_match_unchecked(s, sl("through"))) return TOKEN_DIRECTIVE_THROUGH;
        }
    }
    return TOKEN_UNKNOWN;
}

internal Token_Tag
get_keyword_tag(Str s) {
    switch (s.count) {
        Case 2: {
            if (strings_match_unchecked(s, sl("if"))) return TOKEN_KEYWORD_IF;
            if (strings_match_unchecked(s, sl("s8"))) return TOKEN_KEYWORD_S8;
            if (strings_match_unchecked(s, sl("u8"))) return TOKEN_KEYWORD_U8;
        }
        Case 3: {
            if (strings_match_unchecked(s, sl("for"))) return TOKEN_KEYWORD_FOR;
            if (strings_match_unchecked(s, sl("int"))) return TOKEN_KEYWORD_INT;
            if (strings_match_unchecked(s, sl("s16"))) return TOKEN_KEYWORD_S16;
            if (strings_match_unchecked(s, sl("s32"))) return TOKEN_KEYWORD_S32;
            if (strings_match_unchecked(s, sl("s64"))) return TOKEN_KEYWORD_S64;
            if (strings_match_unchecked(s, sl("u16"))) return TOKEN_KEYWORD_U16;
            if (strings_match_unchecked(s, sl("u32"))) return TOKEN_KEYWORD_U32;
            if (strings_match_unchecked(s, sl("u64"))) return TOKEN_KEYWORD_U64;
            if (strings_match_unchecked(s, sl("f32"))) return TOKEN_KEYWORD_F32;
            if (strings_match_unchecked(s, sl("f64"))) return TOKEN_KEYWORD_F64;
        }
        Case 4: {
            if (strings_match_unchecked(s, sl("else"))) return TOKEN_KEYWORD_ELSE;
            if (strings_match_unchecked(s, sl("case"))) return TOKEN_KEYWORD_CASE;
            if (strings_match_unchecked(s, sl("cast"))) return TOKEN_KEYWORD_CAST;
            if (strings_match_unchecked(s, sl("enum"))) return TOKEN_KEYWORD_ENUM;
            if (strings_match_unchecked(s, sl("null"))) return TOKEN_KEYWORD_NULL;
            if (strings_match_unchecked(s, sl("void"))) return TOKEN_KEYWORD_VOID;
            if (strings_match_unchecked(s, sl("true"))) return TOKEN_KEYWORD_TRUE;
            if (strings_match_unchecked(s, sl("bool"))) return TOKEN_KEYWORD_BOOL;
        }
        Case 5: {
            if (strings_match_unchecked(s, sl("while"))) return TOKEN_KEYWORD_WHILE;
            if (strings_match_unchecked(s, sl("break"))) return TOKEN_KEYWORD_BREAK;
            if (strings_match_unchecked(s, sl("union"))) return TOKEN_KEYWORD_UNION;
            if (strings_match_unchecked(s, sl("false"))) return TOKEN_KEYWORD_FALSE;
            if (strings_match_unchecked(s, sl("using"))) return TOKEN_KEYWORD_USING;
        }
        Case 6: {
            if (strings_match_unchecked(s, sl("return"))) return TOKEN_KEYWORD_RETURN;
            if (strings_match_unchecked(s, sl("sizeof"))) return TOKEN_KEYWORD_SIZEOF;
            if (strings_match_unchecked(s, sl("typeof"))) return TOKEN_KEYWORD_TYPEOF;
            if (strings_match_unchecked(s, sl("struct"))) return TOKEN_KEYWORD_STRUCT;
            if (strings_match_unchecked(s, sl("inline"))) return TOKEN_KEYWORD_INLINE;
            if (strings_match_unchecked(s, sl("string"))) return TOKEN_KEYWORD_STRING;
        }
        Default: {
            if (strings_match(s, sl("continue")))   return TOKEN_KEYWORD_CONTINUE;
            if (strings_match(s, sl("enum_flags"))) return TOKEN_KEYWORD_ENUM_FLAGS;
        }
    }
    return TOKEN_UNKNOWN;
}

internal Token
lex_token(Lexer *lexer) {
    auto line = lexer->current_line;
    Token token = {};
    
    auto line_number = lexer->previous_tokens_l1;
    
    // first interation: i = number char in line up until the end of last token + number of noncode chars in the line since
    // every other iteration: i = number of noncode chars found from the start of the line
    
    int block_comment_nesting = 0;
    int line_index = (lexer->previous_tokens_c1+1)-1;
    token.preceding_whitespace_and_comments = Str{ .count = 0, .data = line.data+line_index };
    
    bool started_a_fresh_line = false;
    
    while (true) {
        if (block_comment_nesting != 0) {
            assert(block_comment_nesting > 0);
            line_index += 1; // because we're checking two characters at a time
            while (line_index < line.count) {
                auto char_0 = line.data[line_index-1];
                auto char_1 = line.data[line_index];
                if (char_0 == '/' && char_1 == '*') {
                    block_comment_nesting += 1;
                    line_index += 1;
                } else if (char_0 == '*' && char_1 == '/') {
                    block_comment_nesting -= 1;
                    line_index += 1;
                    if (block_comment_nesting <= 0) {
                        break;
                    }
                }
                line_index += 1;
            }
            
            if (line_index < line.count) continue; // if there's more characters in the line, continue
        } else {
            while (line_index < line.count) {
                auto c = line.data[line_index];
                if (!(c == ' ' || c == '\t')) {
                    if (c != '/') break;
                    if (line_index+1 >= line.count) break;
                    if (line.data[line_index+1] == '/') {
                        line_index = cast_s32(line.count);
                    } else if (line.data[line_index+1] == '*') {
                        line_index += 2; // for the "/*"
                        block_comment_nesting = 1;
                    }
                    break;
                }
                line_index += 1;
            }
            bool found_a_code_character = line_index != line.count && block_comment_nesting == 0;
            if (found_a_code_character) {
                assert(!(line_index > line.count));
                break;
            }
        }
        
        if (started_a_fresh_line) {
            lexer->whitespace_and_comments_line_count += 1;
        }
        started_a_fresh_line = true;
        
        bool that_was_the_last_line = lexer->rest_of_file.count <= 0;
        if (that_was_the_last_line) {
            assert(lexer->rest_of_file.count == 0);
            //token.loc = previous_token.loc;
            token.loc.relevant_lines = lexer->relevant_lines;
            token.loc.l0 = line_number;
            token.loc.l1 = line_number;
            token.loc.c0 = 1;
            token.loc.c1 = line_index+1;
            token.preceding_whitespace_and_comments.count = (line.data - token.preceding_whitespace_and_comments.data) + line.count;
            token.tag = TOKEN_END_OF_FILE;
            if (block_comment_nesting != 0) report_error(token.loc.fully_pathed_source_file,
                                                         token.loc.relevant_lines, token.loc.l0, token.loc.c0,
                                                         "File ended in the middle of a block-comment.");
            return token;
        }
        lexer->prev_prev_line_start = lexer->prev_line_start;
        lexer->prev_line_start      = lexer->current_line.data;
        lexer->current_line         = consume_next_line(&lexer->rest_of_file);
        lexer->relevant_lines.count = (lexer->current_line.data - lexer->prev_prev_line_start) + lexer->current_line.count;
        lexer->relevant_lines.data  = lexer->prev_prev_line_start;
        
        line = lexer->current_line;
        
        token.preceding_whitespace_and_comments.count += 1;
        
        line_number += 1;
        line_index = 0;
    }
    assert((line.data+line_index) >= token.preceding_whitespace_and_comments.data);
    //token.preceding_whitespace_and_comments.count = (line.data+line_index) - token.preceding_whitespace_and_comments.data;    
    assert(token.preceding_whitespace_and_comments.count >= 0);
    
    
    if (line.count <= 0) {
        return token;
    }
    auto token_start_index = line_index;
    
    token.loc.fully_pathed_source_file = lexer->fully_pathed_source_file_name;
    token.loc.relevant_lines = lexer->relevant_lines;
    token.loc.l0 = line_number;
    token.loc.c0 = token_start_index+1;
    token.loc.l1 = token.loc.l0;
    token.loc.c1 = token.loc.c0;
    token.text.data = line.data + line_index;
    
    //assert(line.data[token_start_index] != 0 && line.data[token_start_index+1] != 0 && line.data[token_start_index+2] != 0);
    
    
    auto char_0 = line.data[token_start_index];
    char char_1 = (token_start_index+1 < line.count) ? line.data[token_start_index+1] : 0;
    char char_2 = (token_start_index+2 < line.count) ? line.data[token_start_index+2] : 0;
    
    switch (char_0) {
        //OrCase '$':
        //OrCase '@':
        //OrCase '\\':
        //OrCase '`':
        Case   ',':
        OrCase ':':
        OrCase ';':
        OrCase '(':
        OrCase ')':
        OrCase '[':
        OrCase ']':
        OrCase '{':
        OrCase '}':
        OrCase '~': {
            token.tag = char_0;
            token.text.count = 1;
        }
        
        Case '.': {
            if (char_1 == '.') {
                //if (char_2 == '.') { token.text.count = 3; token.tag = TOKEN_TRIPLE_DOT;} // @InclusiveFor
                //else { token.text.count = 2; token.tag = TOKEN_DOUBLE_DOT; }
                token.text.count = 2; token.tag = TOKEN_DOUBLE_DOT;
            } else {
                token.text.count = 1; token.tag = char_0;
            }
        }
        
        Case '-': {
            if (char_1) {
                if (char_1 == '-') {
                    if (char_2 == '-') {
                        token.text.count = 3; token.tag = TOKEN_TRIPLE_MINUS;
                    } else {
                        token.text.count = 2; token.tag = TOKEN_DOUBLE_MINUS;
                    }
                } else if (char_1 == '>') {
                    token.text.count = 2; token.tag = TOKEN_ARROW;
                } else if (char_1 == '=') {
                    token.text.count = 2; token.tag = TOKEN_MINUS_EQUAL;
                } else {
                    token.text.count = 1; token.tag = '-';
                }
            } else {
                token.text.count = 1; token.tag = '-';
            }
        }
        
        Case '+': {
            if     (char_1 == '=') { token.text.count = 2; token.tag = TOKEN_PLUS_EQUAL; }
            else if (char_1 == '+') { token.text.count = 2; token.tag = TOKEN_DOUBLE_PLUS; }
            else                                     { token.text.count = 1; token.tag = '+'; }
        }
        
        Case '*': {
            if (char_1 == '=') { token.text.count = 2; token.tag = TOKEN_TIMES_EQUAL; }
            else                                { token.text.count = 1; token.tag = '*'; }
        }
        
        Case '%': {
            if (char_1 == '=') { token.text.count = 2; token.tag = TOKEN_MOD_EQUAL; }
            else                                { token.text.count = 1; token.tag = '%'; }
        }
        
        Case '=': {
            if (char_1 == '=') { token.text.count = 2; token.tag = TOKEN_IS_EQUAL; }
            else                                { token.text.count = 1; token.tag = '='; }
        }
        
        Case '!': {
            if (char_1 == '=') { token.text.count = 2; token.tag = TOKEN_NOT_EQUAL; }
            else                                { token.text.count = 1; token.tag = '!'; }
        }
        
        Case '<': {
            if (char_1 == '<') {
                if (char_2 == '=') {
                    token.text.count = 3; token.tag = TOKEN_LEFT_SHIFT_EQUAL;
                } else {
                    token.text.count = 2; token.tag = TOKEN_LEFT_SHIFT_OR_DEREFERENCE;
                }
            } else if (char_1 == '=') {
                token.text.count = 2; token.tag = TOKEN_LESS_EQUAL;
            } else {
                token.text.count = 1; token.tag = '<';
            }
        }
        
        Case '?': {
            token.text.count = 1; token.tag = TOKEN_LEFT_SHIFT_OR_DEREFERENCE;
        }
        
        Case '>': {
            if (char_1 == '>') {
                if (char_2 == '=') {
                    token.text.count = 3; token.tag = TOKEN_RIGHT_SHIFT_EQUAL;
                } else {
                    token.text.count = 2; token.tag = TOKEN_RIGHT_SHIFT;
                }
            } else if (char_1 == '=') {
                token.text.count = 2; token.tag = TOKEN_GREATER_EQUAL;
            } else {
                token.text.count = 1; token.tag = '>';
            }
        }
        
        Case '&': {
            if     (char_1 == '=') { token.text.count = 2; token.tag = TOKEN_BITWISE_AND_EQUAL; }
            else if (char_1 == '&') { token.text.count = 2; token.tag = TOKEN_LOGICAL_AND; }
            else                                     { token.text.count = 1; token.tag = '&'; }
        }
        
        Case '|': {
            if     (char_1 == '=') { token.text.count = 2; token.tag = TOKEN_BITWISE_OR_EQUAL; }
            else if (char_1 == '|') { token.text.count = 2; token.tag = TOKEN_LOGICAL_OR; }
            else                                     { token.text.count = 1; token.tag = '|'; }
        }
        
        Case '^': {
            if (char_1 == '=') { token.text.count = 2; token.tag = TOKEN_BITWISE_XOR_EQUAL; }
            else                                { token.text.count = 1; token.tag = '^'; }
        }
        
        //Case '\'': {}
        
        Case '"': {
            bool found_end = false;
            bool escaping = false;
            token.text.count = 1;
            ForI (it_index, token_start_index+1, line.count) {
                token.text.count += 1;
                auto it = line.data[it_index];
                if (it < ' ') {
                    if (it == '\r' || it == '\n') {
                        // :MultiLineStrings
                        assert(token.loc.c0 == token.loc.c1 && token.loc.l0 == token.loc.l1);
                        token.loc.c1 += cast_s32(token.text.count - 1);
                        report_error(token, "Newline found in string literal\n"
                                     "Right now, string literals can only be on a single line.");
                    } else {
                        assert(token.loc.c0 == token.loc.c1 && token.loc.l0 == token.loc.l1);
                        token.loc.c1 += cast_s32(token.text.count - 1 - 1); // - 1 so we don't print the unprintable character
                        report_error(token, "Unprintable character 0x%x found in string literal (after the highlighted part).", it);
                    }
                    return token;
                }
                
                if (escaping) {
                    escaping = false;
                    switch (it) {
                        Case   '\\':
                        OrCase '"':
                        OrCase 'n':
                        OrCase 'r':
                        OrCase 'e':
                        OrCase '0': break;
                        
                        Default: {
                            report_error(token.loc.fully_pathed_source_file,
                                         token.loc.relevant_lines, token.loc.l0, token.loc.c0+it_index,
                                         "Unknown string escape character '\\%c'", it);
                            return token;
                        }
                    }
                } else {
                    if (it == '\\') {
                        escaping = true;
                    } else if (it == '"') {
                        found_end = true;
                        break;
                    }
                }
                
            }
            
            if (!found_end) {
                assert(token.loc.c0 == token.loc.c1 && token.loc.l0 == token.loc.l1);
                token.loc.c1 += cast_s32(token.text.count - 1);
                report_error(token, "File ended in the middle of a string literal");
                return token;
            }
            
            token.tag = TOKEN_STRING;
            //token.loc.c1 += cast_s32(token.text.count - 1); // :MultiLineStrings
        }
        
        Case '/': {
            token.tag = '/';
            token.text.count = 1;
        }
        
        Case '#': {
            
            if (!(is_alpha(char_1) || char_1 == '_' || char_1 == '-')) {
                token.text.count = 1;
                report_error(token, "Compile-time directive has no name.");
                break;
            }
            token.text.count = 2;
            ForI (i, token_start_index+2, line.count) {
                if (!is_alphanumeric_or_underscore(line.data[i])) break;
                token.text.count += 1;
            }
            
            token.tag = get_directive_tag(advance(token.text, (char_1 == '-') ? 2 : 1));
            if (token.tag == TOKEN_UNKNOWN) {
                assert(token.loc.c0 == token.loc.c1 && token.loc.l0 == token.loc.l1);
                token.loc.c1 += cast_s32(token.text.count - 1);
                report_error(token, "Unknown compile-time directive.");
                break;
            }
        }
        
        Default: {
            if (is_alpha(char_0) || char_0 == '_') {
                ForI (i, token_start_index, line.count) {
                    if (!is_alphanumeric_or_underscore(line.data[i])) break;
                    token.text.count += 1;
                }
                
                token.tag = get_keyword_tag(token.text);
                if (token.tag == TOKEN_UNKNOWN) token.tag = TOKEN_IDENT;
            } else if (is_numeric(char_0)) {
                // @Incomplete: handle when unexpected character shows up (alpha, _, { etc.)
                token.tag = TOKEN_NUMBER;
                
                if (char_0 == '0' && (char_1 == 'x' || char_1 == 'h')) {
                    token.text.count = 2;
                    ForI (i, token_start_index+2, line.count) {
                        auto c = line.data[i];
                        if (!(is_numeric(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '_')) break;
                        token.text.count += 1; // @CleanUp: don't increment, just do i+2 or smth
                    }
                } else {
                    bool found_dot = false;
                    ForI (i, token_start_index, line.count) {
                        if (!is_numeric(line.data[i])) {
                            if (!found_dot && line.data[i] == '.') {
                                if (i+1 < line.count && line.data[i+1] == '.') break;
                                found_dot = true;
                            } else {
                                break;
                            }
                        }
                        token.text.count += 1;
                    }
                }
            } else {
                token.text.count = 1;
                report_error(token, "Unknown token. (%d)", (int)char_0);
            }
        }
    } // end switch (char_0)
    
    //if (token.loc.c0 == token.loc.c1 && token.loc.l0 == token.loc.l1) { // :MultiLineStrings
    assert(chai.error_was_reported || (token.loc.c0 == token.loc.c1 && token.loc.l0 == token.loc.l1));
    token.loc.c1 += cast_s32(token.text.count - 1);
    
    lexer->previous_tokens_l1 = token.loc.l1;
    lexer->previous_tokens_c1 = token.loc.c1;
    return token;
}


internal inline void
eat_token(Lexer *lexer) {
    if (lexer->saved_tokens[2].tag != TOKEN_UNKNOWN) {
        assert(lexer->saved_tokens[1].tag != TOKEN_UNKNOWN);
        assert(lexer->saved_tokens[0].tag != TOKEN_UNKNOWN);
        lexer->saved_tokens[0] = lexer->saved_tokens[1];
        lexer->saved_tokens[1] = lexer->saved_tokens[2];
        lexer->saved_tokens[2].tag = TOKEN_UNKNOWN;
    } else if (lexer->saved_tokens[1].tag != TOKEN_UNKNOWN) {
        assert(lexer->saved_tokens[0].tag != TOKEN_UNKNOWN);
        lexer->saved_tokens[0] = lexer->saved_tokens[1];
        lexer->saved_tokens[1].tag = TOKEN_UNKNOWN;
    } else {
        assert(lexer->saved_tokens[0].tag != TOKEN_UNKNOWN);
        lexer->saved_tokens[0].tag = TOKEN_UNKNOWN;
    }
}

internal inline Token
peek_token(Lexer *lexer, s32 index) {
    assert(index >= 0 && index < lexer->MAX_SAVED_TOKENS);
    ForI (i, 0, index) { assert(lexer->saved_tokens[i].tag != TOKEN_UNKNOWN); }
    if (lexer->saved_tokens[index].tag != TOKEN_UNKNOWN) return lexer->saved_tokens[index];
    lexer->saved_tokens[index] = lex_token(lexer);
    return lexer->saved_tokens[index];
}

internal inline Token
peek_next_token(Lexer *lexer) {
    return peek_token(lexer, 0);
}

internal inline Token
get_token(Lexer *lexer) {
    if (lexer->saved_tokens[0].tag != TOKEN_UNKNOWN) {
        auto token = lexer->saved_tokens[0];
        eat_token(lexer);
        return token;
    }
    auto token = lex_token(lexer);
    return token;
}
