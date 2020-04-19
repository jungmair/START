#pragma once
//paths to resource and cache folders depend on the relative location of the program binary
//this helper functions locates cache and resource folders also in parent folders
#include <sys/stat.h>

inline std::string locate(std::string basefile, std::string folder) {
    std::string str = folder;
    while (true) {
        struct stat info;

        if (stat(str.c_str(), &info) != 0 || !(info.st_mode & S_IFDIR)) {
            str = "../" + str;

        } else {
            return str + "/" + basefile;
        }
    }
}

static std::string locate_resource(std::string basefile) {
    return locate(basefile, "tests/resources");
}

static std::string locate_cache(std::string basefile) {
    return locate(basefile, "tests/cache");
}
