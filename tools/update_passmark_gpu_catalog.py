#!/usr/bin/env python3
"""Build a local-only GPU score catalog from PassMark's public model list.

PassMark's page and benchmark compilation are copyrighted. This tool keeps both
the untouched response and the derived CSV under data/local/, which is ignored
by Git. Do not redistribute either file without permission from PassMark.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import re
import sys
import urllib.request
from html.parser import HTMLParser
from pathlib import Path


SOURCE_URL = "https://www.videocardbenchmark.net/gpu_list.php"
USER_AGENT = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 Chrome/138.0 Safari/537.36"
)


class PassMarkTableParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.in_table = False
        self.in_body = False
        self.in_row = False
        self.in_cell = False
        self.cell_text: list[str] = []
        self.row: list[str] = []
        self.rows: list[tuple[str, int, int]] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attributes = {key.lower(): (value or "") for key, value in attrs}
        if tag.lower() == "table" and attributes.get("id", "").lower() == "cputable":
            self.in_table = True
        elif self.in_table and tag.lower() == "tbody":
            self.in_body = True
        elif self.in_body and tag.lower() == "tr":
            self.in_row = True
            self.row = []
        elif self.in_row and tag.lower() == "td":
            self.in_cell = True
            self.cell_text = []

    def handle_data(self, data: str) -> None:
        if self.in_cell:
            self.cell_text.append(data)

    def handle_endtag(self, tag: str) -> None:
        lowered = tag.lower()
        if self.in_cell and lowered == "td":
            self.row.append(" ".join("".join(self.cell_text).split()))
            self.cell_text = []
            self.in_cell = False
        elif self.in_row and lowered == "tr":
            if len(self.row) >= 3:
                score = self._positive_integer(self.row[1])
                rank = self._positive_integer(self.row[2])
                if self.row[0] and score is not None and rank is not None:
                    self.rows.append((self.row[0], score, rank))
            self.row = []
            self.in_row = False
        elif self.in_body and lowered == "tbody":
            self.in_body = False
        elif self.in_table and lowered == "table":
            self.in_table = False

    @staticmethod
    def _positive_integer(value: str) -> int | None:
        compact = value.replace(",", "").strip()
        if not re.fullmatch(r"[0-9]+", compact):
            return None
        parsed = int(compact)
        return parsed if parsed > 0 else None


def parse_arguments() -> argparse.Namespace:
    repository = Path(__file__).resolve().parents[1]
    local_directory = repository / "data" / "local"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default=SOURCE_URL)
    parser.add_argument("--output", type=Path, default=local_directory / "passmark_gpu_catalog.csv")
    parser.add_argument("--raw-output", type=Path, default=local_directory / "passmark_gpu_list.html")
    parser.add_argument(
        "--acknowledge-terms",
        action="store_true",
        help="confirm that the downloaded data will remain local and be used in accordance with PassMark's terms",
    )
    return parser.parse_args()


def download(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "text/html"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read()


def main() -> int:
    arguments = parse_arguments()
    if not arguments.acknowledge_terms:
        print("error: pass --acknowledge-terms after reviewing PassMark's terms of use", file=sys.stderr)
        return 2

    payload = download(arguments.url)
    if b"Video Card Benchmark data Copyrighted by Passmark Software" not in payload:
        print("error: the downloaded page did not contain the expected PassMark copyright notice", file=sys.stderr)
        return 1

    parser = PassMarkTableParser()
    parser.feed(payload.decode("utf-8", errors="replace"))
    if len(parser.rows) < 100:
        print(f"error: expected at least 100 GPU rows, parsed {len(parser.rows)}", file=sys.stderr)
        return 1

    arguments.raw_output.parent.mkdir(parents=True, exist_ok=True)
    arguments.raw_output.write_bytes(payload)

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    retrieved_at = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()
    with arguments.output.open("w", encoding="utf-8", newline="") as output:
        output.write("# source=PassMark G3D Mark\n")
        output.write(f"# source_url={arguments.url}\n")
        output.write(f"# retrieved_at={retrieved_at}\n")
        output.write("# copyright=Copyright PassMark Software; local use only; do not redistribute without permission\n")
        writer = csv.writer(output)
        writer.writerow(["canonical_model", "score", "rank", "vendor", "aliases", "vendor_id", "device_id"])
        for name, score, rank in parser.rows:
            writer.writerow([name, score, rank, "", "", "", ""])

    print(f"saved {len(parser.rows)} GPU entries to {arguments.output}")
    print(f"saved the untouched source page to {arguments.raw_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
