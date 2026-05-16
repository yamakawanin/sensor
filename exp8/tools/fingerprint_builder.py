import csv
import sys
from collections import defaultdict

def parse_row(row):
    if not row:
        return None
    location = row[0].strip()
    if not location:
        return None
    rssi = []
    for val in row[1:]:
        val = val.strip()
        if not val:
            continue
        rssi.append(int(val))
    if not rssi:
        return None
    return location, rssi


def main():
    if len(sys.argv) != 2:
        print("Usage: python fingerprint_builder.py samples.csv")
        sys.exit(1)

    path = sys.argv[1]
    samples = defaultdict(list)

    with open(path, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if header and header[0].lower().startswith("location"):
            pass
        else:
            if header:
                parsed = parse_row(header)
                if parsed:
                    samples[parsed[0]].append(parsed[1])
        for row in reader:
            parsed = parse_row(row)
            if parsed:
                samples[parsed[0]].append(parsed[1])

    if not samples:
        print("No valid samples found.")
        sys.exit(1)

    print("Averaged fingerprints:")
    for location, rows in samples.items():
        length = max(len(r) for r in rows)
        sums = [0] * length
        counts = [0] * length
        for r in rows:
            for i, val in enumerate(r):
                sums[i] += val
                counts[i] += 1
        avgs = [int(round(sums[i] / counts[i])) for i in range(length)]
        print(f"{location}: {avgs}")


if __name__ == "__main__":
    main()
