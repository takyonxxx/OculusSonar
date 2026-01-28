#!/usr/bin/env python3
"""
SONAR YOLO TRAINING - Windows/Linux
Opset 21 ONNX export with optimized training parameters
"""

import os
import shutil
from pathlib import Path
import yaml

print("=" * 60)
print("SONAR YOLO TRAINING - IMPROVED")
print("=" * 60)

# 1. BASE DIRECTORY
script_dir = Path(__file__).parent
print(f"\nScript dizini: {script_dir}")

# 2. ZIP DOSYASINI AÇ
zip_file = script_dir / "sonar_model.zip"
if not zip_file.exists():
    print(f"\n✗ HATA: {zip_file} bulunamadı!")
    print("\nsonar_model.zip'i bu klasöre kopyala:")
    print(f"  {script_dir}")
    exit(1)

print(f"✓ ZIP bulundu: {zip_file}")

# ZIP'i aç
import zipfile
extract_dir = script_dir / "dataset"
if extract_dir.exists():
    shutil.rmtree(extract_dir)
extract_dir.mkdir()

print("\nZIP açılıyor...")
with zipfile.ZipFile(zip_file, 'r') as zip_ref:
    zip_ref.extractall(extract_dir)
print(f"✓ Dataset açıldı: {extract_dir}")

# 3. YOLO DATASET YAPISI OLUŞTUR
yolo_dir = script_dir / "yolo_dataset"
if yolo_dir.exists():
    shutil.rmtree(yolo_dir)

(yolo_dir / "train" / "images").mkdir(parents=True)
(yolo_dir / "train" / "labels").mkdir(parents=True)
(yolo_dir / "val" / "images").mkdir(parents=True)
(yolo_dir / "val" / "labels").mkdir(parents=True)

# 4. GÖRÜNTÜLERİ KOPYALA
print("\nGörüntüler kopyalanıyor...")

images = sorted(list((extract_dir / "obj_Train_data").glob("*.png")))
print(f"Toplam: {len(images)} görüntü")

if len(images) == 0:
    print("✗ HATA: Görüntü bulunamadı!")
    exit(1)

# Split: 80% train, 20% val
split_idx = int(len(images) * 0.8)
train_images = images[:split_idx]
val_images = images[split_idx:]

print(f"Train: {len(train_images)} görüntü")
print(f"Val: {len(val_images)} görüntü")

# Train
for img in train_images:
    shutil.copy(img, yolo_dir / "train" / "images" / img.name)
    label = img.with_suffix('.txt')
    if label.exists():
        shutil.copy(label, yolo_dir / "train" / "labels" / label.name)

# Validation
for img in val_images:
    shutil.copy(img, yolo_dir / "val" / "images" / img.name)
    label = img.with_suffix('.txt')
    if label.exists():
        shutil.copy(label, yolo_dir / "val" / "labels" / label.name)

print("✓ Dosyalar kopyalandı")

# 5. DATA.YAML
print("\ndata.yaml oluşturuluyor...")

data_yaml = {
    'path': str(yolo_dir.absolute()),
    'train': 'train/images',
    'val': 'val/images',
    'nc': 1,  # Number of classes
    'names': ['Object']  # Class names
}

with open(yolo_dir / "data.yaml", 'w') as f:
    yaml.dump(data_yaml, f)

print(f"✓ data.yaml: {yolo_dir / 'data.yaml'}")

# 6. ULTRALYTICS KONTROL
print("\n" + "=" * 60)
print("YOLOv8 KURULUMU")
print("=" * 60)

try:
    from ultralytics import YOLO
    print("✓ Ultralytics YOLOv8 hazır")
except ImportError:
    print("✗ Ultralytics kurulu değil")
    print("\nKurulum yapılıyor...")
    os.system("pip install ultralytics")
    from ultralytics import YOLO
    print("✓ Kurulum tamamlandı")

# 7. TRAINING WITH IMPROVED PARAMETERS
print("\n" + "=" * 60)
print("MODEL EĞİTİMİ BAŞLIYOR")
print("=" * 60)
print("\nİyileştirilmiş parametrelerle eğitim başlıyor...")
print("Bu işlem 1-3 saat sürebilir (CPU'ya göre)\n")

model = YOLO('yolov8n.pt')

# Improved training parameters for sonar detection
results = model.train(
    data=str(yolo_dir / "data.yaml"),
    epochs=150,              # Increased for better convergence
    imgsz=640,
    batch=8,                 # Adjust based on your RAM
    name='sonar_model',
    project=str(script_dir / "runs"),
    patience=25,             # Early stopping patience
    save=True,
    device='cpu',            # Change to 'cuda' or '0' if you have GPU
    workers=4,               # Increase if you have more CPU cores
    
    # Optimization parameters
    optimizer='Adam',        # Adam often works better for small datasets
    lr0=0.001,              # Initial learning rate
    lrf=0.01,               # Final learning rate
    momentum=0.937,
    weight_decay=0.0005,
    
    # Augmentation (important for sonar images)
    hsv_h=0.015,            # HSV hue augmentation
    hsv_s=0.3,              # HSV saturation
    hsv_v=0.2,              # HSV value
    degrees=10.0,           # Rotation degrees
    translate=0.1,          # Translation
    scale=0.3,              # Scale augmentation
    shear=0.0,              # Shear
    perspective=0.0,        # Perspective
    flipud=0.5,             # Flip up-down
    fliplr=0.5,             # Flip left-right
    mosaic=1.0,             # Mosaic augmentation
    mixup=0.1,              # Mixup augmentation
    
    # Other settings
    verbose=True,
    plots=True,
    save_period=10          # Save checkpoint every 10 epochs
)

# 8. ONNX EXPORT WITH OPSET 21
print("\n" + "=" * 60)
print("ONNX EXPORT - OPSET 21")
print("=" * 60)

best_pt = script_dir / "runs" / "sonar_model" / "weights" / "best.pt"
if best_pt.exists():
    print(f"\n✓ Best model: {best_pt}")
    
    model = YOLO(str(best_pt))
    
    # Export with opset 21 for compatibility
    onnx_path = model.export(
        format='onnx',
        imgsz=640,
        simplify=True,
        opset=21,           # Opset 21 for better compatibility
        dynamic=False       # Fixed input size for faster inference
    )
    
    print(f"✓ ONNX export: {onnx_path}")
    
    # Copy to project root as sonar_model.onnx (application looks for this name)
    output = script_dir / "sonar_model.onnx"
    shutil.copy(onnx_path, output)
    print(f"✓ Kaydedildi: {output}")
    
    # Print model info
    import onnx
    model_onnx = onnx.load(str(output))
    print("\n=== ONNX Model Info ===")
    print(f"Opset version: {model_onnx.opset_import[0].version}")
    print(f"Input shape: {model_onnx.graph.input[0].type.tensor_type.shape}")
    print(f"Output shape: {model_onnx.graph.output[0].type.tensor_type.shape}")
    
else:
    print("\n✗ best.pt bulunamadı!")

# 9. ÖZET
print("\n" + "=" * 60)
print("TRAİNİNG TAMAMLANDI!")
print("=" * 60)
print(f"\nSONUÇLAR:")
print(f"  Best Weights: {best_pt}")
print(f"  ONNX Model: {script_dir / 'sonar_model.onnx'}")
print(f"  Training Metrics: {script_dir / 'runs' / 'sonar_model' / 'results.csv'}")
print(f"  Training Plots: {script_dir / 'runs' / 'sonar_model'}")
print(f"\nSIRADAKI ADIMLAR:")
print(f"  1. Eğitim sonuçlarını kontrol et (results.csv, confusion_matrix.png)")
print(f"  2. sonar_model.onnx'i uygulama dizinine kopyala")
print(f"     (build/release/sonar_model.onnx)")
print(f"  3. Uygulamayı çalıştır ve test et!")
print(f"\nÖNEMLİ:")
print(f"  - Confidence threshold: 0.25-0.4 arası test et")
print(f"  - IOU threshold: 0.4-0.6 arası test et")
print(f"  - Sonar görüntülerinde objeler küçükse threshold'ları düşür")
print("=" * 60)
