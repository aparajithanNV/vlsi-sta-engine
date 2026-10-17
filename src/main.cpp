#include <iostream>
#include <fstream>

#include "sta/BenchParser.h"
// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.

int main()
{
    std::cout << "Static Timing Analyzer\n";
    std::fstream file;
    file.open("../../iscas85/c17.bench");
    int success{0};
    if (file.is_open())
    {
        BenchParser parser;
        TimingGraph timingGraph;
        success = parser.parse_bench(file, timingGraph);
        auto num_pins = timingGraph.get_number_of_pins();
        std::cout << "Num pins " << num_pins << "\n";
        std::cout.flush();
    }
    else
    {
        return 1;
    }
    return success;
}