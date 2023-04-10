
#include "bo.h"

#if _INTERNAL_BUILD

internal inline void do_assert(char *expression, char *file_path, int line_number);
#define ___Stringify(a) #a
#define assert_m(e,m) if (!(e)) { do_assert((m), __FILE__, __LINE__); __debugbreak(); }
#define assert(e) assert_m(e, ___Stringify(e))

#define BoBadAssert(expression) if (!(expression)) { *(int *)0 = 0; }
//#define BoBadAssert(expression) if (!(expression)) { __debugbreak(); }
#define CompileTimeAssert(expression) typedef char ___CompileTimeAssertFailed[(expression) ? 1 : -1]
#define InvalidDefaultCase(s) Default: assert(!s)
#define IfDebug(...) __VA_ARGS__ // use for debug only stuff

#else // _INTERNAL_BUILD
#define assert(...)
#define BoBadAssert(...)
#define CompileTimeAssert(...)
#define InvalidDefaultCase(...)
#define IfDebug(...)
#endif // _INTERNAL_BUILD

#define ShouldntReachHere assert(!"Shouldn't reach here")

#define BO_STRING_C_STYLE 1
#define BO_STRING_SPRINT 1
#include "bo_string.cpp"

#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#include "libs/stb_sprintf_bo_string.h"
#define sprint stbsp_sprintf
#define snprint stbsp_snprintf
#define vsnprint stbsp_vsnprintf

#undef NULL // fuck you crt

#define atomic_read(a) (a)

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define cast_s8(e)  (((( S8_MIN <= (e)) && ((e) <=  S8_MAX)) ? 0 : (*(int *)0 = 0)), ((s8 )(e)))
#define cast_s16(e) ((((S16_MIN <= (e)) && ((e) <= S16_MAX)) ? 0 : (*(int *)0 = 0)), ((s16)(e)))
#define cast_s32(e) ((((S32_MIN <= (e)) && ((e) <= S32_MAX)) ? 0 : (*(int *)0 = 0)), ((s32)(e)))
#define cast_s64(e) ((((S64_MIN <= (e)) && ((e) <= S64_MAX)) ? 0 : (*(int *)0 = 0)), ((s64)(e)))
#define cast_u8(e)  ((((      0 <= (e)) && ((e) <=  U8_MAX)) ? 0 : (*(int *)0 = 0)), ((u8 )(e)))
#define cast_u16(e) ((((      0 <= (e)) && ((e) <= U16_MAX)) ? 0 : (*(int *)0 = 0)), ((u16)(e)))
#define cast_u32(e) ((((      0 <= (e)) && ((e) <= U32_MAX)) ? 0 : (*(int *)0 = 0)), ((u32)(e)))
#define cast_u64(e) ((((      0 <= (e)) && ((e) <= U64_MAX)) ? 0 : (*(int *)0 = 0)), ((u64)(e)))

#define malloc_array(T, n) ((T *)malloc(sizeof(T) * (n)))


//~ Memory Arena

struct Arena_Chunk {
    s64 size;
    s64 occupied; // maybe use this as an 'unreferred' counter for free_chunks?
    u8 *data;
    Arena_Chunk *next;
};

struct Arena {
    Arena_Chunk *chunks;
    Arena_Chunk *free_chunks;
    s64 min_chunk_size = 128*1024;
#if _INTERNAL_BUILD
    s64 high_water_mark;
    bool locked;
#endif
};

internal inline s64
round_up_s64(s64 x, s64 b) {
    x += b - 1;
    x -= x % b;
    return x;
}

internal inline void *
round_up_pointer(void *x, s64 b) {
    s64 result = round_up_s64((s64)x, b);
    return *(void **)&result;
}

internal inline s64
get_rounding_diff(s64 x, s64 b) {
    s64 rounded = round_up_s64(x, b);
    return rounded - x;
}

internal inline s64
get_pointer_rounding_diff(void *x, s64 b) {
    void *rounded = round_up_pointer(x, b);
    return ((u8 *)rounded - (u8 *)x);
}

internal inline u64
next_power_of_two(u64 a) {
    a -= 1;
    a |= a >> 1;
    a |= a >> 2;
    a |= a >> 4;
    a |= a >> 8;  // remove this and below for 8-bit version
    a |= a >> 16; // remove this and below for 16-bit version
    a |= a >> 32; // remove this for 32-bit version
    a += 1;
    return a;
}

internal Arena_Chunk *
arena__new_chunk(Arena *arena, s64 requested_size) {
    Arena_Chunk *chunk = arena->free_chunks;
    Arena_Chunk *prev = null;
    while (chunk) {
        if (chunk->size >= requested_size) {
            if (prev) {
                prev->next = chunk->next;
            } else {
                arena->free_chunks = chunk->next;
            }
            break;
        }
        prev = chunk;
        chunk = chunk->next;
    }
    
    if (!chunk) {
        s64 size_needed = requested_size + sizeof(Arena_Chunk);
        s64 full_size = max(size_needed, arena->min_chunk_size);
        full_size = round_up_s64(full_size, 4*1024);
        s64 body_size = full_size - sizeof(Arena_Chunk);
        
        u8 *data = (u8 *)malloc(full_size);
        assert(data);
        
        chunk = (Arena_Chunk *)data;
        chunk->data = (u8 *)(chunk + 1);
        chunk->occupied = 0;
        chunk->size = body_size;
    }
    
    assert(chunk->occupied == 0);
    chunk->next = arena->chunks;
    arena->chunks = chunk;
    return chunk;
}

internal inline Str
xxx_arena_get_free_memory(Arena *arena) {
#if _INTERNAL_BUILD
    assert(!arena->locked);
    arena->locked = true;
#endif
    Arena_Chunk *chunk = arena->chunks;
    if (!chunk)  chunk = arena__new_chunk(arena, 0);
    
    Str s;
    s.count =         chunk->size - chunk->occupied;
    s.data  = (char *)chunk->data + chunk->occupied;
    return s;
}

internal inline void
xxx_arena_commit_used_bytes(Arena *arena, s64 bytes_used) {
#if _INTERNAL_BUILD
    assert(arena->locked);
    arena->locked = false;
#endif
    assert(bytes_used >= 0 && bytes_used + arena->chunks->occupied <= arena->chunks->size);
    arena->chunks->occupied += bytes_used;
}

internal inline void
arena_reset(Arena *arena) {
    if (arena->chunks) {
        IfDebug(s64 total_occupied = 0);
        Arena_Chunk *chunk = arena->chunks;
        while (chunk->next) {
            auto next = chunk->next;
            
            chunk->occupied = 0;
            chunk->next = arena->free_chunks;
            arena->free_chunks = chunk;
            
            IfDebug(total_occupied += chunk->occupied);
            chunk = next;
        }
        IfDebug(total_occupied += chunk->occupied;
                if (arena->high_water_mark < total_occupied) {
                    arena->high_water_mark = total_occupied;
                });
        chunk->occupied = 0;
        arena->chunks = chunk;
    }
}

internal inline void *
arena_alloc(Arena *arena, s64 bytes, s64 alignment = 8) {
    assert(bytes >= 0);
    void *result = null;
    
    if (bytes > 0) {
        Arena_Chunk *chunk = arena->chunks;
        if (!chunk) {
            chunk = arena__new_chunk(arena, bytes);
        }
        
        s64 rounding_diff = get_pointer_rounding_diff(chunk->data + chunk->occupied, alignment);
        
        if (chunk->occupied + rounding_diff + bytes > chunk->size) {
            chunk = arena__new_chunk(arena, bytes);
            rounding_diff = 0;
        }
        result = chunk->data + rounding_diff + chunk->occupied;
        chunk->occupied += rounding_diff + bytes;
    }
    return result;
}

internal Str
arena_vprint(Arena *arena, char *format, va_list args) {
    // @Volatile: don't alloc from arena before the commit
    Str free = xxx_arena_get_free_memory(arena);
    char *data = free.data;
    
    // length excludes zero-terminator
    int length = stbsp_vsnprintf(data, cast_s32(free.count), format, args);
    if (length + 1 <= free.count) {
        xxx_arena_commit_used_bytes(arena, length + 1); // + 1 for zero-terminator
    } else {
        data = (char *)arena_alloc(arena, length + 1);
        length = stbsp_vsnprintf(data, length + 1, format, args);
    }
    assert(data[length] == '\0');
    
    return Str{length, data};
}

internal Str
arena_print(Arena *arena, char *format, ...) {
    va_list args;
    va_start(args, format);
    auto result = arena_vprint(arena, format, args);
    va_end(args);
    return result;
}

#define arena_alloc_array(arena, T, count) ((T *)arena_alloc((arena), sizeof(T)*(count), sizeof(T)))

internal inline void *
aalloc(Arena *arena, s64 bytes, s64 alignment = 8) {
    return arena_alloc(arena, bytes, alignment);
}

internal Str
aprint(Arena *arena, char *format, ...) {
    va_list args;
    va_start(args, format);
    auto result = arena_vprint(arena, format, args);
    va_end(args);
    return result;
}

//- Scoped arena scratch buffer

global Arena g_scoped_arena;

struct Scoped_Arena {
    Arena_Chunk *saved_chunk;
    s64 saved_occupied;
    
    operator Arena *() {
        return &g_scoped_arena;
    }
    
    Scoped_Arena() {
        Arena_Chunk *chunk = g_scoped_arena.chunks;
        this->saved_chunk    = chunk;
        this->saved_occupied = chunk ? chunk->occupied : 0;
    }
    
    // @Copypaste: This is almost identical to arena_reset()
    // Can they be factored together?
    ~Scoped_Arena() {
        if (!g_scoped_arena.chunks) return;
        Arena_Chunk *chunk = g_scoped_arena.chunks;
        
        while (chunk->next && chunk != this->saved_chunk) {
            auto next = chunk->next;
            
            chunk->occupied = 0;
            chunk->next = g_scoped_arena.free_chunks;
            g_scoped_arena.free_chunks = chunk;
            
            chunk = next;
        }
        
        chunk->occupied = this->saved_occupied;
        g_scoped_arena.chunks = chunk;
    }
};



//- Frame arena

// @Cleanup: Maybe remove this frame arena entirely? Since there is no concept of a frame in a compiler.
// NOTE(bo): The goal of this frame arena is to hold all scratch/temporary allocations
// you might want in a frame wihin a single chunk. The high_water_mark member is there
// for you to be able to see the max amount of bytes allocated in a frame. You can use
// this to adjust min_chunk_size such that the frame arena doesn't have to allocate
// additional chunks in the majority of cases.
global Arena g_frame_arena;

internal inline void *
talloc(s64 bytes) {
    return arena_alloc(&g_frame_arena, bytes);
}

#define _talloc_array(T, count) ((T *)arena_alloc_array(&g_frame_arena, T, (count)))

internal inline Str
vtprint(char *format, va_list args) {
    return arena_vprint(&g_frame_arena, format, args);
}

internal Str
tprint(char *format, ...) {
    va_list args;
    va_start(args, format);
    auto result = arena_vprint(&g_frame_arena, format, args);
    va_end(args);
    return result;
}





internal inline bool write_stdout(Str s);

internal int
vprint(char *format, va_list args) {
    Str s = arena_vprint(&g_frame_arena, format, args);
    
    // @Incomplete @Robustness: test this
    write_stdout(s);
    
    return cast_s32(s.count);
}

// This is specifically 'printf' as a @Hack to use the complier format string checking
internal int
printf(char *format, ...) {
    va_list args;
    va_start(args, format);
    auto result = vprint(format, args);
    va_end(args);
    return result;
}
// cast to char * so that string literals match to my printf and not stdio's
#define print(format, ...) printf((char *)format, ##__VA_ARGS__)


template<typename T>
struct Array {
    s64 count;
    T *data;
    s64 capacity;
    
    void resize(s64 new_cap) {
        if (new_cap <= 0) {
            free(this->data);
            *this = {};
            return;
        }
        if (new_cap > this->capacity) {
            new_cap = next_power_of_two(new_cap);
            this->data = (T *)realloc(this->data, new_cap*sizeof(T));
            assert_m(this->data, "Failed to realloc an Array");
        }
        this->capacity = new_cap;
        if (this->count > this->capacity) {
            this->count = this->capacity;
        }
    }
    
    inline void add_new_elements(s64 number_of_elements_to_add) {
        s64 new_count = this->count + number_of_elements_to_add;
        if (new_count > this->capacity) this->resize(new_count);
        this->count = new_count;
    }
    
    inline void push(T value) {
        this->add_new_elements(1);
        this->data[this->count - 1] = value;
    }
    
    inline T pop() {
        assert(this->count > 0);
        this->count -= 1;
        return this->data[this->count];
    }
    
    inline T &operator[](s64 i) {
        return this->data[i];
    }
};

// @Incomplete: Make a version that takes an Arena to use instead of Array
struct String_Builder : public Array<char> {
    void append_string(char *format, ...) {
        va_list args;
        va_start(args, format);
        
        char *free_data  = this->data     + this->count;
        s64   free_count = this->capacity - this->count;
        
        // length excludes zero-terminator
        int length = stbsp_vsnprintf(free_data, cast_s32(free_count), format, args);
        // + 1 for zero-terminator
        if (length + 1 > free_count) {
            this->resize(this->count + length + 1);
            free_data  = this->data     + this->count;
            free_count = this->capacity - this->count;
            length = stbsp_vsnprintf(free_data, cast_s32(free_count), format, args);
        }
        // stbsp_vsnprintf always wants enough space for a zero-termainator,
        // but we don't want it ourselves so we omit the + 1 here.
        assert(free_data[length] == '\0');
        this->count += length;
        
        va_end(args);
    }
};

#include "lexer.h"
#include "ast.h"

#include "parser.cpp"
#include "typestuff.cpp"
#include "c_writer.cpp"

#include <windows.h>



struct Scoped_Lock {
    CRITICAL_SECTION *crit_sect;
    
    Scoped_Lock(CRITICAL_SECTION *_crit_sect) {
        crit_sect = _crit_sect;
        EnterCriticalSection(crit_sect);
    }
    
    ~Scoped_Lock() {
        LeaveCriticalSection(crit_sect);
    }
};

internal inline bool
write_stdout(Str s) {
    assert(s.data);
    DWORD bytes_written = 0;
    auto success = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s.data, cast_u32(s.count), &bytes_written, null);
    bool result = success && (s.count == bytes_written);
    if (!result) {
        report_error_noloc("Internal compiler error: WriteFile failed in %s.\nGetLastError(): %d, s.count: %lld, bytes_written: %d", __func__, GetLastError(), s.count, bytes_written);
    }
    return result;
}

internal inline s64
read_stdin(char *buffer, u32 size) {
    assert(buffer);
    assert(size >= 256);
    // The windows console will accept a minimum of 256 character as input,
    // including \r\n, no matter what we set size to.
    DWORD number_of_bytes_read = 0;
    auto success = ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, size, &number_of_bytes_read, 0);
    if (success) return (s64)number_of_bytes_read;
    
    report_error_noloc("Internal compiler error: ReadFile failed in %s.\nGetLastError(): %d, size: %d, number_of_bytes_read: %d", __func__, GetLastError(), size, number_of_bytes_read);
    return -1;
}

internal inline void
free_file_memory(void *memory) {
    if (!memory) return;
    auto success = VirtualFree(memory, 0, MEM_RELEASE);
    if (!success) {
        report_error_noloc("Internal compiler error: VirtualFree failed in %s.\nGetLastError(): %d, memory: %p", __func__, GetLastError(), memory);
    }
}

internal Str
read_entire_file(char *filename) {
    
    auto file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file_handle == INVALID_HANDLE_VALUE) {
        report_error_noloc("Internal compiler error: CreateFileA failed in %s.\nGetLastError(): %d, filename: \"%s\"", __func__, GetLastError(), filename);
        return {};
    }
    defer { CloseHandle(file_handle); };
    
    LARGE_INTEGER file_size;
    auto success = GetFileSizeEx(file_handle, &file_size);
    if (!success) {
        report_error_noloc("Internal compiler error: GetFileSizeEx failed in %s.\nGetLastError(): %d, file_size: %lld", __func__, GetLastError(), file_size.QuadPart);
        return {};
    }
    
    auto file_size32 = cast_u32(file_size.QuadPart);
    
    Str result = {};
    result.data = (char *)VirtualAlloc(0, file_size32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    if (result.data) {
        DWORD bytes_read;
        if (ReadFile(file_handle, result.data, file_size32, &bytes_read, 0) &&
            (file_size32 == bytes_read)) {
            result.count = file_size32;
        } else {
            free_file_memory(result.data);
            report_error_noloc("Internal compiler error: ReadFile failed in %s.\nGetLastError(): %d, file_size32: %d, bytes_read: %d", __func__, GetLastError(), file_size32, bytes_read);
            result = {};
        }
    } else {
        report_error_noloc("Internal compiler error: VirtualAlloc failed in %s.\nGetLastError(): %d, file_size32: %d", __func__, GetLastError(), file_size32);
    }
    return result;
}

internal bool
write_entire_file(char *filename, Str data) {
    auto file_handle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (file_handle == INVALID_HANDLE_VALUE) {
        report_error_noloc("Internal compiler error: CreateFileA failed in %s.\nGetLastError(): %d, filename: \"%s\"", __func__, GetLastError(), filename);
        return false;
    }
    
    defer { CloseHandle(file_handle); };
    DWORD bytes_written;
    bool write_success = WriteFile(file_handle, data.data, cast_u32(data.count), &bytes_written, 0);
    if (write_success && (data.count == bytes_written)) return true;
    
    report_error_noloc("Internal compiler error: WriteFile failed in %s.\nGetLastError(): %d, data.count: %lld, bytes_written: %d", __func__, GetLastError(), data.count, bytes_written);
    return false;
}

internal bool
create_empty_file(char *filename) {
    auto file_handle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (file_handle == INVALID_HANDLE_VALUE) {
        report_error_noloc("Internal compiler error: CreateFileA failed in %s.\nGetLastError(): %d, filename: \"%s\"", __func__, GetLastError(), filename);
        return false;
    }
    CloseHandle(file_handle);
    return true;
}

internal bool
append_to_existing_file(char *filename, Str data) {
    auto file_handle = CreateFileA(filename, GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (file_handle == INVALID_HANDLE_VALUE) {
        report_error_noloc("Internal compiler error: CreateFileA failed in %s.\nGetLastError(): %d, filename: \"%s\"", __func__, GetLastError(), filename);
        return false;
    }
    
    defer { CloseHandle(file_handle); };
    if (SetFilePointer(file_handle, 0, 0, FILE_END) == INVALID_SET_FILE_POINTER) {
        report_error_noloc("Internal compiler error: SetFilePointer failed in %s.\nGetLastError(): %d", __func__, GetLastError());
        return false;
    }
    
    DWORD bytes_written;
    bool write_success = WriteFile(file_handle, data.data, (DWORD)data.count, &bytes_written, 0);
    if (write_success && (data.count == bytes_written)) return true;
    
    report_error_noloc("Internal compiler error: WriteFile failed in %s.\nGetLastError(): %d, data.count: %lld, bytes_written: %d", __func__, GetLastError(), data.count, bytes_written);
    return false;
}

internal inline DWORD
setup_windows_console_for_colored_output() {
    auto stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stdout_handle == INVALID_HANDLE_VALUE) {
        print("GetStdHandle error %d. Color output disabled.\n\n", GetLastError());
        return (DWORD)-1;
    }
    
    DWORD output_mode = 0;
    if (!GetConsoleMode(stdout_handle, &output_mode)) {
        print("GetConsoleMode error %d. Color output disabled.\n\n", GetLastError());
        return (DWORD)-1;
    }
    
    DWORD initial_output_mode = output_mode;
    output_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    
    if (!SetConsoleMode(stdout_handle, output_mode)) {
        print("SetConsoleMode error %d. Color output disabled.\n\n", GetLastError());
        return (DWORD)-1;
    }
    return initial_output_mode;
}

internal inline void
reset_windows_console(DWORD initial_output_mode) {
    if (initial_output_mode == (DWORD)-1) return;
    if (!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), initial_output_mode)) {
        print("Internal compiler error: SetConsoleMode failed in %s.\nGetLastError(): %d\n\n", __func__, GetLastError());
    }
}

#include <dbghelp.h>

internal inline void
do_assert(char *expression, char *assert_file_path, int assert_line_number) {
    print("*** Assert failed: %s\n"
          "    In file: %s\n"
          "    On line: %d\n", expression, assert_file_path, assert_line_number);
    
    auto machine_type = IMAGE_FILE_MACHINE_AMD64;
    auto process = GetCurrentProcess();
    auto thread = GetCurrentThread();
    if (SymInitialize(process, null, TRUE) == FALSE) {
        print("ERROR: SymInitialize failed in %s.\nGetLastError(): %d\n",  __func__, GetLastError());
        BoBadAssert(false);
        return;
    }
    SymSetOptions(SYMOPT_LOAD_LINES);
    
    CONTEXT context_record = {};
    context_record.ContextFlags = CONTEXT_FULL;
    RtlCaptureContext(&context_record);
    
    STACKFRAME stack_frame = {};
    stack_frame.AddrPC.Mode      = AddrModeFlat;
    stack_frame.AddrFrame.Mode   = AddrModeFlat;
    stack_frame.AddrStack.Mode   = AddrModeFlat;
    stack_frame.AddrPC.Offset    = context_record.Rip;
    stack_frame.AddrFrame.Offset = context_record.Rbp;
    stack_frame.AddrStack.Offset = context_record.Rsp;
    
    print("\nStack trace:\n");
    while (StackWalk(machine_type, process, thread, &stack_frame, &context_record, null, SymFunctionTableAccess, SymGetModuleBase, null)) {
#if 0
        Str module_string = sl("MODULE_NAME_ERROR");
        char module_string_buffer[MAX_PATH + 1];
        
        DWORD64 module_base = SymGetModuleBase(process, stack_frame.AddrPC.Offset);
        if (module_base) {
            auto length = GetModuleFileNameA((HINSTANCE)module_base, module_string_buffer, ArrayCount(module_string_buffer));
            if (length > 0) {
                // length is size of buffer and includes null if buffer was too small
                if (length == ArrayCount(module_string_buffer)) length -= 1;
                module_string = {length, module_string_buffer};
                ForIIR (i, (int)(module_string.count-1), 0) {
                    if (module_string.data[i] == '\\') {
                        module_string = advance(module_string, i + 1);
                        break;
                    }
                }
            }
        }
#endif
        
        Str proc_name = sl("PROC_NAME_ERROR");
        
        const int bytes_reserved_for_symbol_name = 256;
        // Name[1] is the last member of SYMBOL_INFO
        // 1 byte is already reserved by Name[1], hence the -1
        char symbol_info_buffer[sizeof(SYMBOL_INFO) + bytes_reserved_for_symbol_name-1] = {};
        
        auto symbol_info = (SYMBOL_INFO *)symbol_info_buffer;
        symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol_info->MaxNameLen = bytes_reserved_for_symbol_name;
        
        DWORD64 __ignored_offset = 0;
        if (SymFromAddr(process, stack_frame.AddrPC.Offset, &__ignored_offset, symbol_info)) {
            proc_name = {symbol_info->NameLen, symbol_info->Name};
        }
        
        char *file_path = "FILE_NAME_ERROR";
        s32 line_number = -1;
        
        IMAGEHLP_LINE file_and_line_info = {sizeof(IMAGEHLP_LINE)};
        DWORD __ignored_line_offset = 0;
        if (SymGetLineFromAddr(process, stack_frame.AddrPC.Offset, &__ignored_line_offset, &file_and_line_info)) {
            file_path = file_and_line_info.FileName;
            line_number = file_and_line_info.LineNumber;
        }
        
        print("%s(%d): %S\n", file_path, line_number, proc_name);
    }
}

int main(int _arg_count, char **_args) {
    LARGE_INTEGER performance_counter_frequency = {};
    auto success = QueryPerformanceFrequency(&performance_counter_frequency);
    assert(success);
    
    LARGE_INTEGER start_counter = {};
    success = QueryPerformanceCounter(&start_counter);
    assert(success);
    
    DWORD initial_output_mode = setup_windows_console_for_colored_output();
    defer { reset_windows_console(initial_output_mode); };
    chai.console_can_output_colors = (initial_output_mode != (DWORD)-1); // @MagicNumber
    
    auto arg_count = _arg_count - 1;
    auto args = _args + 1;
    
    if (arg_count < 1) {
        print("Usage: <input> [output]\n"
              "  <input>: Code file to be compiled.\n"
              " [output]: File to output the generated C code to. Will be based on <input> if omitted.\n");
        return 2;
    }
    
    auto fully_pathed_input_file = c_style_to_string(args[0]); // @Incomplete @Hack
    ForI (i, 0, fully_pathed_input_file.count) {
        if (fully_pathed_input_file.data[i] == '/') fully_pathed_input_file.data[i] = '\\';
    }
    
    if (arg_count > 1) {
        chai.fully_pathed_output_file_name = c_style_to_string(args[1]);
        ForI (i, 0, chai.fully_pathed_output_file_name.count) {
            if (chai.fully_pathed_output_file_name.data[i] == '/') chai.fully_pathed_output_file_name.data[i] = '\\';
        }
    } else {
#if 0
        auto last_slash_index = index_of_last_char(fully_pathed_input_file, '\\');
        if (last_slash_index == -1) last_slash_index = index_of_last_char(fully_pathed_input_file, '/');
        
        s64 start_of_filename_index = 0;
        if (start_of_filename_index >= 0) start_of_filename_index = last_slash_index + 1;
        auto filename = advance(fully_pathed_input_file, start_of_filename_index);
        
        auto chars_to_copy = index_of_last_char(filename, '.');
        if (chars_to_copy == -1) chars_to_copy = filename.count;
        
        chai.output_file_path.count = chars_to_copy + 2;
        chai.output_file_path.data = (char *)alloc(chai.string_bucket_array, chai.output_file_path.count + 1);
        
        ForI (i, 0, chars_to_copy) chai.output_file_path.data[i] = filename.data[i];
        chai.output_file_path.data[chai.output_file_path.count-2] = '.';
        chai.output_file_path.data[chai.output_file_path.count-1] = 'c';
        chai.output_file_path.data[chai.output_file_path.count] = 0;
#else
        print("Please specify an output file");
        Sleep(1000); // @Temporary @Hack
        return 2;
#endif
    }
    {
        auto last_slash_index = index_of_last_char(fully_pathed_input_file, '\\');
        assert(last_slash_index != -1);
        chai.path_to_input_file.count = last_slash_index+1;
        chai.path_to_input_file.data  = fully_pathed_input_file.data;
    }
    
    print("Started parsing %S\n\n", fully_pathed_input_file);
    
    Array<Ast *> statements = {};
    statements.resize(258);
    
    parse_everything(statements, fully_pathed_input_file);
    if (!chai.error_was_reported) {
        print("Parsing done\n\n");
        
        typecheck_everything(statements);
        if (!chai.error_was_reported) {
            print("Type stuff done\n\n");
            write_c_file(statements);
            if (!chai.error_was_reported) {
                print("Writing to %S done\n\n", chai.fully_pathed_output_file_name);
            }
        }
    }
    
    
    
    if (!chai.error_was_reported) {
        LARGE_INTEGER end_counter = {};
        success = QueryPerformanceCounter(&end_counter);
        assert(success);
        f32 seconds_elapsed = (f32)(end_counter.QuadPart - start_counter.QuadPart) /
        (f32)performance_counter_frequency.QuadPart;
        print("\nCompliation finished in %f seconds\n", seconds_elapsed);
        print("SUCCESS\nSUCCESS\nSUCCESS\nSUCCESS\nSUCCESS\n");
    }
    char aksjhdgfkajhsgdfka[300];
    read_stdin(aksjhdgfkajhsgdfka, sizeof(aksjhdgfkajhsgdfka));
    
    return chai.error_was_reported ? 1 : 0;
}