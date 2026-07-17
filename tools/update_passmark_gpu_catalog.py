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
import json
import re
import sys
import urllib.parse
import urllib.request
from html.parser import HTMLParser
from pathlib import Path


SOURCE_URL = "https://www.videocardbenchmark.net/gpu_list.php"
GPUINFO_API_URL = "https://vulkan.gpuinfo.org/api/internal/devices.php"
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
    gpuinfo = parser.add_mutually_exclusive_group()
    gpuinfo.add_argument(
        "--include-gpuinfo",
        action="store_true",
        help="fetch one desktop device-name listing from gpuinfo.org (with API permission) and replace matched names",
    )
    gpuinfo.add_argument(
        "--gpuinfo-input",
        type=Path,
        help="read a previously saved gpuinfo.org devices API response instead of accessing the service",
    )
    parser.add_argument("--gpuinfo-url", default=GPUINFO_API_URL)
    parser.add_argument("--gpuinfo-platform", choices=("windows", "linux"), default="windows")
    parser.add_argument(
        "--gpuinfo-raw-output",
        type=Path,
        default=local_directory / "vulkan_gpuinfo_devices.json",
    )
    parser.add_argument(
        "--acknowledge-terms",
        action="store_true",
        help="confirm that the downloaded data will remain local and be used in accordance with PassMark's terms",
    )
    return parser.parse_args()


def download(url: str, accept: str = "text/html") -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": accept})
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read()


COMPANY_NOISE = {
    "amd",
    "ati",
    "nvidia",
    "intel",
    "advanced",
    "micro",
    "devices",
    "corporation",
    "corp",
    "inc",
    "tm",
    "r",
}
RELAXED_NOISE = COMPANY_NOISE | {"geforce", "radeon", "graphics", "gpu"}


def model_tokens(value: str, relaxed: bool = False) -> list[str]:
    noise = RELAXED_NOISE if relaxed else COMPANY_NOISE
    return [
        token
        for token in re.findall(r"[a-z0-9]+", value.lower())
        if token not in noise and token not in {"with", "design"}
    ]


def normalized_vendor(vendor: str, model: str = "") -> str:
    combined = f"{vendor} {model}".lower()
    if any(name in combined for name in ("nvidia", "geforce", "quadro", "tesla")):
        return "nvidia"
    if any(name in combined for name in ("advanced micro devices", "amd", "radeon", "firepro", "firegl")):
        return "amd"
    if any(name in combined for name in ("intel", "iris", "uhd graphics", "hd graphics")) or "arc " in combined:
        return "intel"
    return vendor.strip().lower()


def fuzzy_prefix_distance(first: list[str], second: list[str]) -> int | None:
    shorter, longer = (first, second) if len(first) <= len(second) else (second, first)
    if len(shorter) < 2 or len(shorter) == len(longer) or shorter != longer[: len(shorter)]:
        return None
    if not any(any(character.isdigit() for character in token) for token in shorter):
        return None
    return len(longer) - len(shorter)


def match_passmark_row(name: str, vendor: str, rows: list[tuple[str, int, int]]) -> int | None:
    strict = model_tokens(name)
    relaxed = model_tokens(name, relaxed=True)
    source_vendor = normalized_vendor(vendor, name)
    strict_matches: list[int] = []
    relaxed_matches: list[int] = []
    fuzzy_matches: list[tuple[int, int]] = []

    for index, (candidate, _score, _rank) in enumerate(rows):
        candidate_vendor = normalized_vendor("", candidate)
        if source_vendor and candidate_vendor and source_vendor != candidate_vendor:
            continue
        candidate_strict = model_tokens(candidate)
        if strict and candidate_strict == strict:
            strict_matches.append(index)
        if relaxed and model_tokens(candidate, relaxed=True) == relaxed:
            relaxed_matches.append(index)
        distance = fuzzy_prefix_distance(strict, candidate_strict)
        if distance is not None:
            fuzzy_matches.append((distance, index))

    for matches in (strict_matches, relaxed_matches):
        unique = list(dict.fromkeys(matches))
        if len(unique) == 1:
            return unique[0]
        if unique:
            return None
    if not fuzzy_matches:
        return None
    nearest_distance = min(distance for distance, _index in fuzzy_matches)
    nearest = {index for distance, index in fuzzy_matches if distance == nearest_distance}
    return min(nearest, key=lambda index: (rows[index][1], rows[index][0]))


def gpuinfo_request_url(base_url: str, platform: str) -> str:
    columns = ("device", "api", "driver", "submissiondate", "reportcount", "compare")
    parameters: dict[str, str] = {
        "platform": platform,
        "draw": "1",
        "start": "0",
        "length": "10000",
        "search[value]": "",
        "search[regex]": "false",
        "order[0][column]": "device",
        "order[0][dir]": "asc",
        "filter[extension]": "",
        "filter[submitter]": "",
        "filter[apiversion]": "",
    }
    for index, column in enumerate(columns):
        parameters[f"columns[{index}][data]"] = column
        parameters[f"columns[{index}][searchable]"] = "true" if index < 3 else "false"
        parameters[f"columns[{index}][orderable]"] = "false" if index == 5 else "true"
        parameters[f"columns[{index}][search][value]"] = ""
        parameters[f"columns[{index}][search][regex]"] = "false"
    parsed = urllib.parse.urlsplit(base_url)
    parameters.update(dict(urllib.parse.parse_qsl(parsed.query)))
    parameters["platform"] = platform
    return urllib.parse.urlunsplit(
        (parsed.scheme, parsed.netloc, parsed.path, urllib.parse.urlencode(parameters), parsed.fragment)
    )


def clean_gpuinfo_name(name: str, vendor: str) -> str:
    cleaned = " ".join(name.replace("|", " ").split())
    tokens = cleaned.split()
    if len(tokens) >= 2 and vendor and tokens[0].lower() == vendor.lower() and tokens[1].lower() == vendor.lower():
        del tokens[0]
    return " ".join(tokens)


def parse_gpuinfo_devices(payload: bytes) -> list[tuple[str, str, int]]:
    try:
        response = json.loads(payload)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"gpuinfo.org response is not valid JSON: {error}") from error
    if not isinstance(response, dict) or not isinstance(response.get("data"), list):
        raise ValueError("gpuinfo.org response does not contain a data array")
    total = response.get("recordsFiltered", response.get("recordsTotal"))
    if isinstance(total, int) and total > len(response["data"]):
        raise ValueError(f"gpuinfo.org response was truncated ({len(response['data'])} of {total} devices)")

    devices: list[tuple[str, str, int]] = []
    for item in response["data"]:
        if not isinstance(item, dict):
            continue
        name = item.get("gpuname")
        vendor = item.get("vendor") or ""
        report_count = item.get("reportcount", 0)
        if (
            isinstance(name, str)
            and name.strip()
            and isinstance(vendor, str)
            and isinstance(report_count, int)
            and report_count >= 0
        ):
            vendor = vendor.strip()
            devices.append((clean_gpuinfo_name(name, vendor), vendor, report_count))
    if not devices:
        raise ValueError("gpuinfo.org response contains no GPU model names")
    return devices


def merge_gpuinfo_names(
    rows: list[tuple[str, int, int]], devices: list[tuple[str, str, int]]
) -> tuple[dict[int, str], dict[int, list[str]], int]:
    matches_by_row: dict[int, dict[str, int]] = {}
    matched = 0
    for name, vendor, report_count in devices:
        index = match_passmark_row(name, vendor, rows)
        if index is None:
            continue
        matched += 1
        names = matches_by_row.setdefault(index, {})
        names[name] = names.get(name, 0) + report_count

    canonical_names: dict[int, str] = {}
    aliases: dict[int, list[str]] = {}
    for index, names in matches_by_row.items():
        canonical = min(names, key=lambda name: (-names[name], len(name), name.lower()))
        canonical_names[index] = canonical
        row_aliases = [rows[index][0]] if rows[index][0] != canonical else []
        row_aliases.extend(
            name
            for name in sorted(names, key=lambda name: (-names[name], len(name), name.lower()))
            if name != canonical and name not in row_aliases
        )
        if row_aliases:
            aliases[index] = row_aliases
    return canonical_names, aliases, matched


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

    aliases: dict[int, list[str]] = {}
    canonical_names: dict[int, str] = {}
    gpuinfo_devices: list[tuple[str, str, int]] = []
    gpuinfo_matched = 0
    gpuinfo_retrieved_at = ""
    gpuinfo_source_url = f"https://vulkan.gpuinfo.org/listdevices.php?platform={arguments.gpuinfo_platform}"
    if arguments.include_gpuinfo or arguments.gpuinfo_input is not None:
        if arguments.gpuinfo_input is not None:
            gpuinfo_payload = arguments.gpuinfo_input.read_bytes()
        else:
            request_url = gpuinfo_request_url(arguments.gpuinfo_url, arguments.gpuinfo_platform)
            gpuinfo_payload = download(request_url, accept="application/json")
            arguments.gpuinfo_raw_output.parent.mkdir(parents=True, exist_ok=True)
            arguments.gpuinfo_raw_output.write_bytes(gpuinfo_payload)
        try:
            gpuinfo_devices = parse_gpuinfo_devices(gpuinfo_payload)
        except ValueError as error:
            print(f"error: {error}", file=sys.stderr)
            return 1
        canonical_names, aliases, gpuinfo_matched = merge_gpuinfo_names(parser.rows, gpuinfo_devices)
        gpuinfo_retrieved_at = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()

    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    retrieved_at = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()
    with arguments.output.open("w", encoding="utf-8", newline="") as output:
        output.write("# source=PassMark G3D Mark\n")
        output.write(f"# source_url={arguments.url}\n")
        output.write(f"# retrieved_at={retrieved_at}\n")
        output.write("# copyright=Copyright PassMark Software; local use only; do not redistribute without permission\n")
        if gpuinfo_devices:
            output.write("# model_source=Vulkan Hardware Database by Sascha Willems\n")
            output.write(f"# model_source_url={gpuinfo_source_url}\n")
            output.write(f"# model_retrieved_at={gpuinfo_retrieved_at}\n")
            output.write("# model_license=Creative Commons Attribution 4.0 International (CC BY 4.0)\n")
            output.write("# model_changes=Matched device names replace PassMark names; original names are aliases\n")
        writer = csv.writer(output)
        writer.writerow(["canonical_model", "score", "rank", "vendor", "aliases", "vendor_id", "device_id"])
        for index, (name, score, rank) in enumerate(parser.rows):
            writer.writerow([canonical_names.get(index, name), score, rank, "", "|".join(aliases.get(index, [])), "", ""])

    print(f"saved {len(parser.rows)} GPU entries to {arguments.output}")
    print(f"saved the untouched source page to {arguments.raw_output}")
    if gpuinfo_devices:
        alias_count = sum(len(names) for names in aliases.values())
        renamed_count = sum(canonical_names[index] != parser.rows[index][0] for index in canonical_names)
        print(
            f"matched {gpuinfo_matched} of {len(gpuinfo_devices)} gpuinfo.org desktop names; "
            f"renamed {renamed_count} canonical models and added {alias_count} aliases"
        )
        if arguments.include_gpuinfo:
            print(f"saved the untouched gpuinfo.org response to {arguments.gpuinfo_raw_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
