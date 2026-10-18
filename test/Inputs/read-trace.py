# read-trace.py < trace.pbf: print a templight protobuf trace (templight_messages.proto), read from standard
# input, as text, one line per header and entry, without templight's own reader. A truncated trailing record
# ends the listing with a "truncated" line instead of an error, so a trace written up to a crash can be checked.
import struct
import sys


def varint(buf, pos):
    value, shift = 0, 0
    while True:
        if pos >= len(buf):
            raise EOFError
        byte = buf[pos]
        pos += 1
        value |= (byte & 0x7F) << shift
        shift += 7
        if not byte & 0x80:
            return value, pos


def fields(buf):
    pos = 0
    while pos < len(buf):
        key, pos = varint(buf, pos)
        number, wire = key >> 3, key & 7
        if wire == 0:
            value, pos = varint(buf, pos)
        elif wire == 1:
            if pos + 8 > len(buf):
                raise EOFError
            value, pos = struct.unpack("<d", buf[pos:pos + 8])[0], pos + 8
        elif wire == 2:
            size, pos = varint(buf, pos)
            if pos + size > len(buf):
                raise EOFError
            value, pos = buf[pos:pos + size], pos + size
        else:
            raise ValueError("unsupported wire type %d" % wire)
        yield number, value


def name_of(dictionary, ident):
    marked, markers = dictionary[ident]
    out, markers = [], list(markers)
    for ch in marked.decode("utf-8", "replace"):
        out.append(name_of(dictionary, markers.pop(0)) if ch == "\0" else ch)
    return "".join(out)


def template_name(dictionary, buf):
    for number, value in fields(buf):
        if number == 1:
            return value.decode("utf-8", "replace")
        if number == 3:
            return name_of(dictionary, value)
    return "?"


def location(buf, files):
    name, ident, line, column = None, None, 0, 0
    for number, value in fields(buf):
        if number == 1:
            name = value.decode("utf-8", "replace")
        elif number == 2:
            ident = value
        elif number == 3:
            line = value
        elif number == 4:
            column = value
    if name is not None:
        files[ident] = name
    return "%s:%d:%d" % (files.get(ident, "?"), line, column)


def main():
    data = sys.stdin.buffer.read()
    dictionary, files = [], {}
    try:
        for number, trace in fields(data):
            if number != 1:
                break
            for field, value in fields(trace):
                if field == 1:
                    header = dict(fields(value))
                    print("header version=%d source=%s" % (header.get(1, 0), header.get(2, b"").decode()))
                elif field == 3:
                    entry = list(fields(value))
                    dictionary.append((dict(entry).get(1, b""), [v for n, v in entry if n == 2]))
                elif field == 2:
                    for kind, body in fields(value):
                        parts = dict(fields(body))
                        if kind == 1:
                            print("begin kind=%d name=%s at=%s" % (parts.get(1, -1),
                                  template_name(dictionary, parts.get(2, b"")),
                                  location(parts.get(3, b""), files)))
                        elif kind == 2:
                            print("end")
    except EOFError:
        print("truncated")
    print("entries-done")


if __name__ == "__main__":
    main()
