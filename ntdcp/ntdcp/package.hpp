#pragma once

#include <cstdint>

namespace ntdcp
{

enum class TransportType
{
    no_acknoledgement = 0,
    retransmission_limit,
    unbreackable
};

struct Package
{
    uint64_t source_addr = 0;
    uint64_t destination_addr = 0;

    uint16_t source_port = 0;
    uint16_t destination_port = 0;

    uint16_t package_id = 0;
    uint16_t session_id = 0;

    uint16_t acknoledgement_for_id = 0;

    uint8_t hop_limit = 255;

    TransportType transport_type = TransportType::no_acknoledgement;

    uint8_t* data = nullptr;
    uint16_t size = 0;
};

bool parse_package(Package& out, uint8_t* buffer, uint16_t buffer_size);
uint16_t package_size(const Package& out);
void write_package(const Package& package, uint8_t* buffer);


}