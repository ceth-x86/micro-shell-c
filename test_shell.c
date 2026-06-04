#include "shell.c"
#include <assert.h>

int tests_run = 0;
int tests_failed = 0;

#define RUN_TEST(test) do { \
    printf("Running %s... ", #test); \
    int failed_before = tests_failed; \
    test(); \
    tests_run++; \
    if (tests_failed == failed_before) { \
        printf("PASSED\n"); \
    } else { \
        printf("FAILED\n"); \
    } \
} while (0)

#define ASSERT_STR_EQ(got, expected) do { \
    if (got == NULL && expected == NULL) break; \
    if (got == NULL || expected == NULL || strcmp(got, expected) != 0) { \
        printf("\n  Assertion failed at %s:%d:\n    Expected: \"%s\"\n    Got:      \"%s\"\n", \
               __FILE__, __LINE__, expected ? expected : "NULL", got ? got : "NULL"); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_INT_EQ(got, expected) do { \
    if ((got) != (expected)) { \
        printf("\n  Assertion failed at %s:%d:\n    Expected: %d\n    Got:      %d\n", \
               __FILE__, __LINE__, (expected), (got)); \
        tests_failed++; \
        return; \
    } \
} while (0)

void test_preprocess_line() {
    char out[MAX_LINE_LEN * 2];

    preprocess_line("ls", out, sizeof(out));
    ASSERT_STR_EQ(out, "ls");

    preprocess_line("ls|grep x", out, sizeof(out));
    ASSERT_STR_EQ(out, "ls | grep x");

    preprocess_line("cat<file.txt", out, sizeof(out));
    ASSERT_STR_EQ(out, "cat < file.txt");

    preprocess_line("cmd>output", out, sizeof(out));
    ASSERT_STR_EQ(out, "cmd > output");

    preprocess_line("  cat  <in>  out  |  wc -l  ", out, sizeof(out));
    ASSERT_STR_EQ(out, "  cat  < in >  out  |  wc -l  ");
}

void test_parse_single_command() {
    Command cmd;
    char cmd_str[MAX_LINE_LEN];

    // Case 1: Simple command with arguments
    strcpy(cmd_str, "ls -la /tmp");
    parse_single_command(cmd_str, &cmd);
    ASSERT_INT_EQ(cmd.argc, 3);
    ASSERT_STR_EQ(cmd.argv[0], "ls");
    ASSERT_STR_EQ(cmd.argv[1], "-la");
    ASSERT_STR_EQ(cmd.argv[2], "/tmp");
    ASSERT_STR_EQ(cmd.argv[3], NULL);
    ASSERT_STR_EQ(cmd.input_file, NULL);
    ASSERT_STR_EQ(cmd.output_file, NULL);

    // Case 2: Input redirection
    strcpy(cmd_str, "cat < input.txt");
    parse_single_command(cmd_str, &cmd);
    ASSERT_INT_EQ(cmd.argc, 1);
    ASSERT_STR_EQ(cmd.argv[0], "cat");
    ASSERT_STR_EQ(cmd.argv[1], NULL);
    ASSERT_STR_EQ(cmd.input_file, "input.txt");
    ASSERT_STR_EQ(cmd.output_file, NULL);

    // Case 3: Output redirection
    strcpy(cmd_str, "grep foo > out.txt");
    parse_single_command(cmd_str, &cmd);
    ASSERT_INT_EQ(cmd.argc, 2);
    ASSERT_STR_EQ(cmd.argv[0], "grep");
    ASSERT_STR_EQ(cmd.argv[1], "foo");
    ASSERT_STR_EQ(cmd.argv[2], NULL);
    ASSERT_STR_EQ(cmd.input_file, NULL);
    ASSERT_STR_EQ(cmd.output_file, "out.txt");

    // Case 4: Both input and output redirection
    strcpy(cmd_str, "cmd < in.txt > out.txt");
    parse_single_command(cmd_str, &cmd);
    ASSERT_INT_EQ(cmd.argc, 1);
    ASSERT_STR_EQ(cmd.argv[0], "cmd");
    ASSERT_STR_EQ(cmd.argv[1], NULL);
    ASSERT_STR_EQ(cmd.input_file, "in.txt");
    ASSERT_STR_EQ(cmd.output_file, "out.txt");
}

int main() {
    printf("=== Starting Shell Unit Tests ===\n");
    
    RUN_TEST(test_preprocess_line);
    RUN_TEST(test_parse_single_command);
    
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed == 0) {
        printf("All tests passed successfully!\n");
        return 0;
    } else {
        printf("Some tests failed.\n");
        return 1;
    }
}
