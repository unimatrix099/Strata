#!/usr/bin/env python3
"""bench.py with eight prompts instead of two (so a change that alters the text averages over more answers):
bench_many.py <bench.py arguments>"""
import importlib.util, os, sys
spec = importlib.util.spec_from_file_location("bench", os.path.join(os.path.dirname(__file__), "..", "..", "bench",
                                              "results", "2026-10-05-split-decode-7900xtx", "bench.py"))
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)
bench.PROMPTS = {
    "story": bench.PROMPTS["story"],
    "code": bench.PROMPTS["code"],
    "explain": "Explain how a refrigerator works to a 12-year-old, in about 300 words.",
    "email": "Write a polite email to a landlord asking to fix a broken heater, mentioning it has been a week.",
    "sql": "Write a SQL query that finds the top 5 customers by total order value in 2025, with table definitions.",
    "list": "Give me 15 ideas for a weekend trip near Munich, one line each with why it is worth it.",
    "math": "Solve step by step: a train leaves at 9:40 at 84 km/h, another at 10:10 at 105 km/h on the same track; "
            "when does the second catch up?",
    "rust": "Write a Rust function that parses a CSV line with quoted fields into a Vec<String>, with tests.",
}
sys.exit(bench.main())
