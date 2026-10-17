//
// Created by aparajithannv on 08/10/26.
//

#include <gtest/gtest.h>
#include "sta/TimingAnalyzer.h"
#include "sta/TimingGraph.h"

namespace
{
    class TimingAnalyzerTest : public ::testing::Test
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

            auto success = timing_graph.find_topological_levels();
        }
    };

    TEST_F(TimingAnalyzerTest, FullPassLateMode)
    {
        // slack = -0.35
        // auto late_mode_slack = calculate_slack(timing_graph, Mode::FullPassLate);
    }

    TEST_F(TimingAnalyzerTest, FullPassEarlyMode)
    {
        // slack = -1
        // auto early_mode_slack = calculate_slack(timing_graph, Mode::FullPassEarly);
    }

    TEST_F(TimingAnalyzerTest, IncrementalLateMode)
    {
        // Calculate Full pass slack with an arc delay set to a different value from original
        // Revert changes and calculate incremental slack and compare to original slack
        // ArcID id{5};
        // timing_graph.set_arc_delay(id, early_delay, late_delay);
        // calculate_slack(timing_graph, Mode::FullPassLate)
        // timing_graph.set_arc_delay(id, early_delay, late_delay);
        // auto incremental_slack = calculate_slack(timing_graph, Mode::IncrementalLate);
    }

    TEST_F(TimingAnalyzerTest, IncrementalEarlyMode)
    {
        // ArcID id{6};
        // timing_graph.set_arc_delay(id, early_delay, late_delay);
        // calculate_slack(timing_graph, Mode::FullPassEarly);
        // timing_graph.set_arc_delay(id, early_delay, late_delay);
        // auto early_incremental_slack = calculate_slack(timing_graph, Mode::IncrementalEarly)
    }
}