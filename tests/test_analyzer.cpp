//
// Created by aparajithannv on 08/10/26.
//

#include <gtest/gtest.h>
#include <vector>
#include "sta/TimingAnalyzer.h"
#include "sta/TimingGraph.h"

namespace
{
    class TimingAnalyzerTest : public ::testing::Test
    {
    protected:
        TimingGraph timing_graph;
        STAState sta_state;
        ArcUpdates arc_data;
        PinID pin_id;

        void SetUp() override
        {
            pin_id = timing_graph.add_pin(PinType::PI);       // a=0
            timing_graph.update_input_pins(pin_id);
            pin_id = timing_graph.add_pin(PinType::PI);       // b=1
            timing_graph.update_input_pins(pin_id);
            pin_id = timing_graph.add_pin(PinType::PI);       // c=2
            timing_graph.update_input_pins(pin_id);

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
            pin_id = timing_graph.add_pin(PinType::PO);       // f=14
            timing_graph.update_output_pins(pin_id);
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
            auto success = timing_graph.find_topological_levels();

            sta_state.circuit_RAT = 5.5;
            auto num_pins = timing_graph.get_number_of_pins();
            sta_state.actual_arrival_times = std::vector<float>(num_pins, 0.0);
            sta_state.required_arrival_times = std::vector<float>(num_pins, 0.0);
            sta_state.slack = std::vector<float>(num_pins, 0.0);

            arc_data.arcs = {7};
            arc_data.early_delays = {0.8};
            arc_data.late_delays = {1.3};
        }
    };

    TEST_F(TimingAnalyzerTest, FullPassLateMode)
    {
        const std::vector<float> pin_AAT{0, 0, 0.6, 1.1, 0.1, 3.2, 0.15, 1.2, 3.4, 1.4, 0.7, 5.65, 3.4, 3.65, 5.85, 0};
        const std::vector<float> pin_RAT{0.95, -0.35, 0.95, 0.75, -0.25, 3.1, 1.1, 1.1, 3.05, 1.05, 1.05, 5.3, 3.3, 3.3, 5.5, -0.35};
        const std::vector<float> pin_slack{0.95, -0.35, 0.35, -0.35, -0.35, -0.1, 0.95, -0.1, -0.35, -0.35, 0.35, -0.35, -0.1, -0.35, -0.35, -0.35};
        calculate_slack_full_pass_late(timing_graph, sta_state);
        auto num_pins = timing_graph.get_number_of_pins();
        for (PinID i = 0; i < num_pins; i++)
        {
            ASSERT_NEAR(sta_state.actual_arrival_times[i], pin_AAT[i], 1e-5);
            ASSERT_NEAR(sta_state.required_arrival_times[i], pin_RAT[i], 1e-5);
            ASSERT_NEAR(sta_state.slack[i], pin_slack[i], 1e-5);
        }
        ASSERT_NEAR(sta_state.wns, -0.35, 1e-5);
    }

    TEST_F(TimingAnalyzerTest, FullPassEarlyMode)
    {
        const std::vector<float> pin_AAT{0, 0, 0, 1, 0, 2, 0, 1, 2, 1, 0, 4, 2, 2, 4, 0};
        const std::vector<float> pin_RAT{1, 0, 1, 1, 0, 3, 1, 1, 3, 1, 1, 5, 3, 3, 5, 1};
        const std::vector<float> pin_slack{-1, 0, -1, 0, 0, -1, -1, 0, -1, 0, -1, -1, -1, -1, -1, -1};
        sta_state.circuit_RAT = 5.0;
        calculate_slack_full_pass_early(timing_graph, sta_state);
        auto num_pins = timing_graph.get_number_of_pins();
        for (PinID i = 0; i < num_pins; i++)
        {
            ASSERT_NEAR(sta_state.actual_arrival_times[i], pin_AAT[i], 1e-5);
            ASSERT_NEAR(sta_state.required_arrival_times[i], pin_RAT[i], 1e-5);
            ASSERT_NEAR(sta_state.slack[i], pin_slack[i], 1e-5);
        }
        ASSERT_NEAR(sta_state.wns, -1.0, 1e-5);
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

        STAState baseline_state=sta_state, incremental_state, golden_batch_state=sta_state;

        calculate_slack_full_pass_late(timing_graph, baseline_state);
        incremental_state = baseline_state;
        // calculate_slack_incremental_late(timing_graph, incremental_state, arc_data);
        calculate_incremental_AAT(timing_graph, incremental_state, arc_data, Analysis::Late);
        calculate_incremental_RAT(timing_graph, incremental_state, arc_data, Analysis::Late);
        timing_graph.set_arc_delay(arc_data.arcs[0], arc_data.early_delays[0], arc_data.late_delays[0]);
        calculate_slack_full_pass_late(timing_graph, golden_batch_state);
        auto num_pins = timing_graph.get_number_of_pins();
        for (PinID i = 0; i < num_pins; i++)
        {
            ASSERT_NEAR(incremental_state.actual_arrival_times[i], golden_batch_state.actual_arrival_times[i], 1e-5) << "Mismatch at pin "<< i;
            ASSERT_NEAR(incremental_state.required_arrival_times[i], golden_batch_state.required_arrival_times[i], 1e-5) << "Mismatch at pin "<< i;
            ASSERT_NEAR(incremental_state.slack[i], golden_batch_state.slack[i], 1e-5) << "Mismatch at pin "<< i;
        }
    }

    TEST_F(TimingAnalyzerTest, IncrementalEarlyMode)
    {
        // ArcID id{6};
        // timing_graph.set_arc_delay(id, early_delay, late_delay);
        // calculate_slack(timing_graph, Mode::FullPassEarly);
        // timing_graph.set_arc_delay(id, early_delay, late_delay);
        // auto early_incremental_slack = calculate_slack(timing_graph, Mode::IncrementalEarly)
        STAState baseline_state=sta_state, incremental_state, golden_batch_state=sta_state;

        calculate_slack_full_pass_early(timing_graph, baseline_state);
        incremental_state = baseline_state;
        // calculate_slack_incremental_early(timing_graph, incremental_state, arc_data);
        calculate_incremental_AAT(timing_graph, incremental_state, arc_data, Analysis::Early);
        calculate_incremental_RAT(timing_graph, incremental_state, arc_data, Analysis::Early);
        timing_graph.set_arc_delay(arc_data.arcs[0], arc_data.early_delays[0], arc_data.late_delays[0]);
        calculate_slack_full_pass_early(timing_graph, golden_batch_state);
        auto num_pins = timing_graph.get_number_of_pins();
        for (PinID i = 0; i < num_pins; i++)
        {
            ASSERT_NEAR(incremental_state.actual_arrival_times[i], golden_batch_state.actual_arrival_times[i], 1e-5);
            ASSERT_NEAR(incremental_state.required_arrival_times[i], golden_batch_state.required_arrival_times[i], 1e-5);
            ASSERT_NEAR(incremental_state.slack[i], golden_batch_state.slack[i], 1e-5);
        }
    }

    TEST_F(TimingAnalyzerTest, FullPassLateParallel)
    {
        STAState golden_state=sta_state, parallel_state=sta_state;
        tf::Executor executor;
        calculate_slack_full_pass_late(timing_graph, golden_state);
        calculate_slack_full_pass_parallel(timing_graph, parallel_state, Analysis::Late, executor);
        auto num_pins = timing_graph.get_number_of_pins();
        for (PinID i = 0; i < num_pins; i++)
        {
            ASSERT_NEAR(golden_state.actual_arrival_times[i], parallel_state.actual_arrival_times[i], 1e-5);
            ASSERT_NEAR(golden_state.required_arrival_times[i], parallel_state.required_arrival_times[i], 1e-5);
            ASSERT_NEAR(golden_state.slack[i], parallel_state.slack[i], 1e-5);
        }
    }

    TEST_F(TimingAnalyzerTest, CriticalPath)
    {
        calculate_slack_full_pass_late(timing_graph, sta_state);
        find_critical_path(timing_graph, sta_state, Analysis::Late);
        auto critical_path = sta_state.critical_path;
        PinID s=15, b=1, x1=4, x0=3, z1=9, z0=8, w2=13, w0=11, f=14;
        std::vector<PinID> golden_critical_path{s, b, x1, x0, z1, z0, w2, w0, f};
        ASSERT_EQ(critical_path, golden_critical_path);
        report_timing(timing_graph, sta_state, Analysis::Late);
    }
}