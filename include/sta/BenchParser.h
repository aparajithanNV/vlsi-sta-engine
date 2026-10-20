//
// Created by aparajithannv on 08/10/26.
//


#ifndef STA_ENGINE_BENCHPARSER_H
#define STA_ENGINE_BENCHPARSER_H

#include <sta/TimingGraph.h>
#include <istream>
#include <unordered_map>

class BenchParser
{
    public:
    BenchParser() = default;
    /* istream is not copyable, must take it as input parameter.
     * Parser should be stateless, should take in the timing graph too as input parameter
    */
    auto parse_bench(std::istream &is, TimingGraph &timing_graph) -> int;
    private:
    auto parse_input(std::string_view line, TimingGraph &timing_graph, std::unordered_map<std::string, PinID>& net_sources) -> int;
    auto parse_output(std::string_view line, TimingGraph &timing_graph, std::unordered_map<std::string, std::vector<PinID>>& net_destinations) -> int;
    auto parse_gate(std::string_view line, TimingGraph &timing_graph, std::unordered_map<std::string, PinID>& net_sources, std::unordered_map<std::string, std::vector<PinID>>& net_destinations) -> int;
};

#endif //STA_ENGINE_BENCHPARSER_H
