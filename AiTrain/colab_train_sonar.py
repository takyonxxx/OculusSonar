# SONAR YOLO TRAINING - GOOGLE COLAB
# ===================================
# Bu kodu Google Colab'da çalıştır (GPU ile hızlı training)
# ONNX export: Opset 21 (ONNX Runtime uyumlu)

# 1. YOLOv8 KUR
!pip install ultralytics -q

# 2. DATASET YÜKLE
# sonar_model.zip'i Colab'a yükle (Files → Upload)
!unzip -q sonar_model.zip -d /content/dataset

# 3. DATASET YAPISINI OLUŞTUR
import os
import shutil
from pathlib import Path
import yaml

base_dir = Path("/content/dataset")
yolo_dir = Path("/content/yolo_dataset")

# Dizinleri oluştur
yolo_dir.mkdir(exist_ok=True)
(yolo_dir / "train" / "images").mkdir(parents=True, exist_ok=True)
(yolo_dir / "train" / "labels").mkdir(parents=True, exist_ok=True)
(yolo_dir / "val" / "images").mkdir(parents=True, exist_ok=True)
(yolo_dir / "val" / "labels").mkdir(parents=True, exist_ok=True)

# Görüntüleri kopyala ve split yap
images = sorted(list((base_dir / "obj_Train_data").glob("*.png")))
print(f"Toplam: {len(images)} görüntü")

split_idx = int(len(images) * 0.8)
train_images = images[:split_idx]
val_images = images[split_idx:]

print(f"Train: {len(train_images)}")
print(f"Val: {len(val_images)}")

# Train dosyalarını kopyala
for img in train_images:
    shutil.copy(img, yolo_dir / "train" / "images" / img.name)
    label = img.with_suffix('.txt')
    if label.exists():
        shutil.copy(label, yolo_dir / "train" / "labels" / label.name)

# Val dosyalarını kopyala
for img in val_images:
    shutil.copy(img, yolo_dir / "val" / "images" / img.name)
    label = img.with_suffix('.txt')
    if label.exists():
        shutil.copy(label, yolo_dir / "val" / "labels" / label.name)

# 4. DATA.YAML OLUŞTUR
data_yaml = {
    'path': str(yolo_dir.absolute()),
    'train': 'train/images',
    'val': 'val/images',
    'nc': 1,
    'names': ['Object']
}

with open(yolo_dir / "data.yaml", 'w') as f:
    yaml.dump(data_yaml, f)

print("Dataset hazır!")

# 5. TRAİNİNG BAŞLAT
from ultralytics import YOLO

model = YOLO('yolov8n.pt')

results = model.train(
    data=str(yolo_dir / "data.yaml"),
    epochs=100,
    imgsz=640,
    batch=16,
    name='sonar',
    patience=20,
    device=0  # GPU
)

# 6. ONNX EXPORT
model = YOLO('/content/runs/detect/sonar/weights/best.pt')
onnx_path = model.export(format='onnx', imgsz=640, simplify=True, opset=21)

print(f"\n✓ ONNX: {onnx_path}")
print("\nDownload linki:")
print("Files → runs/detect/sonar/weights/best.onnx")
print("→ Sağ tık → Download")
print("\nSONRA:")
print("1. best.onnx'i indir")
print("2. sonar_model.onnx olarak yeniden adlandır")
print("3. build/release/sonar_model.onnx olarak kopyala")
