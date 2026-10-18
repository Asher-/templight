# read-xml-trace.py < trace: parse a templight xml, nestedxml or graphml trace from standard input with
# the standard library's XML parser, which refuses a document that is not well-formed, and print its root
# and then, in document order, one line per element naming an instantiation or a graph edge.
import sys
import xml.etree.ElementTree as ElementTree


def local(tag):
    return tag.rsplit("}", 1)[-1]


def main():
    try:
        root = ElementTree.fromstring(sys.stdin.buffer.read())
    except ElementTree.ParseError as error:
        print("parse-error %s" % error)
        return 1
    print("root=%s" % local(root.tag))
    for element in root.iter():
        tag = local(element.tag)
        if tag == "Context":
            print("context %s" % element.get("context"))
        elif tag == "Entry":
            print("entry %s" % element.get("Name"))
        elif tag == "Exit":
            print("exit")
        elif tag == "node":
            names = [d.text for d in element if local(d.tag) == "data" and d.get("key") == "d1"]
            print("node %s %s" % (element.get("id"), names[0] if names else ""))
        elif tag == "edge":
            print("edge %s->%s" % (element.get("source"), element.get("target")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
