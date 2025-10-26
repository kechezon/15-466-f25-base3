#include "../ChimeBombData.hpp"
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <ranges>
#include <string_view>
#include <iomanip>
#include <iostream>
// #include <boost/algorithm/string.hpp> // can't get this to work :(

/*********************************************************************************************************************
 * Tutorials used:
 * File reading: https://cplusplus.com/doc/tutorial/files/
 * String stream: https://www.geeksforgeeks.org/cpp/stringstream-c-applications/
 * Boost split: https://www.geeksforgeeks.org/cpp/boostsplit-c-library/
 * Writing to binary: https://stackoverflow.com/questions/14089266/how-to-correctly-write-vector-to-binary-file-in-c
 ********************************************************************************************************************/
int main() {
    std::ifstream chartTextFile;
    chartTextFile.open("starter_soaring_chart.txt");
    std::ofstream chartOutput;
    chartOutput.open("../dist/starter_soaring.chrt", std::ios::binary);

    std::vector<ChimeBombData> chimeBombs = {};
    if (chartTextFile.is_open()) {
        std::string line;
        while (getline(chartTextFile, line)) {
            if (line.length() > 0 && (&line[0])[0] != '%') {
                std::stringstream linestream(line);
                std::vector<std::string> info = {};
                std::string piece;

                ChimeBombData data;

                while (getline(linestream, piece, ',')) {
                    info.emplace_back(piece);
                }

                // assert(info.size() == 6);

                size_t measure = 0;
                float beat = 0;
                float pan = 0;
                float pitch = 0;
                float volume = 0;
                std::string sample_name = "";

                // chart order: measure->beat->pan->pitch->volume->name
                std::stringstream measureStream(info[0]);
                measureStream >> data.measure;

                data.pan = std::stof(info[2]);
                data.volume = std::stof(info[4]);
                data.beat = std::stof(info[1]);
                data.pitch = std::stof(info[3]);
                
                for (size_t i = 0; i < info[5].length(); i++) {
                    data.sample_name[i] = info[5].at(i);
                }
                for (size_t i = info[5].length(); i < 100; i++) {
                    data.sample_name[i] = '\0';
                }
                std::cout << data.measure << "." << data.beat << ": " << data.sample_name << std::endl;
                // data.sample_name = info[5];

                chimeBombs.emplace_back(data);
            }
        }
        chartOutput.write(reinterpret_cast<const char*>(&chimeBombs[0]), chimeBombs.size() * sizeof(ChimeBombData));
        chartOutput.close();
    }
    chartTextFile.close();

    return 0;
}