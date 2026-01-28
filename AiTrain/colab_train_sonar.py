# SONAR YOLO TRAINING - GOOGLE COLAB
# ===================================
# Bu kodu Google Colab'da çalıştır (GPU ile hızlı training)
# ONNX export: Opset 21 (ONNX Runtime uyumlu)
# Runtime → Change runtime type → GPU seçmeyi unutma!

# 1. YOLOv8 KUR
!pip install ultralytics -q

# 2. GPU KONTROLÜ
import torch

print("=" * 60)
print("GPU/CUDA KONTROLÜ")
print("=" * 60)

if torch.cuda.is_available():
    device = 0
    gpu_name = torch.cuda.get_device_name(0)
    gpu_memory = torch.cuda.get_device_properties(0).total_memory / (1024**3)
    print(f"✓ CUDA kullanılabilir!")
    print(f"  GPU: {gpu_name}")
    print(f"  VRAM: {gpu_memory:.1f} GB")
    print(f"  CUDA Version: {torch.version.cuda}")
    
    # VRAM'a göre batch size
    if gpu_memory >= 15:  # T4, V100, A100
        batch_size = 32
    elif gpu_memory >= 10:
        batch_size = 24
    elif gpu_memory >= 8:
        batch_size = 16
    else:
        batch_size = 12
    print(f"  Otomatik batch size: {batch_size}")
else:
    device = 'cpu'
    batch_size = 8
    print("✗ GPU bulunamadı!")
    print("  Runtime → Change runtime type → GPU seç!")

# 3. DATASET YÜKLE
# sonar_model.zip'i Colab'a yükle (Files → Upload)
!unzip -q sonar_model.zip -d /content/dataset

# 4. DATASET YAPISINI OLUŞTUR
import os
import shutil
from pathlib import Path
import yaml

base_dir = Path("/content/dataset")
yolo_dir = Path("/content/yolo_dataset")

yolo_dir.mkdir(exist_ok=True)
(yolo_dir / "train" / "images").mkdir(parents=True, exist_ok=True)
(yolo_dir / "train" / "labels").mkdir(parents=True, exist_ok=True)
(yolo_dir / "val" / "images").mkdir(parents=True, exist_ok=True)
(yolo_dir / "val" / "labels").mkdir(parents=True, exist_ok=True)

images = sorted(list((base_dir / "obj_Train_data").glob("*.png")))
print(f"\nToplam: {len(images)} görüntü")

split_idx = int(len(images) * 0.8)
train_images = images[:split_idx]
val_images = images[split_idx:]

print(f"Train: {len(train_images)}")
print(f"Val: {len(val_images)}")

for img in train_images:
    shutil.copy(img, yolo_dir / "train" / "images" / img.name)
    label = img.with_suffix('.txt')
    if label.exists():
        shutil.copy(label, yolo_dir / "train" / "labels" / label.name)

for img in val_images:
    shutil.copy(img, yolo_dir / "val" / "images" / img.name)
    label = img.with_suffix('.txt')
    if label.exists():
        shutil.copy(label, yolo_dir / "val" / "labels" / label.name)

# 5. DATA.YAML OLUŞTUR
data_yaml = {
    'path': str(yolo_dir.absolute()),
    'train': 'train/images',
    'val': 'val/images',
    'nc': 1,
    'names': ['Object']
}

with open(yolo_dir / "data.yaml", 'w') as f:
    yaml.dump(data_yaml, f)

print("✓ Dataset hazır!")

# 6. TRAİNİNG BAŞLAT
print("\n" + "=" * 60)
print("MODEL EĞİTİMİ BAŞLIYOR")
print("=" * 60)

device_str = f"GPU ({gpu_name})" if device == 0 else "CPU"
print(f"Cihaz: {device_str}")
print(f"Batch size: {batch_size}")
print(f"Tahmini süre: 10-20 dakika (Colab GPU)\n")

from ultralytics import YOLO

model = YOLO('yolov8n.pt')

results = model.train(
    data=str(yolo_dir / "data.yaml"),
    epochs=100,
    imgsz=640,
    batch=batch_size,
    name='sonar',
    patience=20,
    device=device,
    workers=4,
    
    # Optimization
    optimizer='Adam',
    lr0=0.001,
    lrf=0.01,
    
    # Augmentation
    hsv_h=0.015,
    hsv_s=0.3,
    hsv_v=0.2,
    degrees=10.0,
    translate=0.1,
    scale=0.3,
    flipud=0.5,
    fliplr=0.5,
    mosaic=1.0,
    mixup=0.1,
    
    # GPU optimizations
    amp=(device == 0),  # Automatic Mixed Precision
    verbose=True,
    plots=True,
)

# 7. ONNX EXPORT
print("\n" + "=" * 60)
print("ONNX EXPORT")
print("=" * 60)

model = YOLO('/content/runs/detect/sonar/weights/best.pt')
onnx_path = model.export(format='onnx', imgsz=640, simplify=True, opset=21)

final_onnx = Path("/content/sonar_model.onnx")
shutil.copy(onnx_path, final_onnx)

print(f"✓ ONNX: {final_onnx}")

# 8. TEMİZLİK
print("\n" + "=" * 60)
print("TEMİZLİK YAPILIYOR")
print("=" * 60)

cleanup_dirs = [
    Path("/content/dataset"),
    Path("/content/yolo_dataset"),
    Path("/content/runs"),
]

cleanup_files = [
    Path("/content/yolov8n.pt"),
]

for dir_path in cleanup_dirs:
    if dir_path.exists():
        try:
            shutil.rmtree(dir_path)
            print(f"✓ Silindi: {dir_path.name}/")
        except Exception as e:
            print(f"✗ Silinemedi: {dir_path.name}/ - {e}")

for file_path in cleanup_files:
    if file_path.exists():
        try:
            file_path.unlink()
            print(f"✓ Silindi: {file_path.name}")
        except Exception as e:
            print(f"✗ Silinemedi: {file_path.name} - {e}")

# 9. ÖZET
print("\n" + "=" * 60)
print("TRAİNİNG TAMAMLANDI!")
print("=" * 60)

print(f"\nKALAN DOSYALAR:")
print(f"  ✓ /content/sonar_model.onnx  - Eğitilmiş model")
print(f"  ✓ /content/sonar_model.zip   - Orijinal dataset")

print(f"\nİNDİRME:")
print(f"  Files panelinde → sonar_model.onnx → Sağ tık → Download")

print(f"\nSONRAKİ ADIMLAR:")
print(f"  1. sonar_model.onnx'i indir")
print(f"  2. build/release/sonar_model.onnx olarak kopyala")
print(f"  3. Uygulamayı çalıştır ve test et!")
print("=" * 60)
