import cv2
import numpy as np
import argparse

def expand2square(img, background_color=(127.5, 127.5, 127.5)):
    h, w = img.shape[:2]
    size = max(h, w)
    # 创建一个 size x size 的背景
    square_img = np.full((size, size, 3), background_color, dtype=np.uint8)

    # 将原图贴到中心
    x_offset = (size - w) // 2
    y_offset = (size - h) // 2
    square_img[y_offset:y_offset+h, x_offset:x_offset+w] = img
    return square_img

def preprocess_image(image_path, output_size, npy_path):
    # 1. 读图 (BGR)
    img = cv2.imread(image_path)
    if img is None:
        raise ValueError(f"Image at {image_path} could not be loaded")

    # 2. BGR -> RGB
    img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

    # 3. 扩展为正方形
    background_color = (127.5, 127.5, 127.5)  # 灰色背景
    square_img = expand2square(img, background_color)

    # 4. resize 到指定分辨率
    resized_img = cv2.resize(square_img, (output_size, output_size), interpolation=cv2.INTER_LINEAR)

    print(f"resized_img.shape: {resized_img.shape}")
    # 5. 保存 npy 文件
    np.save(npy_path, resized_img)

    return resized_img

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Preprocess image: expand to square and resize")
    parser.add_argument("--image_path", type=str, required=True, help="输入图像路径")
    parser.add_argument("--output_size", type=int, default=1024, help="输出分辨率 (默认: 1024)")
    parser.add_argument("--npy_path", type=str, required=True, help="保存 npy 文件路径")
    parser.add_argument("--save_png", type=str, default=None, help="可选，保存处理后的 PNG 文件路径")

    args = parser.parse_args()

    out = preprocess_image(args.image_path, args.output_size, args.npy_path)

    if args.save_png:
        cv2.imwrite(args.save_png, cv2.cvtColor(out, cv2.COLOR_RGB2BGR))
