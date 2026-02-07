#include "archiver.h"
#include <zip.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <cerrno>

namespace fs = std::filesystem;

bool add_file_to_zip(zip_t* za, const fs::path& file_path, const std::string& archive_name) {
    zip_source_t* zs = zip_source_file(za, file_path.c_str(), 0, -1);
    if (!zs) {
        std::cerr << "zip_source_file failed for " << file_path << ": " << zip_strerror(za) << "\n";
        return false;
    }

    zip_int64_t idx = zip_file_add(za, archive_name.c_str(), zs, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
    if (idx < 0) {
        std::cerr << "zip_file_add failed for " << archive_name << ": " << zip_strerror(za) << "\n";
        zip_source_free(zs);
        return false;
    }

    return true;
}

extern "C" int dir_archiver(const char* input, const char* output) {
    if (!input || !output) {
        std::cerr << "Invalid arguments: input and output must not be NULL\n";
        return 2;
    }

    fs::path src = fs::path(input);
    fs::path out = fs::path(output);

    if (!fs::exists(src) || !fs::is_directory(src)) {
        std::cerr << "Source must be an existing directory: " << src << "\n";
        return 3;
    }

    int errorp = 0;
    zip_t* za = zip_open(output, ZIP_CREATE | ZIP_TRUNCATE, &errorp);
    if (!za) {
        std::cerr << "zip_open failed with error code: " << errorp << "\n";
        return 4;
    }

    for (auto it = fs::recursive_directory_iterator(src); it != fs::recursive_directory_iterator(); ++it) {
        try {
            const fs::path& p = it->path();
            if (fs::is_regular_file(p)) {
                fs::path rel = fs::relative(p, src);
                std::string archive_path = rel.generic_string();
                
                if (!add_file_to_zip(za, p, archive_path)) {
                    std::cerr << "Failed to add file: " << p << "\n";
                }
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "Exception while processing: " << ex.what() << "\n";
        }
    }

    if (zip_close(za) < 0) {
        std::cerr << "zip_close failed\n";
        zip_discard(za);
        return 5;
    }

    std::cout << "Created " << output << " from folder " << src << "\n";
    return 0;
}
