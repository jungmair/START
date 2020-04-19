#pragma once
//generating all datasets freshly makes testing very slow
//-> wrapper for dataset generators implementing caching
#include <cstring>
#include <type_traits>
#include <string>
#include <sstream>
#include <fstream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "string.tcc"
#include <algorithm>

template<typename X>
struct cached {
    using type_t=typename X::type;
    static_assert(!std::is_same<type_t, std::string>());

    template<typename T>
    struct generator_name_impl {
        static std::string name() {
            return typeid(T).name();
        }
    };

    template<const char *path>
    struct generator_name_impl<file_string_data_set<path>> {
        static std::string name() {
            //generate useful string for file dataset
            std::string x = typeid(file_string_data_set<path>).name();
            x += "_";
            x += path;
            std::replace(x.begin(), x.end(), '/', '_');
            return x;
        }
    };

    static void write(std::vector<type_t> &vec, const std::string &cache_file) {
        //create file
        std::string filename = cache_file + ".vec.bin";
        int fd = open(filename.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        size_t len = vec.size() * sizeof(type_t);
        if (ftruncate(fd, len)) {
            throw std::system_error(errno, std::generic_category(), "ftruncate failed");
        }
        //map file to memory, copy data from vector to mapped file, unmap+ close file
        void *addr = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        memcpy(addr, &vec[0], len);
        munmap(addr, len);
        close(fd);
    }

    static void fill(std::vector<type_t> &vec, const std::string &cache_file) {
        //open file
        std::string filename = cache_file + ".vec.bin";
        int fd = open(filename.c_str(), O_RDONLY);
        size_t len = vec.size() * sizeof(type_t);
        //map file to memory, copy data into vector, unmap it and close it
        void *addr = mmap(nullptr, len, PROT_READ, MAP_PRIVATE, fd, 0);
        memcpy(&vec[0], addr, len);
        munmap(addr, len);
        close(fd);
    }

    static bool file_exists(const std::string &filename) {
        //does file exist?
        std::ifstream istream(filename);
        return istream.is_open();
    }

    static void fill(std::vector<type_t> &vec) {
        //generate unique name for cache_file:
        std::string generator_name = generator_name_impl<X>::name();
        std::stringstream ss;
        ss << generator_name << "_" << vec.size();
        std::string filename = locate_cache(ss.str());
        //check if file exists->dataset cached
        if (file_exists(filename + ".vec.bin")) {
            //in cache->fill vector with file data
            fill(vec, filename);
        } else {
            //generate data now
            X::fill(vec);
            //write it to the cache
            write(vec, filename);
        }
    }
};
