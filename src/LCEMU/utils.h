#pragma once
#include <string>
#include <filesystem>
#include <vector>


bool try_int(const std::string& str, int& tar, const int& fail);
bool try_ll(const std::string& str, long long int& tar, const long long int& fail);
bool try_uint(const std::string& str, unsigned int& tar, const unsigned int& fail);
bool try_ull(const std::string& str, unsigned long long int& tar, const unsigned long long int& fail);
bool try_dbl(const std::string& _str, double& tar,const double fail);
std::string pad_left(const std::string& v, const char pad, const int padlen);

std::vector<std::string> split_space(const std::string& str);

bool try_eval_relative(const std::filesystem::path& p, const std::filesystem::path& bs, std::filesystem::path& ret);
void put_with_color(const std::string& mes, const int& color);
void clear_screen(const char& fill = ' ');