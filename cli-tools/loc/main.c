#include "wyn_runtime.h"

typedef struct {
    const char* name;
    long long code;
    long long blank;
    long long comment;
} FileStats;

void FileStats_cleanup(FileStats* obj) {
}

/* Generating 0 methods */

// Lambda functions (defined before use)
FileStats count_lines(const char* filename, const char* content);
long long display_stats(FileStats s);
long long wyn_main();

FileStats count_lines(const char* filename, const char* content) {
    long long code = 0;
    long long blank = 0;
    long long comment = 0;
    long long line_start = 0;
    long long len = string_len(content);
    for (long long i = 0; (i < len); i = (i + 1)) {
    if ((strcmp(wyn_string_charat(content, i), "\n") == 0)) {
    const char* line = string_substring(content, line_start, i);
    const char* trimmed = string_trim(line);
    if ((string_len(trimmed) == 0)) {
    blank = (blank + 1);
    } else {
    if (string_starts_with(trimmed, "//")) {
    comment = (comment + 1);
    } else {
    code = (code + 1);
    }
    }
    line_start = (i + 1);
    }
    }
    if ((line_start < len)) {
    code = (code + 1);
    }
    return *(FileStats*)wyn_arc_new(sizeof(FileStats), &(FileStats){.name = filename, .code = code, .blank = blank, .comment = comment})->data;
}

long long display_stats(FileStats s) {
    __auto_type total = ((s.code + s.blank) + s.comment);
    println(wyn_string_concat_safe("  ", s.name));
    println(wyn_string_concat_safe(wyn_string_concat_safe(wyn_string_concat_safe(wyn_string_concat_safe(wyn_string_concat_safe(wyn_string_concat_safe(wyn_string_concat_safe("    Code: ", int_to_string(s.code)), "  Blank: "), int_to_string(s.blank)), "  Comment: "), int_to_string(s.comment)), "  Total: "), int_to_string(total)));
}

long long wyn_main() {
    println("=== Lines of Code Counter ===");
    WynArray files = ({ WynArray __arr_0 = array_new(); array_push_str(&__arr_0, "../sample-apps/cli-tools/loc/main.wyn"); array_push_str(&__arr_0, "../sample-apps/cli-tools/todo-finder/main.wyn"); array_push_str(&__arr_0, "../sample-apps/cli-tools/word-counter/main.wyn"); array_push_str(&__arr_0, "../sample-apps/cli-tools/config-reader/main.wyn"); array_push_str(&__arr_0, "../sample-apps/cli-tools/md-toc/main.wyn"); __arr_0; });
    long long total_code = 0;
    long long total_blank = 0;
    long long total_comment = 0;
    for (long long i = 0; (i < 5); i = (i + 1)) {
    if (File_exists(array_get_str(files, i))) {
    const char* content = File_read(array_get_str(files, i));
    __auto_type stats = count_lines(array_get_str(files, i), content);
    display_stats(stats);
    total_code = (total_code + stats.code);
    total_blank = (total_blank + stats.blank);
    total_comment = (total_comment + stats.comment);
    }
    }
    println("");
    println("=== Summary ===");
    __auto_type total = ((total_code + total_blank) + total_comment);
    println("  Files:   5");
    println(wyn_string_concat_safe("  Code:    ", int_to_string(total_code)));
    println(wyn_string_concat_safe("  Blank:   ", int_to_string(total_blank)));
    println(wyn_string_concat_safe("  Comment: ", int_to_string(total_comment)));
    println(wyn_string_concat_safe("  Total:   ", int_to_string(total)));
    return 0;
}

