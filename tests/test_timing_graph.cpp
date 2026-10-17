//
// Created by aparajithannv on 08/10/26.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <span>
#include "sta/TimingGraph.h"

using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

/*
 * TODO: Write unit tests for finding the topological level of a given pin
 */

namespace
{
    class TimingGraphTest : public ::testing::Test
    {
    protected:
        TimingGraph timing_graph;

        void SetUp() override
        {
            timing_graph.add_pin(PinType::PI);       // a=0
            timing_graph.add_pin(PinType::PI);       // b=1
            timing_graph.add_pin(PinType::PI);       // c=2
            timing_graph.add_pin(PinType::INTERNAL); // x0=3
            timing_graph.add_pin(PinType::INTERNAL); // x1=4
            timing_graph.add_pin(PinType::INTERNAL); // y0=5
            timing_graph.add_pin(PinType::INTERNAL); // y1=6
            timing_graph.add_pin(PinType::INTERNAL); // y2=7
            timing_graph.add_pin(PinType::INTERNAL); // z0=8
            timing_graph.add_pin(PinType::INTERNAL); // z1=9
            timing_graph.add_pin(PinType::INTERNAL); // z2=10
            timing_graph.add_pin(PinType::INTERNAL); // w0=11
            timing_graph.add_pin(PinType::INTERNAL); // w1=12
            timing_graph.add_pin(PinType::INTERNAL); // w2=13
            timing_graph.add_pin(PinType::PO);       // f=14
            timing_graph.add_pin(PinType::INTERNAL); // s=15

            PinID a{0};
            PinID b{1};
            PinID c{2};
            PinID x0{3};
            PinID x1{4};
            PinID y0{5};
            PinID y1{6};
            PinID y2{7};
            PinID z0{8};
            PinID z1{9};
            PinID z2{10};
            PinID w0{11};
            PinID w1{12};
            PinID w2{13};
            PinID f{14};
            PinID s{15};

            timing_graph.add_arc(s, a, ArcType::WIRE, 0.0, 0.0);
            timing_graph.add_arc(s, b, ArcType::WIRE, 0.0, 0.0);
            timing_graph.add_arc(s, c, ArcType::WIRE, 0.0, 0.6);
            timing_graph.add_arc(a, y1, ArcType::WIRE, 0.0, 0.15);
            timing_graph.add_arc(b, x1, ArcType::WIRE, 0.0, 0.1);
            timing_graph.add_arc(c, z2, ArcType::WIRE, 0.0, 0.1);
            timing_graph.add_arc(y1, y0, ArcType::GATE, 2.0, 2.0);
            timing_graph.add_arc(x1, x0, ArcType::GATE, 1.0, 1.0);
            timing_graph.add_arc(z2, z0, ArcType::GATE, 2.0, 2.0);
            timing_graph.add_arc(x0, y2, ArcType::WIRE, 0.0, 0.1);
            timing_graph.add_arc(x0, z1, ArcType::WIRE, 0.0, 0.3);
            timing_graph.add_arc(y2, y0, ArcType::GATE, 2.0, 2.0);
            timing_graph.add_arc(z1, z0, ArcType::GATE, 2.0, 2.0);
            timing_graph.add_arc(y0, w1, ArcType::WIRE, 0.0, 0.2);
            timing_graph.add_arc(z0, w2, ArcType::WIRE, 0.0, 0.25);
            timing_graph.add_arc(w1, w0, ArcType::GATE, 2.0, 2.0);
            timing_graph.add_arc(w2, w0, ArcType::GATE, 2.0, 2.0);
            timing_graph.add_arc(w0, f, ArcType::WIRE, 0.0, 0.2);

            timing_graph.populate_pin_neighbors();
        }
    };

    TEST_F(TimingGraphTest, GraphSummary)
    {
        auto num_pins = timing_graph.get_number_of_pins();
        auto num_arcs = timing_graph.get_number_of_arcs();

        ASSERT_EQ(num_pins, 16);
        ASSERT_EQ(num_arcs, 18);
    }

    TEST_F(TimingGraphTest, Neighbors)
    {
        auto successor_arcs_of_x0 = timing_graph.get_fanout_arcs(3);
        auto predecessor_arcs_of_w0 = timing_graph.get_fanin_arcs(11);

        std::vector<PinID> successor_pins_of_x0, predecessor_pins_of_w0;
        for (auto arc : successor_arcs_of_x0)
        {
            auto pin = timing_graph.get_destination_pin(arc);
            successor_pins_of_x0.emplace_back(pin);
        }

        for (auto arc : predecessor_arcs_of_w0)
        {
            auto pin = timing_graph.get_source_pin(arc);
            predecessor_pins_of_w0.emplace_back(pin);
        }
        // successor pins of x0 -> y2, z1
        // predecessor pins of w0 -> w1, w2
        ASSERT_THAT(successor_pins_of_x0, UnorderedElementsAre(7, 9)); // y2=7, z1=9
        ASSERT_THAT(predecessor_pins_of_w0, UnorderedElementsAre(12, 13)); // w1=12, w2=13

    }

    TEST_F(TimingGraphTest, TopologicalLevels)
    {
        auto done = timing_graph.find_topological_levels();
        auto num_levels = timing_graph.get_number_of_topological_levels();
        auto pins_in_level_2 = timing_graph.find_pins_in_level(2);

        ASSERT_EQ(num_levels, 9); // Accounting for the single virtual source node 's'
        // pins in level 2 starting from 0 -> 6, 4, 10
        ASSERT_THAT(pins_in_level_2, UnorderedElementsAre(6, 4, 10));
    }
}
