#pragma once
//utility for generating string datasets from random numbers or files
#include <fstream>
#include <unordered_set>
#include "util/random_int.tcc"
#include "util/locate_resources.h"
#include "../../config/key_props.tcc"

struct random_string_data_set {
    using type=std::string;

    static void fill(std::vector<std::string> &vec) {
        //generate random strings from random numbers
        random_int<uint64_t> r;
        std::unordered_set<uint64_t> set;
        for (size_t i = 0; i < vec.size(); i++) {
            uint64_t val = r.next();
            while (set.count(val)) {
                val = r.next();
            }
            std::stringstream sstream;
            sstream << val << "######";
            vec[i] = sstream.str();

            set.insert(val);
        }
    }
};

template<const char *path>
struct file_string_data_set {
    using type=std::string;

    static void fill(std::vector<std::string> &vec) {
        //open file
        std::ifstream istream(locate_resource(path));
        //read line by line until vector is full
        size_t pos = 0;
        for (std::string line; getline(istream, line) && pos < vec.size();) {
            if (line.size() < key_props<std::string>::maxKeySize() - 8) {//ignore overlong strings
                vec[pos++] = line;
            }
        }
    }
};
