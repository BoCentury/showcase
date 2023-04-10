
enum Token_Tag {
    TOKEN_UNKNOWN = 0,
    // 1-127 is reserved for single characters' ascii value directly (*+-=/ etc.)
    
    TOKEN_IDENT = 128,
    TOKEN_NUMBER,
    TOKEN_STRING,
    
    TOKEN_LEFT_SHIFT_OR_DEREFERENCE,
    TOKEN_RIGHT_SHIFT,
    
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_TIMES_EQUAL,
    TOKEN_DIV_EQUAL,
    TOKEN_MOD_EQUAL,
    
    TOKEN_IS_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS_EQUAL,
    TOKEN_LOGICAL_AND,
    TOKEN_LOGICAL_OR,
    
    TOKEN_BITWISE_AND_EQUAL,
    TOKEN_BITWISE_OR_EQUAL,
    TOKEN_BITWISE_XOR_EQUAL,
    
    TOKEN_LEFT_SHIFT_EQUAL,
    TOKEN_RIGHT_SHIFT_EQUAL,
    
    TOKEN_DOUBLE_PLUS,
    TOKEN_DOUBLE_MINUS,
    TOKEN_TRIPLE_MINUS,
    TOKEN_DOUBLE_DOT,
    
    TOKEN_ARROW,
    
    //TOKEN_KEYWORD_FIRST_KEYWORD,
    
    TOKEN_KEYWORD_RETURN,// = TOKEN_KEYWORD_FIRST_KEYWORD,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_WHILE,
    TOKEN_KEYWORD_FOR,
    TOKEN_KEYWORD_BREAK,
    TOKEN_KEYWORD_CONTINUE,
    TOKEN_KEYWORD_CASE,
    TOKEN_KEYWORD_CAST,
    TOKEN_KEYWORD_SIZEOF,
    TOKEN_KEYWORD_TYPEOF,
    TOKEN_KEYWORD_STRUCT,
    TOKEN_KEYWORD_UNION,
    TOKEN_KEYWORD_ENUM,
    TOKEN_KEYWORD_ENUM_FLAGS,
    TOKEN_KEYWORD_INLINE,
    TOKEN_KEYWORD_NULL,
    TOKEN_KEYWORD_TRUE,
    TOKEN_KEYWORD_FALSE,
    TOKEN_KEYWORD_USING,
    
    TOKEN_KEYWORD_FIRST_TYPE,
    
    TOKEN_KEYWORD_VOID = TOKEN_KEYWORD_FIRST_TYPE,
    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_S8,
    TOKEN_KEYWORD_S16,
    TOKEN_KEYWORD_S32,
    TOKEN_KEYWORD_S64,
    TOKEN_KEYWORD_U8,
    TOKEN_KEYWORD_U16,
    TOKEN_KEYWORD_U32,
    TOKEN_KEYWORD_U64,
    TOKEN_KEYWORD_F32,
    TOKEN_KEYWORD_F64,
    TOKEN_KEYWORD_BOOL,
    TOKEN_KEYWORD_STRING,
    
    TOKEN_KEYWORD_LAST_TYPE = TOKEN_KEYWORD_STRING,
    //TOKEN_KEYWORD_LAST_KEYWORD = TOKEN_KEYWORD_LAST_TYPE,
    
    //TOKEN_KEYWORD_FIRST_DIRECTIVE,
    
    TOKEN_DIRECTIVE_TYPE,// = TOKEN_KEYWORD_FIRST_DIRECTIVE,
    TOKEN_DIRECTIVE_THROUGH,
    TOKEN_DIRECTIVE_C_DECL,
    TOKEN_DIRECTIVE_LOAD,
    TOKEN_DIRECTIVE_IF,
    TOKEN_DIRECTIVE_EXPAND,
    TOKEN_DIRECTIVE_EXPORT,
    TOKEN_DIRECTIVE_MUST,
    TOKEN_DIRECTIVE_CHAR,
    
    //TOKEN_KEYWORD_LAST_DIRECTIVE = TOKEN_DIRECTIVE_,
    
    TOKEN_END_OF_FILE,
};

struct Code_Location {
    Str fully_pathed_source_file;
    Str relevant_lines;
    s32 l0;
    s32 c0;
    s32 l1;
    s32 c1;
};

struct Token {
    Code_Location loc;
    Str preceding_whitespace_and_comments;
    Str text;
    s32 tag;
    //s32 preceding_newline_count;
};

struct Lexer {
    Str fully_pathed_source_file_name;
    Str rest_of_file;
    Str current_line;
    
    // @Cleanup: Make this a circular buffer
    char *prev_line_start;
    char *prev_prev_line_start;
    Str relevant_lines;
    
    s32 previous_tokens_l1;
    s32 previous_tokens_c1;
    
    s32 whitespace_and_comments_line_count;
    
    // @Cleanup @Hack: Make this a circular buffer
    enum { MAX_SAVED_TOKENS = 3 };
    Token saved_tokens[MAX_SAVED_TOKENS];
    s32 saved_token_count;
};