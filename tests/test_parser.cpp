//
// Created by aparajithannv on 15/10/26.
//

#include <gtest/gtest.h>
#include "sta/BenchParser.h"
#include "sta/TimingGraph.h"

TEST(ParserTest, Empty)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(

)";
    std::stringstream circuit_stream(circuit);
    auto result = parser.parse_bench(circuit_stream, timing_graph);
    auto num_pins = timing_graph.get_number_of_pins();
    auto num_arcs = timing_graph.get_number_of_arcs();
    ASSERT_EQ(num_pins, 0);
    ASSERT_EQ(num_arcs, 0);
}

TEST(ParserTest, InverterGate)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(
INPUT(1)

OUTPUT(2)

2 = INV(1)
)";
    std::stringstream circuit_stream(circuit);
    auto parse_success = parser.parse_bench(circuit_stream, timing_graph);
    // Empty circuit, Return 0 for success
    auto num_pins = timing_graph.get_number_of_pins();
    auto num_arcs = timing_graph.get_number_of_arcs();
    ASSERT_EQ(parse_success, 0);
    ASSERT_EQ(num_pins, 4);
    ASSERT_EQ(num_arcs, 3);
}

TEST(ParserTest, AndGate)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(
INPUT(1)
INPUT(2)

OUTPUT(3)

3 = AND(1, 2)
)";
    std::stringstream circuit_stream(circuit);
    auto parse_success = parser.parse_bench(circuit_stream, timing_graph);
    auto num_pins = timing_graph.get_number_of_pins();
    auto num_arcs = timing_graph.get_number_of_arcs();
    // Return 0 for success
    ASSERT_EQ(parse_success, 0);
    ASSERT_EQ(num_pins, 6);
    ASSERT_EQ(num_arcs, 5);
}

TEST(ParserTest, MultipleGate)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(
INPUT(1)
INPUT(2)
INPUT(3)

OUTPUT(7)

4 = INV(2)
5 = AND(1, 4)
6 = OR(4, 3)
7 = AND(5, 6)
)";
    std::stringstream circuit_stream(circuit);
    auto parse_success = parser.parse_bench(circuit_stream, timing_graph);
    auto num_pins = timing_graph.get_number_of_pins();
    auto num_arcs = timing_graph.get_number_of_arcs();
    ASSERT_EQ(parse_success, 0);
    ASSERT_EQ(num_pins, 15);
    ASSERT_EQ(num_arcs, 15);
}

TEST(ParserTest, UnknownGate)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(
INPUT(1)

OUTPUT(2)

2 = WHATAMI(1)
)";
    std::stringstream circuit_stream(circuit);
    auto parse_success = parser.parse_bench(circuit_stream, timing_graph);

    ASSERT_EQ(parse_success, 1);
}

TEST(ParserTest, InvalidSyntax)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(
INPUT(1)
INPUT(2)

OUTPUT(3);

3 = OR(1, 2)
)";
    std::stringstream circuit_stream(circuit);
    auto parse_success = parser.parse_bench(circuit_stream, timing_graph);

    ASSERT_EQ(parse_success, 0);
}

TEST(ParserTest, MissingOutput)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(
INPUT(1)
INPUT(2)

3 = NAND(1, 2)
)";
    std::stringstream circuit_stream(circuit);
    auto parse_success = parser.parse_bench(circuit_stream, timing_graph);

    // Output of gates farthest in the topological order are implicit outputs
    ASSERT_EQ(parse_success, 0);
}

TEST(ParserTest, MissingInternalNetTwo)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(
INPUT(1)
INPUT(2)

OUTPUT(3)

3 = XOR(1, )
)";
    std::stringstream circuit_stream(circuit);
    auto parse_success = parser.parse_bench(circuit_stream, timing_graph);

    ASSERT_EQ(parse_success, 1);
}

TEST(ParserTest, MissingInternalNetOne)
{
    BenchParser parser;
    TimingGraph timing_graph;
    std::string circuit = R"(
INPUT(1)
INPUT(2)

OUTPUT(3)

3 = XOR(, 2)
)";
    std::stringstream circuit_stream(circuit);
    auto parse_success = parser.parse_bench(circuit_stream, timing_graph);

    ASSERT_EQ(parse_success, 1);
}
