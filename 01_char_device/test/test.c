#include <stdio.h>
#include <string.h>

int main() {
    int  result     = 0;
    FILE *fp        = NULL;
    char filename[] = "/dev/debimate";
    char str1[]     = "abcde";
    char str2[]     = "fghij";
    char buf[11]    = {'\0'};

    fp = fopen(filename, "w+");
    if(fp == NULL) {
        perror("fopen");
        return -1;
    }

    result = fwrite(str1, sizeof(char), 5, fp);
    result = fwrite(str2, sizeof(char), 5, fp);
    if(result != 5) {
        perror("fwrite"); /* サンプルのため最後の書き込みのみチェックする */
        fclose(fp);
        return -1;
    }
    printf("Write down \"%s\" and \"%s\" to %s\n",
            str1, str2, filename);

	result = fread(buf, sizeof(char), 10, fp);
    if(result != 10) {
        perror("fread");
        fclose(fp);
        return -1;
    }
    printf("Read \"%s(10byte)\" from %s\n", buf, filename);

    fclose(fp);
    return 0;
}
