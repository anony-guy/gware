#include <stdio.h>
#include <regex.h>

int main() {
    regex_t regex;
    int ret = regcomp(&regex, "^a.*b$", 0);
    if (!ret) {
        ret = regexec(&regex, "a test b", 0, NULL, 0);
        if (!ret) {
            printf("Match\n");
        }
    }
    return 0;
}
