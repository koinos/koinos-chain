
class Context:
    def __init__(self):
        self.acc = []
        self.first_element = True

def ser_begin_object(ctx):
    ctx.acc.append("{")
    ctx.first_element = True

def ser_end_object(ctx):
    ctx.acc.append("}")
    ctx.first_element = False

def ser_object_field(ctx, field_name):
    if ctx.first_element:
        ctx.first_element = False
    else:
        ctx.acc.append(",")
    ctx.acc.append('"')
    ctx.acc.append(field_name)
    ctx.acc.append('":')

def ser_uint64(ctx, obj):
    ctx.acc.append(str(obj))

def ser_multihash_vector(ctx, obj):
    ctx.acc.append("<MHV>")
