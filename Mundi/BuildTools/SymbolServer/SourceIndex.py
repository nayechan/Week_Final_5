#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Git 기반 소스 인덱싱 스트림 생성 스크립트

PDB 파일에 삽입할 Source Server 스트림을 생성합니다.
디버거가 PDB를 로드할 때 Git 저장소에서 소스 코드를 자동으로 가져올 수 있도록 합니다.
"""

import argparse
import os
import sys
from pathlib import Path


def normalize_path(path: str, repo_root: str) -> str:
    """
    절대 경로를 Git 저장소 기준 상대 경로로 변환

    Args:
        path: 소스 파일의 절대 경로
        repo_root: Git 저장소 루트 경로

    Returns:
        Git 저장소 기준 상대 경로 (Unix 스타일 슬래시)
        저장소 외부 파일인 경우 None 반환
    """
    try:
        path = Path(path).resolve()
        repo_root = Path(repo_root).resolve()

        rel_path = path.relative_to(repo_root)
        # Windows 경로를 Unix 스타일로 변환 (Git URL에서 사용)
        return str(rel_path).replace('\\', '/')
    except (ValueError, Exception):
        # 저장소 외부 파일 (서드파티 라이브러리 등)
        return None


def parse_git_remote_url(git_remote: str) -> str:
    """
    Git remote URL을 파싱하여 웹 브라우저용 URL 베이스 생성

    Args:
        git_remote: Git remote URL (https:// 또는 git@ 형식)

    Returns:
        웹 브라우저에서 접근 가능한 베이스 URL

    Examples:
        https://github.com/user/repo.git -> https://github.com/user/repo
        git@github.com:user/repo.git -> https://github.com/user/repo
    """
    git_remote = git_remote.strip()

    # .git 확장자 제거
    if git_remote.endswith('.git'):
        git_remote = git_remote[:-4]

    # SSH 형식을 HTTPS로 변환
    if git_remote.startswith('git@'):
        # git@github.com:user/repo -> github.com:user/repo
        git_remote = git_remote[4:]
        # github.com:user/repo -> github.com/user/repo
        git_remote = git_remote.replace(':', '/', 1)
        # https:// 추가
        git_remote = 'https://' + git_remote

    return git_remote


def generate_srcsrv_stream(
    source_list_file: str,
    git_repo: str,
    git_commit: str,
    git_remote: str,
    output_file: str
) -> int:
    """
    Source Server 스트림 생성

    Args:
        source_list_file: srctool.exe가 생성한 소스 파일 목록
        git_repo: Git 저장소 루트 경로
        git_commit: 현재 빌드의 Git 커밋 해시
        git_remote: Git remote URL
        output_file: 출력할 스트림 파일 경로

    Returns:
        인덱싱된 소스 파일 개수

    Source Server 스트림 형식:
    ┌─────────────────────────────────────────────────────────────┐
    │ SRCSRV: ini ------------------------------------------------│
    │ VERSION=2                                                    │
    │ SRCSRV: variables ------------------------------------------│
    │ GIT_REPO=https://github.com/user/repo                       │
    │ GIT_COMMIT=abc123def456...                                  │
    │ SRCSRVTRG=%GIT_REPO%/raw/%GIT_COMMIT%/%var2%                │
    │ SRCSRVCMD=                                                   │
    │ SRCSRV: source files ---------------------------------------│
    │ C:\path\to\file.cpp*relative/path/file.cpp                  │
    │ C:\path\to\file.h*relative/path/file.h                      │
    │ ...                                                          │
    │ SRCSRV: end ------------------------------------------------│
    └─────────────────────────────────────────────────────────────┘
    """

    # 소스 파일 목록 읽기
    if not os.path.exists(source_list_file):
        print(f"[ERROR] Source list file not found: {source_list_file}", file=sys.stderr)
        return 0

    with open(source_list_file, 'r', encoding='utf-8') as f:
        source_files = [line.strip() for line in f if line.strip()]

    if not source_files:
        print("[WARNING] Source list file is empty", file=sys.stderr)
        return 0

    print(f"[SourceIndex] Processing {len(source_files)} source files...")

    # Git remote URL 파싱
    git_url_base = parse_git_remote_url(git_remote)
    print(f"[SourceIndex] Git repository: {git_url_base}")
    print(f"[SourceIndex] Git commit: {git_commit}")

    # Source Server 스트림 생성
    stream_lines = []

    # ===== 헤더 =====
    stream_lines.append("SRCSRV: ini ------------------------------------------------")
    stream_lines.append("VERSION=2")

    # ===== 변수 정의 =====
    stream_lines.append("SRCSRV: variables ------------------------------------------")
    stream_lines.append(f"GIT_REPO={git_url_base}")
    stream_lines.append(f"GIT_COMMIT={git_commit}")

    # GitHub/GitLab raw file URL 템플릿
    # %var2% = 상대 경로
    stream_lines.append("SRCSRVTRG=%GIT_REPO%/raw/%GIT_COMMIT%/%var2%")
    stream_lines.append("SRCSRVCMD=")

    # ===== 소스 파일 매핑 =====
    stream_lines.append("SRCSRV: source files ---------------------------------------")

    indexed_count = 0
    skipped_count = 0

    for src_path in source_files:
        rel_path = normalize_path(src_path, git_repo)

        if rel_path:
            # 형식: 절대경로*상대경로
            stream_lines.append(f"{src_path}*{rel_path}")
            indexed_count += 1
        else:
            # 저장소 외부 파일 (서드파티 등)
            skipped_count += 1

    # ===== 푸터 =====
    stream_lines.append("SRCSRV: end ------------------------------------------------")

    # 파일 저장 (CRLF 줄바꿈 사용 - Windows 표준)
    try:
        with open(output_file, 'w', encoding='utf-8', newline='\r\n') as f:
            f.write('\n'.join(stream_lines))

        print(f"[SourceIndex] Indexed {indexed_count} source files")
        if skipped_count > 0:
            print(f"[SourceIndex] Skipped {skipped_count} external files (third-party libraries)")

        return indexed_count

    except Exception as e:
        print(f"[ERROR] Failed to write stream file: {e}", file=sys.stderr)
        return 0


def main():
    """메인 엔트리포인트"""
    parser = argparse.ArgumentParser(
        description='Generate Source Server stream for Git-based source indexing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Example:
    python SourceIndex.py \\
        --source-list sources.txt \\
        --git-repo C:\\Projects\\MyGame \\
        --git-commit abc123def456 \\
        --git-remote https://github.com/user/mygame.git \\
        --output srcsrv.txt
        """
    )

    parser.add_argument(
        '--source-list',
        required=True,
        help='Source file list extracted by srctool.exe'
    )
    parser.add_argument(
        '--git-repo',
        required=True,
        help='Git repository root path (absolute path)'
    )
    parser.add_argument(
        '--git-commit',
        required=True,
        help='Git commit hash (SHA-1)'
    )
    parser.add_argument(
        '--git-remote',
        required=True,
        help='Git remote URL (https:// or git@)'
    )
    parser.add_argument(
        '--output',
        required=True,
        help='Output Source Server stream file path'
    )

    args = parser.parse_args()

    # 입력 검증
    if not os.path.exists(args.source_list):
        print(f"[ERROR] Source list file not found: {args.source_list}", file=sys.stderr)
        sys.exit(1)

    if not os.path.isdir(args.git_repo):
        print(f"[ERROR] Git repository directory not found: {args.git_repo}", file=sys.stderr)
        sys.exit(1)

    # 스트림 생성
    count = generate_srcsrv_stream(
        args.source_list,
        args.git_repo,
        args.git_commit,
        args.git_remote,
        args.output
    )

    if count == 0:
        print("[ERROR] No source files were indexed", file=sys.stderr)
        sys.exit(1)

    print(f"[SUCCESS] Source Server stream written to: {args.output}")
    sys.exit(0)


if __name__ == '__main__':
    main()
