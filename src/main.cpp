#include <iostream>
#include <fstream>
#include <string>
#include <charconv>
#include <string_view>

#include "sta/BenchParser.h"
#include "sta/TimingGraph.h"
#include "sta/TimingAnalyzer.h"

int main(int argc, char* argv[])
{
    // 1. Accept file from command line or default to c17.bench
    std::string file_path = "../../iscas85/c17.bench";

    if (argc > 1) {
        file_path = argv[1];
    }

    std::cout << "Loading circuit from: " << file_path << "\n";

    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file: " << file_path << "\n";
        return 1;
    }

    float target_rat = 10.0f;
    if (argc > 2)
    {
        std::string_view rat_string{argv[2]};
        auto [ptr, ec] = std::from_chars(rat_string.begin(), rat_string.end(), target_rat);
        if (ec != std::errc())
        {
            std::cerr << "Warning: Invalid RAT " << rat_string << " Using default: " << target_rat << "\n";
        }

    }
    // 2. Parse the netlist
    BenchParser parser;
    TimingGraph timing_graph;
    if (parser.parse_bench(file, timing_graph) != 0) {
        std::cerr << "Error: Failed to parse benchmark file.\n";
        return 1;
    }

    // 3. Build graph topology and compute topological levels
    timing_graph.populate_pin_neighbors();
    if (!timing_graph.find_topological_levels()) {
        std::cerr << "Error: Combinational cycle detected in the circuit!\n";
        return 1;
    }

    // 4. Initialize timing state
    auto num_pins = timing_graph.get_number_of_pins();
    STAState sta_state;
    sta_state.circuit_RAT = target_rat; // Target deadline
    sta_state.actual_arrival_times.assign(num_pins, 0.0f);
    sta_state.required_arrival_times.assign(num_pins, 0.0f);
    sta_state.slack.assign(num_pins, 0.0f);

    // 5. Run Static Timing Analysis (Late Mode / Setup)
    calculate_slack_full_pass_late(timing_graph, sta_state);

    // 6. Extract the Critical Path and print the signoff report
    find_critical_path(timing_graph, sta_state, Analysis::Late);
    report_timing(timing_graph, sta_state, Analysis::Late);

    return 0;
}