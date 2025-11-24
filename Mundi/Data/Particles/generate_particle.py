import numpy as np
from PIL import Image
import os

def generate_gaussian_particle(size=256, sigma=0.3):
    """
    가우시안 함수를 사용하여 원형 파티클 텍스처 생성

    Args:
        size: 이미지 크기 (정사각형)
        sigma: 가우시안 표준편차 (0.0~1.0, 작을수록 중심에 집중)
    """
    # 좌표 생성 (-1 ~ 1 범위)
    x = np.linspace(-1, 1, size)
    y = np.linspace(-1, 1, size)
    X, Y = np.meshgrid(x, y)

    # 중심으로부터의 거리
    distance = np.sqrt(X**2 + Y**2)

    # 가우시안 함수 적용
    gaussian = np.exp(-(distance**2) / (2 * sigma**2))

    # 0~255 범위로 변환
    intensity = (gaussian * 255).astype(np.uint8)

    # RGBA 이미지 생성 (흰색, 알파는 가우시안)
    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, 0] = 255  # R
    rgba[:, :, 1] = 255  # G
    rgba[:, :, 2] = 255  # B
    rgba[:, :, 3] = intensity  # A (가우시안)

    return Image.fromarray(rgba, 'RGBA')

def generate_soft_circle(size=256, falloff=2.0):
    """
    부드러운 원형 파티클 (가장자리가 더 선명)

    Args:
        size: 이미지 크기
        falloff: 감쇠 정도 (클수록 가장자리가 더 급격히 감소)
    """
    x = np.linspace(-1, 1, size)
    y = np.linspace(-1, 1, size)
    X, Y = np.meshgrid(x, y)

    distance = np.sqrt(X**2 + Y**2)

    # 1에서 거리를 빼고, 0 이하는 0으로
    intensity = np.maximum(0, 1 - distance)
    intensity = intensity ** falloff  # 감쇠 적용

    intensity = (intensity * 255).astype(np.uint8)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, 0] = 255
    rgba[:, :, 1] = 255
    rgba[:, :, 2] = 255
    rgba[:, :, 3] = intensity

    return Image.fromarray(rgba, 'RGBA')

def generate_smoke_particle(size=256):
    """
    연기/불꽃용 파티클 (가우시안 + 노이즈)
    """
    x = np.linspace(-1, 1, size)
    y = np.linspace(-1, 1, size)
    X, Y = np.meshgrid(x, y)

    distance = np.sqrt(X**2 + Y**2)

    # 기본 가우시안
    sigma = 0.4
    gaussian = np.exp(-(distance**2) / (2 * sigma**2))

    # 약간의 노이즈 추가
    noise = np.random.random((size, size)) * 0.1
    result = gaussian + noise * gaussian
    result = np.clip(result, 0, 1)

    intensity = (result * 255).astype(np.uint8)

    rgba = np.zeros((size, size, 4), dtype=np.uint8)
    rgba[:, :, 0] = 255
    rgba[:, :, 1] = 255
    rgba[:, :, 2] = 255
    rgba[:, :, 3] = intensity

    return Image.fromarray(rgba, 'RGBA')

if __name__ == "__main__":
    # 출력 디렉토리 확인
    output_dir = os.path.dirname(os.path.abspath(__file__))

    # 1. 기본 가우시안 파티클
    img = generate_gaussian_particle(256, sigma=0.3)
    img.save(os.path.join(output_dir, "Particle_Gaussian.png"))
    print("Generated: Particle_Gaussian.png")

    # 2. 부드러운 원형 파티클
    img = generate_soft_circle(256, falloff=1.5)
    img.save(os.path.join(output_dir, "Particle_SoftCircle.png"))
    print("Generated: Particle_SoftCircle.png")

    # 3. 연기용 파티클
    img = generate_smoke_particle(256)
    img.save(os.path.join(output_dir, "Smoke.png"))
    print("Generated: Smoke.png")

    # 4. 더 집중된 파티클 (불꽃용)
    img = generate_gaussian_particle(256, sigma=0.15)
    img.save(os.path.join(output_dir, "Particle_Spark.png"))
    print("Generated: Particle_Spark.png")

    print("\nAll particle textures generated successfully!")
