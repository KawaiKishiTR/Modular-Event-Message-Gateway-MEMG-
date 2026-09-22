#pragma once
#include "protocol.hpp"



bool validate_magic(const memg::PacketHeader* hdr);
bool validate_version(const memg::PacketHeader* hdr);
bool validate_header(const memg::PacketHeader* hdr);

