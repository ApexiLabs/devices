"""Add product firmware identities without changing legacy release assets."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def product_metadata(aggregate, product, hardware):
    if hardware['product'] != product:
        raise ValueError('hardware metadata belongs to a different product')
    targets = [target for target in aggregate['targets']
               if target['environment'].startswith(product + '-')]
    if not targets:
        raise ValueError('product has no release targets')
    return {
        'schema_version': 1,
        'product': product,
        'firmware_version': aggregate['version'],
        'commit_sha': aggregate['commit_sha'],
        'hardware_revision': hardware['hardware_revision'],
        'hardware_qualification': hardware['qualification'],
        'release_security_profile': aggregate['release_security_profile'],
        'targets': targets,
    }


def main():
    dist = ROOT / 'dist'
    aggregate = json.loads((dist / 'build-metadata.json').read_text())
    for product in ('logger', 'dash'):
        hardware = json.loads((ROOT / product / 'hardware/revision.json').read_text())
        metadata = product_metadata(aggregate, product, hardware)
        (dist / f'{product}-build-metadata.json').write_text(
            json.dumps(metadata, indent=2) + '\n')


if __name__ == '__main__':
    main()
