#!/usr/bin/env python3

import sys
from urllib.request import urlopen, Request
from urllib.parse import quote, quote_plus
import json
from datetime import datetime

import tqdm


dataset = "quora"
if len(sys.argv) > 1:
    dataset = sys.argv[1]

print(f"{dataset=}")


def feed(id: str, txt: str):
    global dataset
    data = {
        "_id": id,
        "text": txt
    }
    sdata = json.dumps(data).encode("utf-8")
    req = Request(
        f"http://localhost:8080/index?index={dataset}",
        data=sdata,
        method="POST",
        headers={"Content-Type": "application/json"}
    )

    with urlopen(req) as r:
        if r.status != 200:
            raise RuntimeError(f"invalid status {r.status}")


def search(q: str):
    global dataset
    q = quote(q)
    req = Request(
        f"http://localhost:8080/search?q={q}&index={dataset}",
        method="GET",
        headers={"Content-Type": "application/json"}
    )

    with urlopen(req) as r:
        if r.status != 200:
            raise RuntimeError(f"invalid status {r.status}")


with open(f"datasets/{dataset}/corpus.jsonl", "r") as f:
    for line in tqdm.tqdm(f):
        obj = json.loads(line)
        feed(obj["_id"], obj["text"])


a = datetime.now()

count = 0

with open(f"datasets/{dataset}/queries.jsonl", "r") as f:
    for line in tqdm.tqdm(f):
        obj = json.loads(line)
        search(obj["text"])
        count += 1

b = datetime.now()
total = (b - a).total_seconds()
avg = total / count * 1000
print(f"{total=:.3} s | {avg=:.3} ms")
