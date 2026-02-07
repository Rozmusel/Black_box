#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int pdf_merge(const char* file1, const char* file2, const char* output);
int pdf_add_watermark(const char* file, const char* output);

#ifdef __cplusplus
}
#endif
