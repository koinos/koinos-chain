## Base Types

#### Integer Types:

Network Byte Order (Big-Endian) for the fixed length integer size.

Signed/Unsigned types for 8, 16, 32, 64, 128, 160, and 256 bit length integers


#### Bool:

An 8 bit integer. For simplicity, only 0x00 and 0x01 are valid values when a bool is explicitly specified.


#### Varint:

Varint's can hold a maximum of 64 bits of data (increased from 32 for BitShares/Steem). The first bit of an octet signs if the varint continues or not (1 for continuation, 0 for stop). To encode signed varints, a transfomation to zigzag encoding (used by protobufs) is first applied `(n<<1)^(n>>64)` before encoding the now unsigned value as a varint. The max wire size of a varint can be 80 bits total.


#### Object:

Each field of the object is serialized inline according to the object definition.


#### Vector<T>:

A varint size of the vector, followed by that many objects in the vector using their binary encoding.


#### Array<T,N>:

A fixed length container of N items serialized in their binary encoding.


#### Map<K,V>:

A varint size of the map followed by pairs serializing keys and values in the binary format.


#### Variant<T>:

A varint key, followed by the binary encoding of whatever object is held by that key.


#### Optional<T>:

A bool specifying if the optional is valid. If true, it is followed by the binary serialization of the contained type.


#### Multihash:

A varint hash function (key), followed by a varint digest size in bytes, followed by Network Byte Order hash for the specified length.


#### Multihash Vector:

A varint hash function (key), followed by a varint digest size in bytes, followed by a varint size of the vector, followed by that many digests each of the specified digest size.


#### Variable Length Blob:

Serialized as a vector<char>


#### Fixed Length Blob:

The language specifies the fixed length of the blob, in which case it is simply a fixed number of bytes.


## Other common types:

**Array:** A Fixed Length Blob

**String:** A Variable Length Blob

**Timestamp:** A signed 64 bit integer

**Set:** A vector where uniqueness is enforced in the language. For verifying correctness of a set, it should be deserialized, and serialized to check the signature rather than checking the signature directly against the incoming byte stream.