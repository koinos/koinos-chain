#!/usr/bin/env python3

from koinos_protocol.classes import BlockHeaderType as BlockHeader
from koinos_protocol.json import ser_block_header_type
from koinos_protocol.rt.json import Context

header = BlockHeader()

ctx = Context()

ser_block_header_type(ctx, header)

print(header)
print("".join(ctx.acc))
