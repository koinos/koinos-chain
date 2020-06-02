
import typing

Vector = typing.List
Variant = typing.Union
Uint32 = int
Uint64 = int
MultihashType = bytes
MultihashVector = bytes
VlBlob = bytes
FlBlob = bytes

class FlBlobImpl:
    def __new__(cls, fl_blob_length=-1, origin=None):
        # Hacky implementation allowing the syntax
        #     member: FlBlob[n]
        # In reality FlBlob[n] just evaluates to `bytes` for any n.  So the information about n is lost.
        # The serializer still knows the value of n, because the code generator routes information
        # about the value of n to a serializer in generated serializer calls.
        # See https://github.com/python/mypy/issues/3345
        #
        self = super().__new__(cls)
        self.fl_blob_length = fl_blob_length
        self.origin = origin
        if origin is None:
            return self
        elif origin is FlBlob:
            if fl_blob_length < 0:
                raise RuntimeError("Bad fl_blob_length")
            return bytes
        else:
            raise RuntimeError("Bad origin, cannot subclass FlBlob")

    def __getitem__(self, fl_blob_length):
        return self.__class__(fl_blob_length=fl_blob_length, origin=self)

FlBlob = FlBlobImpl()
