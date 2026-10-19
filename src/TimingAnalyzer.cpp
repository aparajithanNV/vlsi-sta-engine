//
// Created by aparajithannv on 08/10/26.
//

#include "sta/TimingAnalyzer.h"

#include <queue>
#include <cmath>
#include <limits>
#include <cstdint>


auto analyze(TimingGraph& timing_graph, STAMode sta_mode, STAState& sta_state, ArcUpdates& arc_data) -> bool
{
    if (sta_mode == STAMode::FullPassEarly)
    {
        calculate_slack_full_pass_early(timing_graph, sta_state);
    }
    else if (sta_mode == STAMode::FullPassLate)
    {
        calculate_slack_full_pass_late(timing_graph, sta_state);
    }
    else if (sta_mode == STAMode::IncrementalEarly)
    {
        calculate_incremental_AAT(timing_graph, sta_state, arc_data, Analysis::Early);
        calculate_incremental_RAT(timing_graph, sta_state, arc_data, Analysis::Early);
        // calculate_slack_incremental_early(timing_graph, sta_state, arc_data);
    }
    else if (sta_mode == STAMode::IncrementalLate)
    {
        calculate_incremental_AAT(timing_graph, sta_state, arc_data, Analysis::Late);
        calculate_incremental_RAT(timing_graph, sta_state, arc_data, Analysis::Late);
        // calculate_slack_incremental_late(timing_graph, sta_state, arc_data);
    }
    else
    {
        return true;
    }
    return false;
}

auto calculate_slack_full_pass_early(TimingGraph& timing_graph, STAState& sta_state) -> void
{
    calculate_AAT(timing_graph, sta_state, Analysis::Early);
    calculate_RAT(timing_graph, sta_state, Analysis::Early);
    calculate_slack(timing_graph, sta_state, Analysis::Early);
}

auto calculate_slack_full_pass_late(TimingGraph& timing_graph, STAState& sta_state) -> void
{
    // auto num_levels = timing_graph.get_number_of_topological_levels();

    // Actual Arrival Time calculation
    // for (auto i = 0u; i < num_levels; i++)
    // {
    //     auto pins_in_level = timing_graph.find_pins_in_level(i);
    //     /*
    //      * For each pin, find predecessor arcs
    //      * For each arc, get the source pin, the AAT of the pin, the arc delay
    //      * Find the maximum of the pin AAT + arc delays
    //      * Assign the maximum to the current pin
    //      * If current pin has no predecessor like the virtual source pin, set AAT to 0
    //      */
    //     for (auto pin : pins_in_level)
    //     {
    //         auto fanin_arcs = timing_graph.get_fanin_arcs(pin);
    //         float pin_AAT;
    //         if (fanin_arcs.empty())
    //         {
    //             pin_AAT = 0;
    //         }
    //         for (auto arc : fanin_arcs)
    //         {
    //             auto source_pin = timing_graph.get_source_pin(arc);
    //             auto AAT_src = sta_state.actual_arrival_times[source_pin];
    //             auto arc_delay = timing_graph.get_late_delay(arc);
    //             auto AAT = AAT_src + arc_delay;
    //             pin_AAT = std::max(pin_AAT, AAT);
    //         }
    //         sta_state.actual_arrival_times[pin] = pin_AAT;
    //     }
    // }

    calculate_AAT(timing_graph, sta_state, Analysis::Late);

    // Calculate RAT
    // for (auto i = num_levels - 1u; i < num_levels; i--)
    // {
    //     auto pins_in_level = timing_graph.find_pins_in_level(i);
    //     for (auto pin : pins_in_level)
    //     {
    //         auto fanout_arcs = timing_graph.get_fanout_arcs(pin);
    //         float pin_RAT;
    //         if (fanout_arcs.empty())
    //         {
    //             pin_RAT = sta_state.circuit_RAT;
    //         }
    //         for (auto arc : fanout_arcs)
    //         {
    //             auto destination_pin = timing_graph.get_destination_pin(arc);
    //             auto RAT_dst = sta_state.required_arrival_times[destination_pin];
    //             auto arc_delay = timing_graph.get_late_delay(arc);
    //             auto RAT = RAT_dst - arc_delay;
    //             pin_RAT = std::min(pin_RAT, RAT);
    //         }
    //         sta_state.required_arrival_times[pin] = pin_RAT;
    //     }
    // }

    calculate_RAT(timing_graph, sta_state, Analysis::Late);

    // Calculate late slack
    // auto num_pins = timing_graph.get_number_of_pins();
    // for (PinID pin_id=0; pin_id < num_pins; pin_id++)
    // {
    //     auto pin_RAT = sta_state.required_arrival_times[pin_id];
    //     auto pin_AAT = sta_state.actual_arrival_times[pin_id];
    //     auto pin_slack = pin_RAT - pin_AAT;
    //     sta_state.slack[pin_id] = pin_slack;
    // }

    calculate_slack(timing_graph, sta_state, Analysis::Late);
}

auto calculate_AAT(TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> void
{
    auto num_levels = timing_graph.get_number_of_topological_levels();
    float pin_AAT;
    for (auto i = 0u; i < num_levels; i++)
    {
        auto pins_in_level = timing_graph.find_pins_in_level(i);
        /*
         * For each pin, find predecessor arcs
         * For each arc, get the source pin, the AAT of the pin, the arc delay
         * Find the maximum of the pin AAT + arc delays
         * Assign the maximum to the current pin
         * If current pin has no predecessor like the virtual source pin, set AAT to 0
         */
        for (auto pin : pins_in_level)
        {
            auto fanin_arcs = timing_graph.get_fanin_arcs(pin);

            if (mode == Analysis::Late)
            {
                pin_AAT = NEG_INF;
            }
            else if (mode == Analysis::Early)
            {
                pin_AAT = POS_INF;
            }

            if (fanin_arcs.empty())
            {
                pin_AAT = 0;
            }
            for (auto arc : fanin_arcs)
            {
                auto source_pin = timing_graph.get_source_pin(arc);
                auto AAT_src = sta_state.actual_arrival_times[source_pin];

                if (mode == Analysis::Late)
                {
                    auto arc_delay = timing_graph.get_late_delay(arc);
                    auto AAT = AAT_src + arc_delay;
                    pin_AAT = std::max(pin_AAT, AAT);
                }
                else if (mode == Analysis::Early)
                {
                    auto arc_delay = timing_graph.get_early_delay(arc);
                    auto AAT = AAT_src + arc_delay;
                    pin_AAT = std::min(pin_AAT, AAT);
                }
            }
            sta_state.actual_arrival_times[pin] = pin_AAT;
        }
    }

}

auto calculate_RAT(TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> void
{
    auto num_levels = timing_graph.get_number_of_topological_levels();
    float pin_RAT;
    // Calculate RAT
    for (auto i = num_levels - 1u; i < num_levels; i--)
    {
        auto pins_in_level = timing_graph.find_pins_in_level(i);
        for (auto pin : pins_in_level)
        {
            auto fanout_arcs = timing_graph.get_fanout_arcs(pin);

            if (mode == Analysis::Late)
            {
                pin_RAT = POS_INF;
            }
            else if (mode == Analysis::Early)
            {
                pin_RAT = NEG_INF;
            }
            if (fanout_arcs.empty())
            {
                pin_RAT = sta_state.circuit_RAT;
            }
            for (auto arc : fanout_arcs)
            {
                auto destination_pin = timing_graph.get_destination_pin(arc);
                auto RAT_dst = sta_state.required_arrival_times[destination_pin];

                if (mode == Analysis::Late)
                {
                    auto arc_delay = timing_graph.get_late_delay(arc);
                    auto RAT = RAT_dst - arc_delay;
                    pin_RAT = std::min(pin_RAT, RAT);
                }
                else if (mode == Analysis::Early)
                {
                    auto arc_delay = timing_graph.get_early_delay(arc);
                    auto RAT = RAT_dst - arc_delay;
                    pin_RAT = std::max(pin_RAT, RAT);
                }
            }
            sta_state.required_arrival_times[pin] = pin_RAT;
        }
    }
}

auto calculate_slack(TimingGraph& timing_graph, STAState& sta_state, Analysis mode) -> void
{
    auto num_pins = timing_graph.get_number_of_pins();
    sta_state.wns = POS_INF;
    for (PinID pin_id=0; pin_id < num_pins; pin_id++)
    {
        auto pin_RAT = sta_state.required_arrival_times[pin_id];
        auto pin_AAT = sta_state.actual_arrival_times[pin_id];

        if (mode == Analysis::Late)
        {
            auto pin_slack = pin_RAT - pin_AAT;
            sta_state.slack[pin_id] = pin_slack;
            sta_state.wns = std::min(sta_state.wns, pin_slack);
        }
        else if (mode == Analysis::Early)
        {
            auto pin_slack = pin_AAT - pin_RAT;
            sta_state.slack[pin_id] = pin_slack;
            sta_state.wns = std::min(sta_state.wns, pin_slack);
        }
    }
}

/*
 * Incremental AAT
 * update the timing graph using the arc data given
 * get the dst pins of each arc, add them to a queue
 * for each pin, find predecessor arcs, find AAT for the pin, if change is over threshold add successor pins to queue
 */
struct CompareLevels
{
    // TODO: Convert to span
    const std::vector<uint32_t> levels_map;

    CompareLevels(const std::vector<uint32_t>& levels_map) : levels_map(levels_map) {}
    bool operator()(const uint32_t a, const uint32_t b) const
    {
        return levels_map[a] > levels_map[b];
    }
};

auto calculate_incremental_AAT(TimingGraph& timing_graph, STAState& sta_state, ArcUpdates& arc_data, Analysis mode) -> void
{
    CompareLevels comparison(timing_graph.get_pin_to_level_map());
    std::priority_queue<PinID, std::vector<PinID>, CompareLevels> dirty_pins(comparison);
    float pin_AAT;
    for (auto i=0u; i < arc_data.arcs.size(); i++)
    {
        auto late_delay = arc_data.late_delays[i];
        auto early_delay = arc_data.early_delays[i];
        auto arc = arc_data.arcs[i];
        timing_graph.set_arc_delay(arc, early_delay, late_delay);
        // auto src = timing_graph.get_source_pin(arc);
        auto dst = timing_graph.get_destination_pin(arc);
        // dirty_pins.push(src);
        dirty_pins.push(dst);
    }

    while (!dirty_pins.empty())
    {
        auto pin = dirty_pins.top();
        dirty_pins.pop();
        auto fanout_arcs = timing_graph.get_fanout_arcs(pin);
        auto fanin_arcs = timing_graph.get_fanin_arcs(pin);

        if (mode == Analysis::Late)
        {
            pin_AAT = NEG_INF;
        }
        else if (mode == Analysis::Early)
        {
            pin_AAT = POS_INF;
        }
        if (fanin_arcs.empty())
        {
            pin_AAT = 0;
        }
        for (auto arc : fanin_arcs)
        {
            auto src_pin = timing_graph.get_source_pin(arc);
            auto AAT_src = sta_state.actual_arrival_times[src_pin];
            if (mode == Analysis::Late)
            {
                auto arc_delay = timing_graph.get_late_delay(arc);
                auto AAT = AAT_src + arc_delay;
                pin_AAT = std::max(pin_AAT, AAT);
            }
            if (mode == Analysis::Early)
            {
                auto arc_delay = timing_graph.get_early_delay(arc);
                auto AAT = AAT_src + arc_delay;
                pin_AAT = std::min(pin_AAT, AAT);
            }
        }
        auto original_pin_AAT = sta_state.actual_arrival_times[pin];

        if (std::abs(original_pin_AAT - pin_AAT) > 1e-5f)
        {
            sta_state.actual_arrival_times[pin] = pin_AAT;
            if (mode == Analysis::Late)
            {
                auto pin_RAT = sta_state.required_arrival_times[pin];
                auto late_slack = pin_RAT - pin_AAT;
                sta_state.slack[pin] = late_slack;
            }
            else if (mode == Analysis::Early)
            {
                auto pin_RAT = sta_state.required_arrival_times[pin];
                auto early_slack = pin_AAT - pin_RAT;
                sta_state.slack[pin] = early_slack;
            }
            for (auto arc : fanout_arcs)
            {
                auto dest_pin = timing_graph.get_destination_pin(arc);
                dirty_pins.push(dest_pin);
            }
        }
    }
}


/*
 * Incremental RAT
 * update the timing graph using the arc data given
 * get the src pins of each arc, add them to a queue
 * for each pin, find successor arcs, findt RAT for the pin, if change is over threshold add predecessor pins to queue
 */

auto calculate_incremental_RAT(TimingGraph& timing_graph, STAState& sta_state, ArcUpdates& arc_data, Analysis mode) -> void
{
    CompareLevels comparison(timing_graph.get_pin_to_level_map());
    std::priority_queue<PinID, std::vector<PinID>, CompareLevels> dirty_pins(comparison);
    float pin_RAT;

    for (auto i=0u; i < arc_data.arcs.size(); i++)
    {
        auto late_delay = arc_data.late_delays[i];
        auto early_delay = arc_data.early_delays[i];
        auto arc = arc_data.arcs[i];
        timing_graph.set_arc_delay(arc, early_delay, late_delay);
        // auto src = timing_graph.get_source_pin(arc);
        auto src = timing_graph.get_source_pin(arc);
        // dirty_pins.push(src);
        dirty_pins.push(src);
    }

    while (!dirty_pins.empty())
    {
        auto pin = dirty_pins.top();
        dirty_pins.pop();
        auto fanout_arcs = timing_graph.get_fanout_arcs(pin);
        auto fanin_arcs = timing_graph.get_fanin_arcs(pin);
        if (mode == Analysis::Late)
        {
            pin_RAT = POS_INF;
        }
        else if (mode == Analysis::Early)
        {
            pin_RAT = NEG_INF;
        }
        if (fanout_arcs.empty())
        {
            pin_RAT = sta_state.circuit_RAT;
        }
        for (auto arc : fanout_arcs)
        {
            auto dst_pin = timing_graph.get_destination_pin(arc);
            auto RAT_dst = sta_state.required_arrival_times[dst_pin];
            if (mode == Analysis::Late)
            {
                auto arc_delay = timing_graph.get_late_delay(arc);
                auto RAT = RAT_dst - arc_delay;
                pin_RAT = std::min(pin_RAT, RAT);
            }
            if (mode == Analysis::Early)
            {
                auto arc_delay = timing_graph.get_early_delay(arc);
                auto RAT = RAT_dst - arc_delay;
                pin_RAT = std::max(pin_RAT, RAT);
            }
        }
        auto original_pin_RAT = sta_state.required_arrival_times[pin];
        if (std::abs(original_pin_RAT - pin_RAT) > 1e-5f)
        {
            if (mode == Analysis::Late)
            {
                auto pin_AAT = sta_state.actual_arrival_times[pin];
                auto late_slack = pin_RAT - pin_AAT;
                sta_state.slack[pin] = late_slack;
            }
            else if (mode == Analysis::Early)
            {
                auto pin_AAT = sta_state.actual_arrival_times[pin];
                auto early_slack = pin_AAT - pin_RAT;
                sta_state.slack[pin] = early_slack;
            }
            sta_state.required_arrival_times[pin] = pin_RAT;
            for (auto arc : fanin_arcs)
            {
                auto src_pin = timing_graph.get_source_pin(arc);
                dirty_pins.push(src_pin);
            }
        }
    }
}
