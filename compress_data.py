"""
PlatformIO Pre-Script zum automatischen Komprimieren aller Dateien im data/ Ordner.
Wird automatisch vor dem Filesystem-Upload ausgeführt.
"""

Import("env")
import gzip
import hashlib
import os
import shutil
from pathlib import Path


def compress_file(input_path, output_path):
    """Komprimiert eine Datei mit gzip und überschreibt bestehende .gz Dateien."""
    with open(input_path, 'rb') as f_in:
        with gzip.open(output_path, 'wb') as f_out:
            shutil.copyfileobj(f_in, f_out)
    
    original_size = os.path.getsize(input_path)
    compressed_size = os.path.getsize(output_path)
    compression_ratio = (1 - compressed_size / original_size) * 100
    
    print(f"✓ {input_path.name} -> {output_path.name} ({compression_ratio:.1f}% kleiner)")

def generate_fs_hash(data_dir):
    """Generiert einen Hash über alle Dateien im data/ Ordner."""
    combined_hash = hashlib.md5()
    
    # Alle Dateien sortiert verarbeiten für konsistenten Hash
    files = sorted([f for f in data_dir.iterdir() if f.is_file() and not f.name.endswith('.gz') and f.name != '.hash'])
    
    for file_path in files:
        with open(file_path, 'rb') as f:
            while chunk := f.read(8192):
                combined_hash.update(chunk)
    
    return combined_hash.hexdigest()

def compress_data_files(*args, **kwargs):
    """PlatformIO Pre-Action: Komprimiert alle Dateien im data/ Ordner."""
    print("🗜️  [PRE-SCRIPT] Komprimiere Dateien für LittleFS...")
    
    data_dir = Path("data")
    
    if not data_dir.exists():
        print("❌ data/ Ordner nicht gefunden!")
        return
    
    # Generiere Filesystem-Hash
    fs_hash = generate_fs_hash(data_dir)
    
    # Speichere Hash in .hash Datei
    with open(data_dir / '.hash', 'w') as f:
        f.write(fs_hash)
    
    print(f"📦 Filesystem Hash: {fs_hash}")
    
    # Alle Dateien komprimieren (außer bereits .gz Dateien und .hash)
    compressed_count = 0
    for file_path in data_dir.iterdir():
        if file_path.is_file() and not file_path.name.endswith('.gz') and file_path.name != '.hash':
            gz_path = file_path.with_suffix(file_path.suffix + '.gz')
            compress_file(file_path, gz_path)
            compressed_count += 1
    
    print(f"✅ {compressed_count} Dateien komprimiert!")

# Komprimiere Dateien direkt beim Import des Scripts
compress_data_files()
