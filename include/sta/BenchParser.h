//
// Created by aparajithannv on 08/10/26.
//


#ifndef STA_ENGINE_BENCHPARSER_H
#define STA_ENGINE_BENCHPARSER_H

#include <sta/TimingGraph.h>
#include <istream>

class BenchParser
{
    public:
    BenchParser() = default;
    /* istream is not copyable, must take it as input parameter.
     * Parser should be stateless, should take in the timing graph too as input parameter
    */
    auto parse_bench(std::istream &is, TimingGraph &timing_graph) -> bool;
};

#endif //STA_ENGINE_BENCHPARSER_H
