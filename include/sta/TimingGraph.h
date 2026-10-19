//
// Created by aparajithannv on 08/10/26.
//

#ifndef STA_ENGINE_TIMING_GRAPH_H
#define STA_ENGINE_TIMING_GRAPH_H

#include <span>
#include <vector>

#include <string>
#include <cstdint>
#include <limits>

/* PinID and ArcID may seem unecessary when the array index as integer is enough
 * PinID and ArcID are for clarity in passing arguments, strong typing to reduce bugs
*/
using PinID = uint32_t;
// Tells compiler to merge all variables into one. No duplication if header included in many places.
inline constexpr PinID INVALID_PIN = std::numeric_limits<uint32_t>::max();
using ArcID = uint32_t;
inline constexpr ArcID INVALID_ARC = std::numeric_limits<uint32_t>::max();

enum class PinType : uint8_t
{
    PI = 1,
    PO = 2,
    INTERNAL = 3
};

enum class ArcType : uint8_t
{
    GATE = 1,
    WIRE = 2
};

class TimingGraph
{
    public:
    TimingGraph() = default;
    auto add_pin(PinType pin_type) -> PinID;
    auto add_arc(PinID pin_1, PinID pin_2, ArcType arc_type, float early_delay, float late_delay) -> ArcID;
    auto set_arc_delay(ArcID arc_id, float early_delay, float late_delay) -> void;
    [[nodiscard]] auto get_fanout_arcs(PinID pin) const -> std::span<const ArcID>;
    [[nodiscard]] auto get_fanin_arcs(PinID pin) const -> std::span<const ArcID>;
    [[nodiscard]] auto get_early_delay(ArcID arc_id) const -> float;
    [[nodiscard]] auto get_late_delay(ArcID arc_id) const -> float;
    // Kahn's topological sort algorithm and checks for cycles
    [[nodiscard]] auto find_topological_levels() -> bool;
    [[nodiscard]] auto get_number_of_topological_levels() const noexcept -> uint32_t;
    [[nodiscard]] auto find_pins_in_level(uint32_t level_num) const -> std::span<const PinID>;
    [[nodiscard]] auto get_destination_pin(ArcID arc_id) const -> PinID;
    [[nodiscard]] auto get_source_pin(ArcID arc_id) const -> PinID;
    [[nodiscard]] auto get_number_of_pins() const noexcept-> uint32_t;
    [[nodiscard]] auto get_number_of_arcs() const noexcept-> uint32_t;
    auto update_input_pins(PinID pin_id) -> void;
    auto populate_pin_neighbors() -> void;
    [[nodiscard]] auto get_pin_to_level_map() const noexcept -> std::span<const uint32_t>;
    private:
    /*
     * We need a CSR for forward and backward traversal
     * We need to store the pin types and arc delays
     * We need to store the arc, the pair of pins. We know an arc can have only two endpoints
     * We store the source and destination pins of arcs in separate vectors
     */
    std::vector<ArcID> pin_successor_arcs;
    std::vector<uint32_t> pin_successor_arc_offsets;
    std::vector<ArcID> pin_predecessor_arcs;
    std::vector<uint32_t> pin_predecessor_arc_offsets;
    std::vector<float> arc_early_delays;
    std::vector<float> arc_late_delays;
    std::vector<ArcType> arc_types;
    std::vector<PinType> pin_types;
    std::vector<PinID> arc_destination_pins;
    std::vector<PinID> arc_source_pins;

    std::vector<PinID> input_pins;
    std::vector<uint8_t> pin_indegrees;
    std::vector<PinID> topological_levels;
    std::vector<uint32_t> topological_levels_offsets;
    std::vector<uint32_t> pin_to_levels_map;

};


#endif //STA_ENGINE_TIMING_GRAPH_H
