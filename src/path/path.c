#include "path.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

int spot_save_path(char* save_path, char* file_name) { // Имя файла всегда поступает в формате [название предмета];[тип предмета]_[незначимые цифры]_[незначимые цифры].pdf
    if (!save_path || !file_name) return 0;
    char subject[256];
    char type[256];
    if (sscanf(file_name, "%255[^;];%255[^_]_%*s.pdf", subject, type) == 2) {
        
        printf("INFO: Extracted subject: '%s', type: '%s' from file name: '%s'\n", subject, type, file_name);
        snprintf(save_path, 512, "docs/%s/%s", type, subject); // Формируем путь сохранения в виде "docs/[тип предмета]/[название предмета]"
        return 1;
    } else {
        printf("WARNING: Failed to parse file name: '%s'. Expected format: [subject];[type]_[timestamp].pdf\n", file_name);
        return 0;
    }
}

int spot_type(char* type, char* file_name) { // Имя файла всегда поступает в формате [название предмета];[тип предмета]_[незначимые цифры]_[незначимые цифры].pdf
    if (!type || !file_name) return 0;
    char subject[256];
    if (sscanf(file_name, "%255[^;];%255[^_]_%*s.pdf", subject, type) == 2) {
        
        printf("INFO: Extracted subject: '%s', type: '%s' from file name: '%s'\n", subject, type, file_name);
        return 1;
    } else {
        printf("WARNING: Failed to parse file name: '%s'. Expected format: [subject];[type]_[timestamp].pdf\n", file_name);
        return 0;
    }
}

int spot_subject(char* subject, char* file_name) { 
if (!subject || !file_name) return 0; 
char type[256];
    if (sscanf(file_name, "%255[^;];%255[^_]_%*s.pdf", subject, type) == 2) {
        printf("INFO: Extracted subject: '%s', type: '%s' from file name: '%s'\n", subject, type, file_name);
        return 1;
    } else { 
        printf("WARNING: Failed to parse file name: '%s'. Expected format: [subject];[type]_[timestamp].pdf\n", file_name);
        return 0;
    }
}

int countdir(char *dir) {
    int count = 0;
    struct dirent *dp;
    DIR *fd;
    
    if ((fd = opendir(dir)) == NULL) {
printf("WARNING: Failed to open directory '%s'\n", dir);
        return 0;
    }
    while ((dp = readdir(fd)) != NULL) {
        if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) continue;
        count++;
    }
    closedir(fd);
    printf("INFO: Found %d files in directory '%s'\n", count, dir);
    return count;
}
