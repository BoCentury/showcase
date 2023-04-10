/*
// C-string functions
#define BO_STRING_C_STYLE 1
*/

struct Str {
    s64 count;
    char *data;
};


#define ForS(i, s) ForI (i, 0, (s).count)


internal bool
strings_match_unchecked(Str a, Str b) {
    assert(a.count == b.count);
    
    ForS (i, b) {
        if (a.data[i] != b.data[i]) return false;
    }
    
    return true;
}
#define strings_match(a, b) (((a).count == (b).count) && strings_match_unchecked((a), (b)))


internal inline bool
operator ==(Str a, Str b) {
    return strings_match(a, b);
}

internal inline bool
operator !=(Str a, Str b) {
    return !strings_match(a, b);
}


internal inline void
advance(Str *s, s64 delta) {
    assert(s->data);
    if (delta > s->count) delta = s->count;
    s->data += delta;
    s->count -= delta;
    assert(s->count >= 0);
}

internal inline Str
advance(Str s, s64 delta) {
    assert(s.data);
    if (delta > s.count) delta = s.count;
    s.data += delta;
    s.count -= delta;
    assert(s.count >= 0);
    return s;
}

internal bool
starts_with(Str s, Str prefix) {
    if (s.count < prefix.count) return false;
    
    ForS (i, prefix) {
        if (s.data[i] != prefix.data[i]) return false;
    }
    return true;
}

internal bool
ends_with(Str s, Str suffix) {
    if (s.count < suffix.count) return false;
    
    s = advance(s, s.count - suffix.count);
    assert(s.count == suffix.count);
    
    ForS (i, suffix) {
        if (s.data[i] != suffix.data[i]) return false;
    }
    return true;
}

// used to be called starts_with_and_seek
internal bool
eat_prefix(Str *sp, Str prefix) {
    if (!starts_with(*sp, prefix)) return false;
    
    advance(sp, prefix.count);
    return true;
}

internal bool
eat_suffix(Str *sp, Str suffix) {
    if (!ends_with(*sp, suffix)) return false;
    
    sp->count -= suffix.count;
    return true;
}


#if 1
//
// TEXT SPECIFIC
//
// @Incomplete: remove crt


#define sl(s) (Str{ArrayCount(s)-1, (s)})
//#define stringf(s) ((int)((s).count)), ((s).data)


internal bool
case_insensitive_match(Str a, Str b) {
    if (a.count != b.count) return false;
    
    ForS (i, b) {
        auto a_char = a.data[i];
        auto b_char = b.data[i];
        
        if (a_char >= 'A' && a_char <= 'Z') a_char += ('a'-'A');
        if (b_char >= 'A' && b_char <= 'Z') b_char += ('a'-'A');
        
        if (a_char != b_char) return false;
    }
    
    return true;
}

internal inline bool
starts_with_case_insensitive(Str s, Str prefix) {
    if (s.count < prefix.count) return false;
    s.count = prefix.count;
    return case_insensitive_match(s, prefix);
}

// used to be called string_to_u64_and_substring
internal bool
eat_u64(Str *s_pointer, u64 *number) {
    u64 result = 0;
    Str s = *s_pointer;
    if (s.count <= 0 || s.data[0] < '0' || s.data[0] > '9') return false;
    
    ForS (i, s) {
        auto digit = s.data[i];
        if (digit < '0' || digit > '9') {
            s.count = i;
            break;
        }
        result *= 10;
        result += digit - '0';
    }
    *s_pointer = s;
    *number = result;
    return true;;
}

internal u64
string_to_u64(Str s) {
    u64 result = 0;
    ForS (i, s) {
        auto digit = s.data[i];
        if (digit < '0' || digit > '9') break;
        result *= 10;
        result += digit - '0';
    }
    return result;
}

internal inline u32
string_to_u32(Str s) {
    u64 result_u64 = string_to_u64(s);
    assert(result_u64 <= U32_MAX); // clamp it instead?
    u32 result = (u32)result_u64;
    return result;
}

internal s64
string_to_s64(Str s) {
    bool negative = false;
    if (s.count > 0 && s.data[0] == '-') {
        negative = true;
        s = advance(s, 1);
    } else if (s.count > 0 && s.data[0] == '+') {
        s = advance(s, 1);
    }
    u64 result_u64 = string_to_u64(s);
    s64 result;
    if (negative) {
        assert(result_u64 <= (u64)S64_MAX+1); // clamp it instead?
        result = -(s64)result_u64;
    } else {
        assert(result_u64 <= S64_MAX); // clamp it instead?
        result =  (s64)result_u64;
    }
    return result;
}

internal inline s32
string_to_s32(Str s) {
    s64 result_s64 = string_to_s64(s);
    assert(result_s64 >= S32_MIN && result_s64 <= S32_MAX); // clamp it instead?
    s32 result = (s32)result_s64;
    return result;
}

internal inline bool
string_to_bool(Str s) {
    return s == sl("true");
}

internal s64
index_of_first_char(Str s, char c) {
    ForS (i, s) {
        if (s.data[i] == c) {
            return i;
        }
    }
    return -1;
}

internal s64
index_of_last_char(Str s, char c) {
    for (s64 i = s.count-1; i >= 0; i -= 1) {
        if (s.data[i] == c) {
            return i;
        }
    }
    return -1;
}

internal inline Str
seek_to_char(Str s, char c) {
    auto i = index_of_first_char(s, c);
    if (i >= 0) return advance(s, i);
    return Str{};
}

internal inline Str
seek_past_char(Str s, char c) {
    auto i = index_of_first_char(s, c);
    if (i >= 0) return advance(s, i + 1);
    return Str{};
}

internal inline Str
seek_to_last_char(Str s, char c) {
    auto i = index_of_last_char(s, c);
    if (i >= 0) return advance(s, i);
    return Str{};
}

internal inline Str
seek_past_last_char(Str s, char c) {
    auto i = index_of_last_char(s, c);
    if (i >= 0) return advance(s, i + 1);
    return Str{};
}

struct Split_Str {
    Str lhs, rhs;
};

// if didn't find space:  lhs = s, rhs = Str{}
internal inline Split_Str
break_by_spaces(Str s) {
    Split_Str result = {};
    if (s.count > 0) {
        assert(s.data);
        result.lhs = s;
        result.rhs = seek_to_char(s, ' ');
        
        result.lhs.count -= result.rhs.count;
        while (result.rhs.count > 0 && result.rhs.data[0] == ' ') {
            result.rhs = advance(result.rhs, 1);
        }
    }
    return result;
}

// if didn't find char:  lhs = s, rhs = Str{}
internal inline Split_Str
break_by_char(Str s, char c) {
    Split_Str result = {};
    if (s.count > 0) {
        assert(s.data);
        result.lhs = s;
        result.rhs = seek_to_char(s, c);
        
        if (result.rhs.count > 0 && result.rhs.data[0] == c) {
            result.lhs.count -= result.rhs.count;
            result.rhs = advance(result.rhs, 1);
        }
    }
    return result;
}

// if didn't find char:  lhs = s, rhs = Str{}
internal inline Split_Str
break_by_last_char(Str s, char c) {
    Split_Str result = {};
    if (s.count > 0) {
        assert(s.data);
        result.lhs = s;
        result.rhs = seek_to_last_char(s, c);
        
        if (result.rhs.count > 0 && result.rhs.data[0] == c) {
            result.lhs.count -= result.rhs.count;
            result.rhs = advance(result.rhs, 1);
        }
    }
    return result;
}

internal inline Str
break_by_spaces(Str s, Str *rhs) {
    auto result = break_by_spaces(s);
    *rhs = result.rhs;
    return result.lhs;
}

internal inline Str
break_by_char(Str s, char c, Str *rhs) {
    auto result = break_by_char(s, c);
    *rhs = result.rhs;
    return result.lhs;
}

internal inline Str
break_by_last_char(Str s, char c, Str *rhs) {
    auto result = break_by_last_char(s, c);
    *rhs = result.rhs;
    return result.lhs;
}


// didn't find newline: line = s, rest = Str{}
// found newline: line = next line without '(\r)\n', s = everything after '\n'
internal inline Split_Str
break_by_newline(Str s) {
    auto line_and_rest = break_by_char(s, '\n');
    if (line_and_rest.lhs.count > 0 && line_and_rest.lhs.data[line_and_rest.lhs.count-1] == '\r') {
        line_and_rest.lhs.count -= 1;
    }
    
    return line_and_rest;
}

internal inline Str
consume_next_line(Str *s) {
    auto line_and_rest = break_by_newline(*s);
    *s = line_and_rest.rhs;
    return line_and_rest.lhs;
}



internal inline bool
is_alpha(char c) {
    bool result = ((c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z'));
    return result;
}


internal inline bool
is_numeric(char c) {
    bool result = (c >= '0' && c <= '9');
    return result;
}

// @Incomplete: change to is_token_char?
internal inline bool
is_alphanumeric_or_underscore(char c) {
    bool result = (is_alpha(c) || is_numeric(c) || c == '_');
    return result;
}

internal inline bool
is_whitespace(char c) {
    switch (c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
        return true;
    }
    return false;
}

internal Str
strip_surrounding_whitespace(Str s) {
    while (s.count > 0 && is_whitespace(s.data[0])) {
        s.data  += 1;
        s.count -= 1;
    }
    
    while (s.count > 0 && is_whitespace(s.data[s.count-1])) {
        s.count -= 1;
    }
    
    return s;
}

#if BO_STRING_C_STYLE

internal s64
c_string_length(char *c_string) {
    auto scan = c_string;
    while (*scan) scan += 1;
    // @Speed
    s64 result = scan - c_string;
    return result;
}

internal bool
starts_with(Str a, const char * const b) {
    ForS (i, a) {
        if (b[i] == '\0' || a.data[i] != b[i]) return false;
    }
    
    return true;
}

internal inline bool
operator ==(Str a, const char * const b) {
    if (!starts_with(a, b)) return false;
    return (b[a.count] == '\0');
}

internal inline bool
operator !=(Str a, const char * const b) {
    return !(a == b);
}

internal inline bool
operator ==(const char * const b, Str a) {
    return a == b;
}

internal inline bool
operator !=(const char * const b, Str a) {
    return !(a == b);
}

internal inline Str
c_style_to_string(char *c_string) {
    Str result;
    result.data = c_string;
    result.count = c_string_length(c_string);
    return result;
}

#endif // BO_STRING_C_STYLE

#endif // TEXT SPECIFIC
