
// @Cleanup: This is a blatant misuse of inheritence. Pull it back a lot.

enum Enum_Ast_Tag {
    AST_UNKNOWN,
    
    AST_EXPR,
    AST_SUBEXPR,
    AST_LITERAL,
    AST_IDENT,
    AST_DECL,
    AST_ASSIGNMENT,
    AST_BINARY,
    AST_UNARY,
    //AST_CAST,
    AST_SIZEOF,
    AST_RETURN,
    AST_CALL,
    AST_PROC_DEFN,
    AST_BRANCH,
    AST_LOOP,
    AST_BLOCK,
    AST_CASE,
    AST_SWITCH,
    AST_FOR_COUNT,
    AST_FOR_EACH,
    AST_EXPLICITLY_UNINITALIZE,
    AST_TYPE_EXPR,
    AST_STRUCT,
    AST_LOAD,
    AST_BREAK,
    AST_CONTINUE,
    AST_COMMA_SEPARATED,
    AST_STATIC_BRANCH,
};

// Helper struct that other structs can inherit to be able to placement-new in NewAst
struct Parser_Alloc_Helper {};

inline void *operator new(umm size, Parser_Alloc_Helper *p) { return p; } // Thanks C++
#define NewAst(T) (new ((T *)arena_alloc(&chai.ast_arena, sizeof(T))) (T))

// Uncomment and compile to double check that only Ast types are used with NewAst
//#define NewAst(type) (((decltype(type::ast_type))0), (new ((type *)alloc(chai.ast_bucket_array, sizeof(type))) (type)));
#define AllocDataTypeUninitialized() ((Data_Type *)arena_alloc(&chai.ast_arena, sizeof(Data_Type)))

struct Ast : Parser_Alloc_Helper {
    Enum_Ast_Tag ast_tag = AST_UNKNOWN;
    Str preceding_whitespace_and_comments = Str{ .count = -1, .data = null };
};

struct Ast_Expr : Ast {
    Ast_Expr() { ast_tag = AST_EXPR; }
    // @Cleanup: Move Code_Location to Ast and maybe delete Ast_Expr.
    Code_Location loc = {};
};

enum struct Data_Type_Tag {
    UNKNOWN,
    
    INT,
    FLOAT,
    BOOL,
    STRING,
    VOID,
    
    STATIC_ARRAY,
    POINTER,
    
    STRUCT,
    
    PROC,
    
    TYPEOF,
    USER_TYPE, 
    // USER_TYPE is a type created by the user.
    // A :: struct/union/enum { member: Type }
    // B :: #type int
    // C :: #type A
};

enum struct Struct_Kind: u32 {
    STRUCT,
    UNION,
    ENUM,
};

// @Cleanup: This is currently doing double duty handling both the
// representation of type expression, and the actual data type of expressions.
// These should not be conflated like this.
// Split it out into Ast_Type_Subexer and Data_Type or whatever.
struct Data_Type {
    struct Number { // INT and FLOAT
        bool is_signed; // INT only
        bool is_solid;
        s32 size;
    };
    struct Pointer {
        Data_Type *to;
    };
    struct Static_Array {
        Data_Type *of;
        struct Ast_Subexpr *size_expr;
    };
    struct Struct {
        struct Ast_Struct_Member_Node *first_member;
        Struct_Kind struct_union_enum;
    };
    struct Procedure {
        struct Ast_Parameter_Node *first_param;
        struct Ast_Return_Type_Node *first_return_type_expr;
        int multi_return_index;
    };
    struct Typeof {
        struct Ast_Subexpr *expr;
    };
    struct User_Type { // User-defined type
        struct Ast_Type_Expr *type_expr;
    };
    
    Data_Type_Tag tag;
    struct Ast_Ident *user_type_ident; // Set if type is a user defined type like Ast_Expr, Data_Type_Tag or DWORD
    // This applies to both type redefines and structs etc.
    // Maybe try to make this clearer
    
    // These structs need to be named cause C++ is complete ass
    union {
        Number       number;
        Pointer      pointer;
        Static_Array array;
        Struct       structure;
        Procedure    proc;
        Typeof       typeof;
        User_Type    user_type;
        bool         void_is_null;
    };
};

internal inline bool
is_number_type(Data_Type_Tag tag) { // @Temporary: move all functions out of this file? or are these fine?
    return tag == Data_Type_Tag::INT || tag == Data_Type_Tag::FLOAT;
}

// Try changing this so it has one member for the base type
// and one for 'modifiers' (pointer-to, array-of)
struct Ast_Type_Expr : Ast_Expr {
    Ast_Type_Expr() { ast_tag = AST_TYPE_EXPR; }
    
    Data_Type type = {};
};

struct Ast_Subexpr : Ast_Expr {
    Ast_Subexpr() { ast_tag = AST_SUBEXPR; }
    
    Data_Type data_type = {};
};

struct Ast_Ident : Ast_Subexpr {
    Ast_Ident() { ast_tag = AST_IDENT; }
    enum {
        CONSTANT = 0x1,
        NO_NAME_MANGLE = 0x2,
        IS_C_DECL = 0x4,
        IS_EXPORT = 0x8,
    };
    // The data_type of idents are stored in chai.variables to be looked up
    s32 variable_index = -1;
    u32 flags = 0;
    Str name = {};
};

// @Cleanup: struct Node<T> ?
struct Ast_Argument_Node : Parser_Alloc_Helper {
    Ast_Subexpr *expr = null;
    Ast_Argument_Node *next = null;
};

struct Ast_Call : Ast_Ident {
    Ast_Call() { ast_tag = AST_CALL; }
    
    //Ast_Ident ident; // :GetsDefaultValues
    Ast_Argument_Node *first_arg = null;
};

struct Ast_Literal : Ast_Subexpr {
    Ast_Literal() { ast_tag = AST_LITERAL; }
    
    enum {
        UNKNOWN,
        
        TRUE,
        FALSE,
        STRING,
        //CHAR,
        NUMBER,
        NULL,
    } tag = {};
    Str text = {};
    bool is_char = false;
    //Data_Type data_type = {};
    union {
        // :UnionZero need to zero the largest member
        u8  _u8;
        u16 _u16;
        u32 _u32;
        u64 _u64;
        s8  _s8;
        s16 _s16;
        s32 _s32;
        s64 _s64;
        f32 _f32;
        f64 _f64;
        bool _bool;
        Str s = {};
    };
};

struct Ast_Explicitly_Uninitalize : Ast_Expr {
    Ast_Explicitly_Uninitalize() { ast_tag = AST_EXPLICITLY_UNINITALIZE; }
};

// @Cleanup: struct Node<T> ?
struct Ast_Ident_Node : Ast_Ident {
    Ast_Ident_Node *next = null;
};

struct Ast_Decl : Ast {
    Ast_Decl() { ast_tag = AST_DECL; }
    Code_Location loc = {};
    
    // @Incomplete: type_expr is misused here a lot.
    // It should only really be set if there's an actual type expression,
    // but it's hackily used to store the actual Data_Type of the declaration,
    // in many places, including struct members.
    // Fix this.
    Ast_Type_Expr *_type_expr = null;
    
    Ast_Ident ident; // :GetsDefaultValues
    Ast_Expr *initialization = null;
    enum {
        NO_USING,
        NORMAL_USING,
        ANON_USING,
    } using_kind = {};
    // ANON_USING: use only _type_expr and data_type to define the anonymous structlike
};


/*
 *Q :: struct {
    *    union { // first_union
        *        w;
        *        union { // second_union
            *            e;
            *            union { // third_union
                *                r;
            *            }
        *        }
    *    }
*}



    *do_dot(q.e); {
    *    find first_union; its anon using so skip name check;
    *    find_matching_member(first_union, "e"); {
        *        find "w", its not "e";
        *        
        *        find second_union; its anon using so skip name check;
        *        find_matching_member(second_union, "e") {
            *            find "e"; it matches;
            *            return end_member = "e";
        *        }
        *        if (end_member) {
            *            auto node  = NewAst(Dot_Chain_Node);
            *            node->decl = &member->decl;
            *            node->next = result.dot_chain;
            *            result.dot_chain = node;
            *            return result;
        *        }
    *    }
*}
*/

// @Cleanup: struct Node<T> ?
struct Ast_Struct_Member_Node : Parser_Alloc_Helper {
    // @Hack: We're currently using decl.type_expr for anonymous structlikes in
    // struct definitions. Union?
    Ast_Decl decl; // :GetsDefaultValues
    Ast_Struct_Member_Node *next = null;
};

#if 0
struct Ast_Struct : Ast_Expr {
    Ast_Struct() { ast_tag = AST_STRUCT; }
    
    Ast_Struct_Member_Node *first_member = null;
};
#endif

enum struct Assign_Tag {
    UNKNOWN,
    
    NORMAL,
    
    MUL,
    DIV,
    MOD,
    ADD,
    SUB,
    
    BITWISE_AND,
    BITWISE_XOR,
    BITWISE_OR,
    
    LEFT_SHIFT,
    RIGHT_SHIFT,
};

struct Ast_Assignment : Ast {
    Ast_Assignment() { ast_tag = AST_ASSIGNMENT; }
    
    Assign_Tag tag = Assign_Tag::UNKNOWN;
    Code_Location op_loc = {};
    
    Ast_Subexpr *left  = null;
    Ast_Subexpr *right = null;
};

enum struct Binary_Tag {
    UNKNOWN,
    
    ARRAY_ACCESS,
    DOT,
    
    MUL,
    DIV,
    MOD,
    
    ADD,
    SUB,
    
    LEFT_SHIFT,
    RIGHT_SHIFT,
    
    BITWISE_AND,
    BITWISE_XOR,
    BITWISE_OR,
    
    LESS_THAN,
    LESS_THAN_OR_EQUAL,
    GREATER_THAN,
    GREATER_THAN_OR_EQUAL,
    
    IS_EQUAL,
    NOT_EQUAL,
    
    LOGICAL_AND,
    
    LOGICAL_OR,
};
/*

* /
number, number

%
int, int

+
number, number
pointer, int
int, pointer

-
number, number
pointer, int
poitner, pointer (to same)

<< >>
int_a, int_b 

& ^ |
int, int

< <= > >=
number, number
//pointer, pointer (to same)

== !=
number, number
//bool
//pointer, pointer (to same)
//proc, proc (to same)
//pointer, *void
//pointer, null
//proc,    null
//*void, pointer
//null,  pointer
//null,  proc
//string, string?


&& ||
bool
//pointer
//proc
//string?


*/

struct Dot_Chain_Node : Parser_Alloc_Helper {
    Ast_Decl       *decl = null;
    Dot_Chain_Node *next = null;
};

struct Ast_Binary : Ast_Subexpr {
    Ast_Binary() { ast_tag = AST_BINARY; }
    
    Binary_Tag tag = Binary_Tag::UNKNOWN;
    //Data_Type data_type = {};
    
    Ast_Subexpr *left  = null;
    Ast_Subexpr *right = null;
    
    Dot_Chain_Node *dot_chain_of_usings_that_lead_to_member = null; // Only used in Binary_Tag::DOT
};

/*

*
 lvalue-expression
 //proc // not a thing since procs are always pointers
  //?expr special case: * and ? cancel each other, neither one is evaluated (C99)
 //expr[expr] special case: * and the ? that is implied in [] cancel each other, only the addition implied in [] is evaluated (C99)

?
pointer

-
number

!
bool
//pointer
//proc
//string?

~
int

cast(T)
number
pointer
//NOT proc, that wouldn't make any sense
//string?
//static array?
//array view?

*/

enum struct Unary_Tag {
    UNKNOWN,
    
    POINTER_TO,
    DEREFERENCE,
    MINUS,
    LOGICAL_NOT,
    BITWISE_NOT,
    
    CAST,
};

struct Ast_Unary : Ast_Subexpr {
    Ast_Unary() { ast_tag = AST_UNARY; }
    
    Unary_Tag tag = Unary_Tag::UNKNOWN;
    //Data_Type data_type = {};
    
    Ast_Subexpr *right = null;
};

struct Ast_Cast : Ast_Unary {
    //Ast_Cast() { ast_tag = AST_CAST; }
    
    Ast_Type_Expr type_expr; // :GetsDefaultValues
};

struct Ast_Sizeof : Ast_Subexpr {
    Ast_Sizeof() { ast_tag = AST_SIZEOF; }
    
    Ast_Type_Expr type_expr; // :GetsDefaultValues
};

// @Cleanup: struct Node<T> ?
struct Ast_Block_Statement_Node : Parser_Alloc_Helper {
    Ast *statement = null;
    Ast_Block_Statement_Node *next = null;
};

struct Ast_Block : Ast {
    Ast_Block() { ast_tag = AST_BLOCK; }
    
    Ast_Block_Statement_Node *first_statement = null;
};

// @Cleanup: struct Node<T> ?
struct Ast_Return_Type_Node : Ast_Type_Expr {
    bool must_receive = false;
    Ast_Return_Type_Node *next = null;
};

// @Cleanup: struct Node<T> ?
struct Ast_Parameter_Node : Parser_Alloc_Helper {
    Ast_Decl decl; // :GetsDefaultValues
    Ast_Parameter_Node *next = null;
};

struct Ast_Load : Ast {
    Ast_Load() { ast_tag = AST_LOAD; }
    
    Ast_Literal *file = null;
};

struct Ast_Proc_Defn : Ast_Expr {
    Ast_Proc_Defn() { ast_tag = AST_PROC_DEFN; }
    
    Ast_Type_Expr signature; // :GetsDefaultValues
    Ast_Block     body;   // :GetsDefaultValues
};

struct Ast_Case : Ast_Block {
    Ast_Case() { ast_tag = AST_CASE; }
    
    bool fall_through = false;
    Ast_Subexpr *expr = null; // @Incomplete: Must be a constant expression
    Ast_Case    *next = null;
};

struct Ast_Switch : Ast {
    Ast_Switch() { ast_tag = AST_SWITCH; }
    
    Ast_Subexpr *switch_expr = null;
    Ast_Case    *first_case  = null;
};

struct Ast_Branch : Ast {
    Ast_Branch() { ast_tag = AST_BRANCH; }
    
    Ast_Subexpr *condition = null;
    Ast *true_statement    = null;
    Ast *false_statement   = null;
};

struct Ast_Static_Branch : Ast {
    Ast_Static_Branch() { ast_tag = AST_STATIC_BRANCH; }
    
    bool condition_is_true = false;
    Ast_Subexpr *condition = null;
    Ast *true_statement    = null;
    Ast *false_statement   = null;
};

struct Ast_Loop : Ast {
    Ast_Loop() { ast_tag = AST_LOOP; }
    
    Ast_Subexpr *condition = null;
    Ast *statement = null;
};

struct Ast_For_Count : Ast {
    Ast_For_Count() { ast_tag = AST_FOR_COUNT; }
    
    bool reverse = false;
    
    //bool is_inclusive = false; // @InclusiveFor
    Ast_Ident it; // :GetsDefaultValues
    Ast_Subexpr *start = null;
    Ast_Subexpr *end   = null;
    Ast *statement  = null;
};

struct Ast_For_Each : Ast {
    Ast_For_Each() { ast_tag = AST_FOR_EACH; }
    
    bool reverse = false;
    bool pointer = false;
    
    Ast_Ident it;       // :GetsDefaultValues
    Ast_Ident it_index; // :GetsDefaultValues
    Ast_Subexpr *range = null;
    Ast *statement  = null;
};

struct Ast_Break : Ast {
    Ast_Break() { ast_tag = AST_BREAK; }
    Code_Location loc = {};
};

struct Ast_Continue : Ast {
    Ast_Continue() { ast_tag = AST_CONTINUE; }
    Code_Location loc = {};
};

// @Cleanup: struct Node<T> ?
struct Ast_Return_Expr_Node : Parser_Alloc_Helper {
    Ast_Subexpr *expr = null;
    Ast_Return_Expr_Node *next = null;
};

struct Ast_Return : Ast {
    Ast_Return() { ast_tag = AST_RETURN; }
    Code_Location loc = {};
    
    int multi_return_index = -1;
    Ast_Return_Expr_Node *first = null;
};

enum Variable_Tag {
    VARIABLE_UNKNOWN,
    
    VARIABLE_NORMAL,
    //VARIABLE_OVERLOADED_FUNCTION,
    VARIABLE_PUSH_SCOPE,
    VARIABLE_POP_SCOPE,
};

struct Variable {
    Variable_Tag tag;
    union {
        struct {
            Ast_Ident *ident;
            //Data_Type *data_type;
            Str overloaded_proc_name;
        };
        s32 matching_push_index;
    };
};


/*
 === Implicit conversions
    
    When an expression is used in the context where a value of a different type is expected, conversion may occur:
    
*    int n = 1L; // expression 1L has type long, int is expected
    *    n = 2.1; // expression 2.1 has type double, int is expected
    *    char *p = malloc(10); // expression malloc(10) has type void*, char* is expected

Conversions take place in the following situations:


 - Conversion as if by assignment

. In assignment, the value of the right is converted to the type of the left.
. In a call, the value of each argument expression is converted to the type of the declared types of the corresponding parameter
. In a return statement, the value of the operand of return is converted to an object having the return type of the function

// . In scalar initialization, the value of the initializer expression is converted to the type of the object being initialized

// Note that actual assignment, in addition to the conversion, also removes extra range and precision from floating-point types and prohibits overlaps; those characteristics do not apply to conversion as if by assignment.


- Usual arithmetic conversions

The arguments of the following arithmetic operators undergo implicit conversions for the purpose of obtaining the common FLOAT type, which is the type in which the calculation is performed:

. binary arithmetic * / % + -
. relational operators < > <= >= == !=
. binary bitwise arithmetic & ^ |
. the conditional operator ?:


*if one operand is f64 {
    *    other operand (f32 or INT) is implicitly converted to f64
*} else if one operand is f32 {
    *    other operand (INT) is implicitly converted to f32
*} else {
    *    both operands are INT;
    *    both operands undergo integer promotions (see below);
    *    then, after integer promotion, one of the following cases applies:
    *    if the types are the same {
        *        nothing needs to be done
    *    } else if the types have the same signedness (both signed or both unsigned) {
        *        operand whose type has the lesser conversion rank[1] is implicitly converted[2] to the other type
    *    } else if unsigned type conversion rank >= signed type conversion rank {
        *        operand with the signed type is implicitly converted to the unsigned type.
    *    } else if the signed type can represent all values of the unsigned type {
        *        operand with the unsigned type is implicitly converted to the signed type.
    *    } else {
        *     both operands undergo implicit conversion to the unsigned type counterpart of the signed operand's type.
    *    }
*}

         1. Refer to "integer promotions" below for the rules on ranking.
2. Refer to "integer conversions" under "implicit conversion semantics" below.
  
TYPES ARE C TYPES
    1.f + 20000001; // int is converted to float, giving 20000000.00
    //                 addition and then rounding to float gives 20000000.00
    (char)'a' + 1L; // first, char 'a', which is 97, is promoted to int
    //                 different types: int and long
    //                 same signedness: both signed
    //                 different rank: long is of greater rank than int
    //                 therefore, int 97 is converted to long 97
    //                 the result is 97 + 1 = 98 of type signed long
    
    2u - 10; // different types: unsigned int and signed int
    //          different signedness
    //          same rank
    //          therefore, signed int 10 is converted to unsigned int 10
    //          since the arithmetic operation is performed for unsigned integers (see "Arithmetic operators" topic),
    //          the calculation performed is (2 - 10) modulo (2 raised to n), where n is the number of bits of unsigned int
    //          if unsigned int is 32-bit long, then 
    //          the result is (-8) modulo (2 raised to 32) = 4294967288 of type unsigned int
    
    5UL - 2ULL; // different types: unsigned long and unsigned long long
    //             same signedness
    //             different rank: rank of unsigned long long is greater
    //             therefore, unsigned long 5 is converted to unsigned long long 5
    //             since the arithmetic operation is performed for unsigned integers (see "Arithmetic operators" topic),
    //             if unsigned long long is 64-bit long, then
    //             the result is (5 - 2) modulo (2 raised to 64) = 3 of type unsigned long long
    
    0UL - 1LL; // different types: unsigned long and signed long long
    //            different signedness
    //            different rank: rank of signed long long is greater.
    //            if ULONG_MAX > LLONG_MAX, then signed long long cannot represent all unsigned long
    //            therefore, this is the last case: both operands are converted to unsigned long long
    //            unsigned long 0 is converted to unsigned long long 0
    //            long long 1 is converted to unsigned long long 1
    //            since the arithmetic operation is performed for unsigned integers (see "Arithmetic operators" topic),
    //            if unsigned long long is 64-bit long, then  
    //            the calculation is (0 - 1) modulo (2 raised to 64)
    //            thus, the result is 18446744073709551615 (ULLONG_MAX) of type unsigned long long
}
As always, the result of a floating-point operator may have greater range and precision than is indicated by its type (see FLT_EVAL_METHOD).

Note: regardless of usual arithmetic conversions, the calculation may always be performed in a narrower type than specified by these rules under the as-if rule



=== Value transformations


- lvalue conversion (pretty sure the whole point of this was to outline that the attributes get lost, but we don't care)

Any lvalue expression of any non-array type, when used in any context other than

. as the operand of the pointer-to operator (if allowed)
. as the left-hand operand of the member access (dot) operator.
. as the left-hand operand of the assignment and compound assignment operators.

undergoes lvalue conversion: The type remains the same. The value remains the same, but loses its lvalue properties (the address may no longer be taken).

If the lvalue has incomplete type, the behavior is undefined.

If the lvalue designates an object of automatic storage duration whose address was never taken and if that object was uninitialized (not declared with an initializer and no assignment to it has been performed prior to use), the behavior is undefined.

This conversion models the memory load of the value of the object from its location.
*{
    *    volatile int n = 1;
    *    int x = n;            // lvalue conversion on n reads the value of n
    *    volatile int* p = &n; // no lvalue conversion: does not read the value of n
*}


- Incomplete types

An incomplete type is an object type that lacks sufficient information to determine the size of the objects of that type. An incomplete type may be completed at some point in the translation unit.

The following types are incomplete:

. the type void. This type cannot be completed. (Special case for us handled seperately)
. array type of unknown size. It can be completed by a later declaration that specifies the size. (doesn't exist for us. the equivalent would be array view which is a complete struct) 
extern char a[]; // the type of a is incomplete (this typically appears in a header)
    char a[10];      // the type of a is now complete (this typically appears in a source file)
. struct/union type of unknown content. It can be completed by a declaration of the same struct/union that defines its content later in the same scope.

    *struct node {
        *    struct node *next; // struct node is incomplete at this point
    *}; // struct node is complete at this point



- Array to pointer conversion

Any lvalue expression of array type, when used in any context other than

. as the operand of the pointer-to operator
. as the string literal used for array initialization

undergoes a conversion to the non-lvalue pointer to its first element.

    int a[3], b[3][4];
    int* p = a;      // conversion to &a[0]
    int (*q)[4] = b; // conversion to &b[0]



- Function to pointer conversion (this is dumb as fuck, functions are pointers)

Any function designator expression, when used in any context other than

. as the operand of the address-of operator
. as the operand of sizeof

undergoes a conversion to the rvalue pointer to the function designated by the expression.
    
int f(int);
    int (*p)(int) = f; // conversion to &f
    (***p)(1); // repeated dereference to f and conversion back to &f
 


- My function to pointer conversion

Any function designator expression, when used in any context other than

. as the operand of the address-of operator

undergoes a conversion to the rvalue pointer to the function designated by the expression.

    int f(int);
    int (*p)(int) = f; // conversion to &f
    (***p)(1); // repeated dereference to f and conversion back to &f



=== Implicit conversion semantics


Implicit conversion, whether as if by assignment or a usual arithmetic conversion, consists of two stages:

 1) value transformation (if applicable)
2) one of the conversions listed below (if it can produce the target type)
 

- Compatible types

Conversion of a value of any type to any compatible type is always a no-op and does not change the representation.

    uint8_t (*a)[10];         // if uint8_t is a typedef to unsigned char
    unsigned char (*b)[] = a; // then these pointer types are compatible


 - Integer promotions

Integer promotion is the implicit conversion of a value of any integer type with rank less or equal to rank of int to the value of type int or unsigned int

If int can represent the entire range of values of the original type, the value is converted to type int. Otherwise the value is converted to unsigned int.

Integer promotions preserve the value, including the sign:

    *int main(void) {
        *    void f(); // old-style function declaration
        *    char x = 'a'; // integer conversion from int to char
        *    f(x); // integer promotion from char back to int
    *}
    *void f(x) int x; {} // the function expects int

    rank above is a property of every integer type and is defined as follows:

1) the ranks of all signed integer types are different and increase with their precision: rank of signed char < rank of short < rank of int < rank of long int < rank of long long int
2) the ranks of all signed integer types equal the ranks of the corresponding unsigned integer types
4) rank of char equals rank of signed char and rank of unsigned char
5) the rank of _Bool is less than the rank of any other standard integer type
6) the rank of any enumerated type equals the rank of its compatible integer type
7) ranking is transitive: if rank of T1 < rank of T2 and rank of T2 < rank of T3 then rank of T1 < rank of T3
8) any aspects of relative ranking of extended integer types not covered above are implementation defined
Note: integer promotions are applied only

. as part of usual arithmetic conversions (see above)
. as part of default argument promotions (see above)
. to the operand of the unary arithmetic operators + and -
. to the operand of the unary bitwise operator ~
. to both operands of the shift operators << and >>


- Boolean conversion

A value of any scalar type can be implicitly converted to _Bool. The values that compare equal to zero are converted to ​0​, all other values are converted to 1

    bool b1 = 0.5;              // b1 == 1 (0.5 converted to int would be zero)
    bool b4 = 0.0/0.0;          // b4 == 1 (NaN does not compare equal to zero)
 

- Integer conversions

A value of any integer type can be implicitly converted to any other integer type. Except where covered by promotions and boolean conversions above, the rules are: 

. if the target type can represent the value, the value is unchanged
 . otherwise, if the target type is unsigned, the value 2^b where b is the number of bits in the target type, is repeatedly subtracted or added to the source value until the result fits in the target type. In other words, unsigned integers implement modulo arithmetic.
. otherwise, if the target type is signed, the behavior is implementation-defined (which may include raising a signal)
    
char x = 'a'; // int -> char, result unchanged
    unsigned char n = -123456; // target is unsigned, result is 192 (that is, -123456+483*256)
    signed char m = 123456;    // target is signed, result is implementation-defined
    assert(sizeof(int) > -1);  // assert fails:
    //                            operator > requests conversion of -1 to size_t,
    //                            target is unsigned, result is SIZE_MAX
 


  - Real floating-integer conversions

A finite value of any real floating type can be implicitly converted to any integer type. Except where covered by boolean conversion above, the rules are:

The fractional part is discarded (truncated towards zero).
. If the resulting value can be represented by the target type, that value is used
. otherwise, the behavior is undefined
    
int n = 3.14; // n == 3
    int x = 1e10; // undefined behavior for 32-bit int
 

A value of any integer type can be implicitly converted to any real floating type.

. if the value can be represented exactly by the target type, it is unchanged
. if the value can be represented, but cannot be represented exactly, the result is the nearest higher or the nearest lower value (in other words, rounding direction is implementation-defined), although if IEEE arithmetic is supported, rounding is to nearest. It is unspecified whether FE_INEXACT is raised in this case.
. if the value cannot be represented, the behavior is undefined, although if IEEE arithmetic is supported, FE_INVALID is raised and the result value is unspecified.
The result of this conversion may have greater range and precision than its target type indicates (see FLT_EVAL_METHOD.

If control over FE_INEXACT is needed in floating-to-integer conversions, rint and nearbyint may be used.

double d = 10; // d = 10.00
    float f = 20000001; // f = 20000000.00 (FE_INEXACT)
    float x = 1+(long long)FLT_MAX; // undefined behavior
 

- Real floating point conversions

A value of any real floating type can be implicitly converted to any other real floating type.

. If the value can be represented by the target type exactly, it is unchanged
. if the value can be represented, but cannot be represented exactly, the result is the nearest higher or the nearest lower value (in other words, rounding direction is implementation-defined), although if IEEE arithmetic is supported, rounding is to nearest
. if the value cannot be represented, the behavior is undefined

The result of this conversion may have greater range and precision than its target type indicates (see FLT_EVAL_METHOD.

double d = 0.1; // d = 0.1000000000000000055511151231257827021181583404541015625
    float f = d;    // f = 0.100000001490116119384765625
    float x = 2*(double)FLT_MAX; // undefined
 

- Pointer conversions

A pointer to void can be implicitly converted to and from any pointer to object type with the following semantics:

. If a pointer to object is converted to a pointer to void and back, its value compares equal to the original pointer.
. No other guarantees are offered

int* p = malloc(10 * sizeof(int)); // malloc returns void*
 

Any integer constant expression with value ​0​ as well as integer pointer expression with value zero cast to the type void* can be implicitly converted to any pointer type (both pointer to object and pointer to function). The result is the null pointer value of its type, guaranteed to compare unequal to any non-null pointer value of that type. This integer or void* expression is known as null pointer constant and the standard library provides one definition of this constant as the macro NULL.
    
int* p = 0;
    double* q = NULL;



=== Notes

Although signed integer overflow in any arithmetic operator is undefined behavior, overflowing a signed integer type in an integer conversion is merely unspecified behavior.

On the other hand, although unsigned integer overflow in any arithmetic operator (and in integer conversion) is a well-defined operation and follows the rules of modulo arithmetic, overflowing an unsigned integer in a floating-to-integer conversion is undefined behavior: the values of real floating type that can be converted to unsigned integer are the values from the open interval (-1; Unnn_MAX+1).

unsigned int n = -1.0; // undefined behavior
Conversions between pointers and integers (except from pointer to _Bool and from integer constant expression with the value zero to pointer), between pointers to objects (except where either to or from is a pointer to void) and conversions between pointers to functions (except when the functions have compatible types) are never implicit and require a cast operator.

There are no conversions (implicit or explicit) between pointers to functions and pointers to objects (including void*) or integers.

*/

















/*

a & 1 == b & 1
 |      ==
|    &    &
|   a 1  b 1
(a & 1) == (b & 1)


a == 1 & b == 1
|      ==
|   ==     1
| a    &  
|     1 b
|
|
|


==
left ==

(a == 1) & (b == 1)


== is 7
right 1 isn't binary so no paren
left == is 7, 7 <= 7 so no paren (<= cause left)

== is 7
left an't binary so no paren
right & is 4, 4

(a == (1 & b)) == 1
(a == 1) & (a == 1)


The only reason for something to be a binary or unary is if you want to be able to use it on itself. ??p, ((a+b)+c)

   proc call are just a special case of an ident. it being operator would mean proc_that_returns_proc()() would be legal and fuck that.
sizeof is a unary in C, but it's like a proc for us, so if it's an operator then sizeof()() or some shit would have to make sense.
the only way to consider a compound literal to be an operator is something like (){(){}} somehow being the precedence, but the {} is just treated as parens so it doens't matter so what the fuck C?


(a::b).c.d
a.(b::c).d // b is not a member, but a would have " : b" in it. i hate everything
this only exists cause the inherited struct doesn't have a name to . on.


after a subexpr:
Left-to-right   a.b.c.d  ->  ((a.b).c).d       a.b[c].d  ->  ((a.b)[c]).d 
         13   a[b]          Array access
     ^    a.b           Member access, automatically dereferences if 'a' is a pointer

before a subexpr:
Right-to-left   -'*a  ->  -('(*a))
12   -a            Unary minus
     ^    !a  ~a        Logical NOT and bitwise NOT
     ^    ?a           Dereference
     ^    *a            Pointer-to
     ^    cast(type)a   Cast

between subexprs:
Left-to-right   a*b/c%d  -> ((a*b)/c)%d
11   a*b a/b a%b   Multiplication, division, and remainder
    10   a+b a-b       Addition and subtraction
    9    a<<b  a>>b    Bitwise left shift and right shift
8    a&b           Bitwise AND
7    a^b           Bitwise XOR (exclusive or)
6    a|b           Bitwise OR  (inclusive or)
    5    a<b   a<=b    For relational operators < and ≤ respectively
               ^    a>b   a>=b    For relational operators > and ≥ respectively
4    a==b  a!=b    For relational operators = and ≠ respectively
3    a&&b          Logical AND
2    a||b          Logical OR

Precedence doesn't matter since assignment is a statement level operation like if, for, return etc.
 Associativity doesn't matter since you can only have one assignment per statement
   1    a=b               Direct assignment
     ^    a+=b  a-=b        Compound assignment by sum and difference
     ^    a*=b  a/=b  a%=b  Compound assignment by product, quotient, and remainder
     ^    a&=b  a^=b  a|=b  Compound assignment by bitwise AND, XOR, and OR
     ^    a<<=b  a>>=b      Compound assignment by bitwise left shift and right shift

lhs of assign can be: dereference, dot, variable

*/