
from dataclasses_json import dataclass_json

from dataclasses import dataclass, field
from typing import List, Tuple, Optional, Union

@dataclass_json
@dataclass
class Node:
    name: str
    sub: Union["AlphaNode", "BetaNode"]

@dataclass_json
@dataclass
class AlphaNode:
    suba: Node

@dataclass_json
@dataclass
class BetaNode:
    subb: Node

n = Node("hello", AlphaNode(Node("world", BetaNode(None))))

print(n.to_json())
