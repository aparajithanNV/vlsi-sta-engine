//
// Created by aparajithannv on 08/10/26.
//

#include "sta/TimingGraph.h"

#include <queue>
#include <unordered_map>

auto TimingGraph::add_pin(PinType pin_type) -> PinID
{
    pin_types.emplace_back(pin_type);
    auto new_pin_id = static_cast<PinID>(pin_types.size() - 1);
    return new_pin_id;
}

auto TimingGraph::get_number_of_pins() const noexcept-> uint32_t
{
    return static_cast<uint32_t>(pin_types.size());
}

auto TimingGraph::add_arc(PinID pin_1, PinID pin_2, ArcType arc_type, float early_delay, float late_delay) -> ArcID
{
    arc_types.emplace_back(arc_type);
    arc_source_pins.emplace_back(pin_1);
    arc_destination_pins.emplace_back(pin_2);
    arc_early_delays.emplace_back(early_delay);
    arc_late_delays.emplace_back(late_delay);
    auto new_arc_id = static_cast<uint32_t>(arc_types.size() - 1);
    return new_arc_id;
}

auto TimingGraph::set_arc_delay(ArcID arc_id, float early_delay, float late_delay) -> void
{
    arc_early_delays[arc_id] = early_delay;
    arc_late_delays[arc_id] = late_delay;
}

auto TimingGraph::get_arc_type(ArcID arc_id) const noexcept -> ArcType
{
    return arc_types[arc_id];
}

auto TimingGraph::get_number_of_arcs() const noexcept -> uint32_t
{
    return static_cast<uint32_t>(arc_types.size());
}

auto TimingGraph::get_destination_pin(ArcID arc_id) const -> PinID
{
    return arc_destination_pins[arc_id];
}

auto TimingGraph::get_source_pin(ArcID arc_id) const -> PinID
{
    return arc_source_pins[arc_id];
}

auto TimingGraph::get_early_delay(ArcID arc_id) const -> float
{
    return arc_early_delays[arc_id];
}

auto TimingGraph::get_late_delay(ArcID arc_id) const -> float
{
    return arc_late_delays[arc_id];
}

auto TimingGraph::populate_pin_neighbors() -> void
{
    auto num_pins = get_number_of_pins();
    std::unordered_map<PinID, std::vector<PinID>> pin_to_fanout_arcs(num_pins);
    std::unordered_map<PinID, std::vector<PinID>> pin_to_fanin_arcs(num_pins);
    auto num_arcs = get_number_of_arcs();
    for (ArcID arc_id=0; arc_id < num_arcs; arc_id++)
    {
        auto destination_pin = get_destination_pin(arc_id);
        auto source_pin = get_source_pin(arc_id);
        pin_to_fanin_arcs[destination_pin].emplace_back(arc_id);
        pin_to_fanout_arcs[source_pin].emplace_back(arc_id);
    }
    uint32_t fanout_arcs_offset = 0;
    for (PinID pin_id=0; pin_id < num_pins; pin_id++)
    {
        auto fanout_arcs = pin_to_fanout_arcs[pin_id];
        pin_successor_arcs.reserve(pin_successor_arcs.size() + fanout_arcs.size());
        pin_successor_arcs.insert(pin_successor_arcs.end(),fanout_arcs.begin(),fanout_arcs.end());
        pin_successor_arc_offsets.emplace_back(fanout_arcs_offset);
        fanout_arcs_offset += fanout_arcs.size();
    }
    pin_successor_arc_offsets.emplace_back(fanout_arcs_offset);
    uint32_t fanin_arcs_offset = 0;
    for (PinID pin_id=0; pin_id < num_pins; pin_id++)
    {
        auto fanin_arcs = pin_to_fanin_arcs[pin_id];
        pin_predecessor_arcs.reserve(pin_predecessor_arcs.size() + fanin_arcs.size());
        pin_predecessor_arcs.insert(pin_predecessor_arcs.end(), fanin_arcs.begin(),fanin_arcs.end());
        pin_predecessor_arc_offsets.emplace_back(fanin_arcs_offset);
        fanin_arcs_offset += fanin_arcs.size();
    }
    pin_predecessor_arc_offsets.emplace_back(fanin_arcs_offset);
}

auto TimingGraph::get_fanin_arcs(PinID pin) const -> std::span<const ArcID>
{
    auto start = pin_predecessor_arc_offsets[pin];
    auto end = pin_predecessor_arc_offsets[pin + 1];
    auto fanin_arcs = std::span(pin_predecessor_arcs.data() + start, end - start);
    return fanin_arcs;
}

auto TimingGraph::get_fanout_arcs(PinID pin) const -> std::span<const ArcID>
{
    auto start = pin_successor_arc_offsets[pin];
    auto end = pin_successor_arc_offsets[pin + 1];
    auto fanout_arcs = std::span(pin_successor_arcs.data() + start, end - start);
    return fanout_arcs;
}

auto TimingGraph::update_input_pins(PinID pin_id) -> void
{
    input_pins.emplace_back(pin_id);
}

auto TimingGraph::get_input_pins() -> std::span<PinID>
{
    return std::span<PinID>(input_pins.data(), input_pins.size());
}

auto TimingGraph::update_output_pins(PinID pin_id) -> void
{
    output_pins.emplace_back(pin_id);
}

auto TimingGraph::get_output_pins() -> std::span<PinID>
{
    return std::span<PinID>(output_pins.data(), output_pins.size());
}

auto TimingGraph::find_topological_levels() -> bool
{
    pin_indegrees.clear();
    topological_levels.clear();
    topological_levels_offsets.clear();
    pin_to_levels_map.clear();
    std::queue<uint32_t> start_pins;
    for (PinID pin_id=0; pin_id < get_number_of_pins(); pin_id++)
    {
        auto fanin_arcs = get_fanin_arcs(pin_id);
        pin_indegrees.emplace_back(static_cast<uint8_t>(fanin_arcs.size()));
        if (fanin_arcs.empty())
        {
            start_pins.push(pin_id);
        }
    }

    uint32_t topological_level_offset = 0;
    topological_levels_offsets.emplace_back(topological_level_offset);
    topological_level_offset += start_pins.size();
    std::vector<uint32_t> pin_topological_levels(get_number_of_pins(), UINT32_MAX);
    std::vector<uint8_t> pin_indegrees_copy = pin_indegrees;
    while (!start_pins.empty())
    {
        auto pin = start_pins.front();
        topological_levels.emplace_back(pin);
        start_pins.pop();
        auto fanout_arcs = get_fanout_arcs(pin);
        for (auto arc_id : fanout_arcs)
        {
            auto destination_pin_id = get_destination_pin(arc_id);
            pin_indegrees_copy[destination_pin_id] -= 1;
            if (pin_indegrees_copy[destination_pin_id] == 0)
            {
                start_pins.push(destination_pin_id);
            }
        }
    }
    uint32_t max_topological_levels = 0;
    for (auto pin_id : topological_levels)
    {
        auto fanin_arcs = get_fanin_arcs(pin_id);
        std::vector<PinID> source_pins;
        for (auto arc_id : fanin_arcs)
        {
            auto source_pin = get_source_pin(arc_id);
            source_pins.emplace_back(source_pin);
        }
        if (source_pins.empty())
        {
            pin_topological_levels[pin_id] = 0;
            max_topological_levels = std::max(max_topological_levels, static_cast<uint32_t>(0));
        }
        else
        {
            uint32_t max_level = 0;
            for (auto src_pin : source_pins)
            {
                auto src_pin_level = pin_topological_levels[src_pin];
                max_level = std::max(max_level, src_pin_level);
            }
            auto pin_level = max_level + 1;
            max_topological_levels = std::max(max_topological_levels, pin_level);
            pin_topological_levels[pin_id] = pin_level;
        }
    }

    // max_topological_levels tracks indices from 0. Must add 1.
    std::vector<std::vector<PinID>> topological_levels_to_pins(max_topological_levels + 1);
    for (PinID pin_id=0; pin_id < get_number_of_pins(); pin_id++)
    {
        auto pin_level = pin_topological_levels[pin_id];
        topological_levels_to_pins[pin_level].emplace_back(pin_id);
    }
    size_t level_offset = 0;
    for (const auto& pins_in_level : topological_levels_to_pins)
    {
        auto level_size = pins_in_level.size();
        level_offset += level_size;
        topological_levels_offsets.emplace_back(level_offset);
    }

    pin_to_levels_map = pin_topological_levels;
    return true;
}

auto TimingGraph::find_pins_in_level(uint32_t level_num) const -> std::span<const PinID>
{
    auto start = topological_levels_offsets[level_num];
    auto end = topological_levels_offsets[level_num + 1];
    auto pins_in_level = std::span<const PinID>(topological_levels.data() + start, end - start);
    return pins_in_level;
}

auto TimingGraph::get_number_of_topological_levels() const noexcept -> uint32_t
{
    return static_cast<uint32_t>(topological_levels_offsets.size() - 1);
}

auto TimingGraph::get_pin_to_level_map() const noexcept -> std::span<const uint32_t>
{
    auto start = 0u;
    auto end = get_number_of_pins();
    auto pin_to_levels = std::span<const uint32_t>(pin_to_levels_map.data() + start, end - start);
    return pin_to_levels;
}
