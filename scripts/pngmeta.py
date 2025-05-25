import json
from PIL import Image, PngImagePlugin
import argparse
import os
import tempfile
import shutil

def flatten_json(nested, parent_key='', sep='.'):
    """Flatten a nested JSON object using dot notation."""
    items = {}
    for k, v in nested.items():
        new_key = f"{parent_key}{sep}{k}" if parent_key else k
        if isinstance(v, dict):
            items.update(flatten_json(v, new_key, sep=sep))
        else:
            items[new_key] = str(v)
    return items

def unflatten_json(flat, sep='.'):
    """Unflatten a dot-notation JSON object back to a nested dictionary."""
    nested = {}
    for compound_key, value in flat.items():
        keys = compound_key.split(sep)
        d = nested
        for key in keys[:-1]:
            d = d.setdefault(key, {})
        d[keys[-1]] = value
    return nested

def write_metadata_in_place(png_path, json_path):
    """Embed flattened metadata into a PNG file in-place."""
    with open(json_path, 'r') as f:
        metadata = json.load(f)

    if not isinstance(metadata, dict):
        raise ValueError("JSON must contain a top-level object.")

    flat_metadata = flatten_json(metadata)

    img = Image.open(png_path)
    meta = PngImagePlugin.PngInfo()
    for k, v in flat_metadata.items():
        meta.add_text(k, v)

    # Save to a temporary file, then move it over original
    with tempfile.NamedTemporaryFile(delete=False, suffix=".png") as tmp:
        tmp_path = tmp.name
    img.save(tmp_path, pnginfo=meta)
    shutil.move(tmp_path, png_path)
    print(f"Metadata embedded into {png_path} (in-place)")

def extract_metadata_from_png(png_path, json_output_path):
    """Extract flat PNG text chunks and reconstruct nested JSON."""
    img = Image.open(png_path)
    raw_metadata = {k: img.info[k] for k in img.info if isinstance(img.info[k], str)}
    nested_metadata = unflatten_json(raw_metadata)

    with open(json_output_path, 'w') as f:
        json.dump(nested_metadata, f, indent=4)

    print(f"Metadata extracted to {json_output_path}")

def main():
    parser = argparse.ArgumentParser(description="Embed or extract hierarchical JSON metadata from PNG images.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Write subcommand
    write_parser = subparsers.add_parser('write', help='Embed JSON metadata into a PNG image (in-place).')
    write_parser.add_argument('png_file', help='PNG image to update')
    write_parser.add_argument('json_file', help='Input JSON metadata file')

    # Read subcommand
    read_parser = subparsers.add_parser('read', help='Extract metadata from a PNG image to a JSON file.')
    read_parser.add_argument('png_file', help='Input PNG image')
    read_parser.add_argument('json_file', help='Output JSON file')

    args = parser.parse_args()

    if args.command == 'write':
        write_metadata_in_place(args.png_file, args.json_file)
    elif args.command == 'read':
        extract_metadata_from_png(args.png_file, args.json_file)

if __name__ == "__main__":
    main()
