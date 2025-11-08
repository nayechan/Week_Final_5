#!/usr/bin/env python3
"""
모든 헤더 파일에 .generated.h include를 자동으로 추가하는 스크립트
"""

import re
from pathlib import Path
import sys


def update_header_file(header_path: Path) -> bool:
    """
    헤더 파일을 읽어서 .generated.h include를 추가하고
    DECLARE_CLASS와 DECLARE_DUPLICATE를 제거합니다.

    Returns:
        True if file was modified, False otherwise
    """
    content = header_path.read_text(encoding='utf-8')
    original_content = content

    # GENERATED_REFLECTION_BODY() 체크
    if 'GENERATED_REFLECTION_BODY()' not in content:
        return False

    # 클래스 이름 추출
    class_match = re.search(r'class\s+(\w+)\s*:\s*public\s+(\w+)', content)
    if not class_match:
        return False

    class_name = class_match.group(1)
    generated_include = f'#include "{class_name}.generated.h"'

    # 기존 .generated.h include 제거 (잘못된 위치에 있을 수 있음)
    content = re.sub(
        rf'^#include\s+"{class_name}\.generated\.h"\s*$',
        '',
        content,
        flags=re.MULTILINE
    )

    # 마지막 #include 다음에 .generated.h include 추가
    lines = content.split('\n')
    new_lines = []
    last_include_index = -1

    # 마지막 #include의 위치를 찾음
    for i, line in enumerate(lines):
        if line.strip().startswith('#include'):
            last_include_index = i

    # 마지막 include 다음에 .generated.h 추가
    if last_include_index >= 0:
        for i, line in enumerate(lines):
            new_lines.append(line)
            if i == last_include_index:
                new_lines.append(generated_include)
    else:
        # include가 없었다면 #pragma once 다음에 추가
        for i, line in enumerate(lines):
            new_lines.append(line)
            if '#pragma once' in line:
                new_lines.append(generated_include)

    content = '\n'.join(new_lines)

    # DECLARE_CLASS 제거
    content = re.sub(
        r'^\s*DECLARE_CLASS\s*\([^)]+\)\s*$',
        '',
        content,
        flags=re.MULTILINE
    )

    # DECLARE_DUPLICATE 제거
    content = re.sub(
        r'^\s*DECLARE_DUPLICATE\s*\([^)]+\)\s*$',
        '',
        content,
        flags=re.MULTILINE
    )

    # 빈 줄 정리 (3개 이상의 연속된 빈 줄을 2개로)
    content = re.sub(r'\n\n\n+', '\n\n', content)

    # 파일이 실제로 변경되었는지 확인
    if content != original_content:
        header_path.write_text(content, encoding='utf-8')
        print(f"  ✅ {header_path.name} - Updated")
        return True

    return False


def update_cpp_file(cpp_path: Path) -> bool:
    """
    .cpp 파일에서 IMPLEMENT_CLASS를 제거합니다.

    Returns:
        True if file was modified, False otherwise
    """
    if not cpp_path.exists():
        return False

    content = cpp_path.read_text(encoding='utf-8')
    original_content = content

    # IMPLEMENT_CLASS 제거
    content = re.sub(
        r'^\s*IMPLEMENT_CLASS\s*\([^)]+\)\s*$',
        '// IMPLEMENT_CLASS is now auto-generated in .generated.cpp',
        content,
        flags=re.MULTILINE
    )

    if content != original_content:
        cpp_path.write_text(content, encoding='utf-8')
        print(f"  ✅ {cpp_path.name} - Removed IMPLEMENT_CLASS")
        return True

    return False


def main():
    if len(sys.argv) < 2:
        print("Usage: python update_headers.py <source-dir>")
        print("Example: python update_headers.py Source/Runtime")
        sys.exit(1)

    source_dir = Path(sys.argv[1])

    if not source_dir.exists():
        print(f"❌ Error: Directory not found: {source_dir}")
        sys.exit(1)

    print("=" * 60)
    print("🔧 Updating Header Files - Adding .generated.h includes")
    print("=" * 60)
    print(f"📁 Source: {source_dir}\n")

    headers_updated = 0
    cpps_updated = 0

    # 모든 .h 파일 찾기
    for header_path in source_dir.rglob("*.h"):
        if update_header_file(header_path):
            headers_updated += 1

            # 대응하는 .cpp 파일도 업데이트
            cpp_path = header_path.with_suffix('.cpp')
            if update_cpp_file(cpp_path):
                cpps_updated += 1

    print()
    print("=" * 60)
    print(f"✅ Update complete!")
    print(f"   Headers updated: {headers_updated}")
    print(f"   CPP files updated: {cpps_updated}")
    print("=" * 60)


if __name__ == '__main__':
    main()
