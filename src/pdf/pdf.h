#pragma once
#include <string>
#include <iostream>
#include <vector>

int pdf2Merge(std::string file1, std::string file2, std::string output);
int pdfAddWatermark(std::string file, std::string output);
int pdfMerge(const std::vector<std::string>& files, const std::string& output);
