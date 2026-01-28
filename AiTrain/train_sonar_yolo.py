#!/usr/bin/env python3
"""
SONAR YOLO TRAINING - Windows/Linux
ONNX export with opset 20 (max supported by PyTorch)
CUDA/GPU support with automatic detection
"""

import os
import shutil
from pathlib import Path
import yaml
import zipfile


def main():
    print("=" * 60)
    print("SONAR YOLO TRAINING - CUDA/GPU SUPPORTED")
    print("=" * 60)

    # 1. BASE DIRECTORY
    script_dir = Path(__file__).parent
    print(f"\nScript dizini: {script_dir}")

    # 2. CUDA/GPU KONTROLÜ
    print("\n" + "=" * 60)
    print("GPU/CUDA KONTROLÜ")
    print("=" * 60)

    import torch

    if torch.cuda.is_available():
        device = 0
        gpu_name = torch.cuda.get_device_name(0)
        gpu_memory = torch.cuda.get_device_properties(0).total_memory / (1024**3)
        print(f"✓ CUDA kullanılabilir!")
        print(f"  GPU: {gpu_name}")
        print(f"  VRAM: {gpu_memory:.1f} GB")
        print(f"  CUDA Version: {torch.version.cuda}")
        
        if gpu_memory >= 8:
            batch_size = 16
        elif gpu_memory >= 6:
            batch_size = 12
        elif gpu_memory >= 4:
            batch_size = 8
        else:
            batch_size = 4
        print(f"  Otomatik batch size: {batch_size}")
    else:
        device = 'cpu'
        batch_size = 8
        gpu_name = None
        print("✗ CUDA bulunamadı, CPU kullanılacak")

    # 3. ZIP DOSYASINI AÇ
    zip_file = script_dir / "sonar_model.zip"
    if not zip_file.exists():
        print(f"\n✗ HATA: {zip_file} bulunamadı!")
        print(f"\nsonar_model.zip'i bu klasöre kopyala:")
        print(f"  {script_dir}")
        return

    print(f"\n✓ ZIP bulundu: {zip_file}")

    extract_dir = script_dir / "dataset"
    if extract_dir.exists():
        shutil.rmtree(extract_dir)
    extract_dir.mkdir()

    print("\nZIP açılıyor...")
    with zipfile.ZipFile(zip_file, 'r') as zip_ref:
        zip_ref.extractall(extract_dir)
    print(f"✓ Dataset açıldı: {extract_dir}")

    # 4. YOLO DATASET YAPISI OLUŞTUR
    yolo_dir = script_dir / "yolo_dataset"
    if yolo_dir.exists():
        shutil.rmtree(yolo_dir)

    (yolo_dir / "train" / "images").mkdir(parents=True)
    (yolo_dir / "train" / "labels").mkdir(parents=True)
    (yolo_dir / "val" / "images").mkdir(parents=True)
    (yolo_dir / "val" / "labels").mkdir(parents=True)

    # 5. GÖRÜNTÜLERİ KOPYALA
    print("\nGörüntüler kopyalanıyor...")

    images = sorted(list((extract_dir / "obj_Train_data").glob("*.png")))
    print(f"Toplam: {len(images)} görüntü")

    if len(images) == 0:
        print("✗ HATA: Görüntü bulunamadı!")
        return

    split_idx = int(len(images) * 0.8)
    train_images = images[:split_idx]
    val_images = images[split_idx:]

    print(f"Train: {len(train_images)} görüntü")
    print(f"Val: {len(val_images)} görüntü")

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

    print("✓ Dosyalar kopyalandı")

    # 6. DATA.YAML
    print("\ndata.yaml oluşturuluyor...")

    data_yaml = {
        'path': str(yolo_dir.absolute()),
        'train': 'train/images',
        'val': 'val/images',
        'nc': 1,
        'names': ['Object']
    }

    with open(yolo_dir / "data.yaml", 'w') as f:
        yaml.dump(data_yaml, f)

    print(f"✓ data.yaml: {yolo_dir / 'data.yaml'}")

    # 7. ULTRALYTICS
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

    # 8. ESKİ RUNS KLASÖRÜNÜ TEMİZLE
    runs_dir = script_dir / "runs"
    if runs_dir.exists():
        shutil.rmtree(runs_dir)
        print("✓ Eski runs klasörü temizlendi")

    # 9. TRAINING
    print("\n" + "=" * 60)
    print("MODEL EĞİTİMİ BAŞLIYOR")
    print("=" * 60)

    device_str = f"GPU ({gpu_name})" if device == 0 else "CPU"
    print(f"\nCihaz: {device_str}")
    print(f"Batch size: {batch_size}")

    if device == 0:
        print("Tahmini süre: 10-30 dakika (GPU)")
    else:
        print("Tahmini süre: 1-3 saat (CPU)")

    print("\nEğitim başlıyor...\n")

    model = YOLO('yolov8n.pt')

    workers = 8 if device == 0 else 4

    results = model.train(
        data=str(yolo_dir / "data.yaml"),
        epochs=150,
        imgsz=640,
        batch=batch_size,
        name='sonar_model',
        project=str(script_dir / "runs"),
        patience=25,
        save=True,
        device=device,
        workers=workers,
        exist_ok=True,
        
        optimizer='Adam',
        lr0=0.001,
        lrf=0.01,
        momentum=0.937,
        weight_decay=0.0005,
        
        hsv_h=0.015,
        hsv_s=0.3,
        hsv_v=0.2,
        degrees=10.0,
        translate=0.1,
        scale=0.3,
        shear=0.0,
        perspective=0.0,
        flipud=0.5,
        fliplr=0.5,
        mosaic=1.0,
        mixup=0.1,
        
        verbose=True,
        plots=True,
        save_period=10,
        amp=(device == 0),
    )

    # 10. ONNX EXPORT - OPSET 17 (en uyumlu)
    print("\n" + "=" * 60)
    print("ONNX EXPORT - OPSET 17")
    print("=" * 60)

    # best.pt'yi bul
    best_pt = None
    for pt_file in (script_dir / "runs").rglob("best.pt"):
        best_pt = pt_file
        break
    
    onnx_exported = False

    if best_pt and best_pt.exists():
        print(f"\n✓ Best model bulundu: {best_pt}")
        
        model = YOLO(str(best_pt))
        
        # OPSET 17 - PyTorch ve ONNX Runtime ile en uyumlu versiyon
        onnx_path = model.export(
            format='onnx',
            imgsz=640,
            simplify=True,
            opset=17,        # 21 yerine 17 (en uyumlu)
            dynamic=False
        )
        
        print(f"✓ ONNX export: {onnx_path}")
        
        output = script_dir / "sonar_model.onnx"
        shutil.copy(onnx_path, output)
        print(f"✓ Kaydedildi: {output}")
        onnx_exported = True
        
        try:
            import onnx
            model_onnx = onnx.load(str(output))
            print(f"\n=== ONNX Model Info ===")
            print(f"Opset version: {model_onnx.opset_import[0].version}")
        except:
            pass
    else:
        print("\n✗ best.pt bulunamadı!")

    # 11. TEMİZLİK
    print("\n" + "=" * 60)
    print("TEMİZLİK YAPILIYOR")
    print("=" * 60)

    cleanup_dirs = [
        script_dir / "dataset",
        script_dir / "yolo_dataset",
        script_dir / "runs",
    ]

    cleanup_files = [
        script_dir / "yolov8n.pt",
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

    # 12. ÖZET
    print("\n" + "=" * 60)
    print("TRAİNİNG TAMAMLANDI!")
    print("=" * 60)

    if onnx_exported:
        print(f"\n✓ sonar_model.onnx hazır!")
        print(f"\nSONRAKİ ADIMLAR:")
        print(f"  1. sonar_model.onnx'i uygulama dizinine kopyala")
        print(f"  2. Uygulamayı çalıştır ve test et!")
    else:
        print(f"\n✗ ONNX export başarısız!")

    print("=" * 60)


if __name__ == '__main__':
    main()
