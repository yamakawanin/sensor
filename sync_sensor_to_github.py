#!/usr/bin/env python3
"""Synchronize the whole sensor repository to GitHub.

Features:
1) Auto-generate/update root README project table based on first-level folders.
2) Git add/commit/push in one command.
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import re
import subprocess
import sys
from dataclasses import dataclass
from typing import Iterable


TABLE_START = "<!-- PROJECT_TABLE_START -->"
TABLE_END = "<!-- PROJECT_TABLE_END -->"

DEFAULT_README_TEMPLATE = f"""# sensor

这个仓库用于存放多个独立传感器相关项目。根目录下每个一级文件夹都视为一个独立项目。

## 项目列表（自动生成）

运行 `python sync_sensor_to_github.py` 时会自动刷新下表。

{TABLE_START}
| 项目名 | 简介 | 路径 |
|---|---|---|
| _暂无项目_ | - | - |
{TABLE_END}
"""


@dataclass
class ProjectInfo:
    name: str
    summary: str
    rel_path: str


def run_cmd(cmd: list[str], cwd: pathlib.Path, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        cmd,
        cwd=str(cwd),
        text=True,
        capture_output=True,
        check=False,
    )
    if check and result.returncode != 0:
        raise RuntimeError(
            f"Command failed: {' '.join(cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result


def is_git_repo(repo_root: pathlib.Path) -> bool:
    result = run_cmd(["git", "rev-parse", "--is-inside-work-tree"], cwd=repo_root, check=False)
    return result.returncode == 0 and result.stdout.strip() == "true"


def first_level_project_dirs(repo_root: pathlib.Path) -> list[pathlib.Path]:
    ignored = {".git", ".venv", "venv", "__pycache__", ".idea", ".vscode", "node_modules"}
    dirs: list[pathlib.Path] = []
    for p in sorted(repo_root.iterdir()):
        if not p.is_dir():
            continue
        if p.name.startswith("."):
            continue
        if p.name in ignored:
            continue
        dirs.append(p)
    return dirs


def extract_summary(project_dir: pathlib.Path) -> str:
    readme = project_dir / "README.md"
    if not readme.exists():
        return "(暂无 README)"

    try:
        lines = readme.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError:
        lines = readme.read_text(encoding="utf-8", errors="ignore").splitlines()

    for line in lines:
        s = line.strip()
        if not s:
            continue
        if s.startswith("#"):
            continue
        if s.startswith("```"):
            continue
        s = re.sub(r"\s+", " ", s)
        return s[:120]

    return "(README 暂无简介内容)"


def collect_projects(repo_root: pathlib.Path) -> list[ProjectInfo]:
    projects: list[ProjectInfo] = []
    for d in first_level_project_dirs(repo_root):
        projects.append(
            ProjectInfo(
                name=d.name,
                summary=extract_summary(d),
                rel_path=d.name,
            )
        )
    return projects


def md_escape(text: str) -> str:
    return text.replace("|", "\\|")


def build_table(projects: Iterable[ProjectInfo]) -> str:
    rows = [
        "| 项目名 | 简介 | 路径 |",
        "|---|---|---|",
    ]
    project_list = list(projects)
    if not project_list:
        rows.append("| _暂无项目_ | - | - |")
    else:
        for p in project_list:
            rows.append(f"| {md_escape(p.name)} | {md_escape(p.summary)} | `{md_escape(p.rel_path)}` |")
    return "\n".join(rows)


def ensure_root_readme(repo_root: pathlib.Path) -> pathlib.Path:
    readme_path = repo_root / "README.md"
    if not readme_path.exists():
        readme_path.write_text(DEFAULT_README_TEMPLATE, encoding="utf-8")
    return readme_path


def update_root_readme(repo_root: pathlib.Path) -> bool:
    readme_path = ensure_root_readme(repo_root)
    original = readme_path.read_text(encoding="utf-8")
    table = build_table(collect_projects(repo_root))
    replacement = f"{TABLE_START}\n{table}\n{TABLE_END}"

    if TABLE_START in original and TABLE_END in original:
        new_content = re.sub(
            rf"{re.escape(TABLE_START)}[\s\S]*?{re.escape(TABLE_END)}",
            replacement,
            original,
            count=1,
        )
    else:
        # If user manually changed README and removed markers, append managed section.
        new_content = original.rstrip() + "\n\n## 项目列表（自动生成）\n\n" + replacement + "\n"

    if new_content != original:
        readme_path.write_text(new_content, encoding="utf-8")
        return True
    return False


def get_current_branch(repo_root: pathlib.Path) -> str:
    result = run_cmd(["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=repo_root)
    branch = result.stdout.strip()
    if not branch:
        raise RuntimeError("Cannot determine current git branch.")
    return branch


def get_git_toplevel(repo_root: pathlib.Path) -> pathlib.Path:
    result = run_cmd(["git", "rev-parse", "--show-toplevel"], cwd=repo_root)
    return pathlib.Path(result.stdout.strip()).resolve()


def ensure_no_git_index_lock(git_toplevel: pathlib.Path) -> None:
    lock_file = git_toplevel / ".git" / "index.lock"
    if lock_file.exists():
        raise RuntimeError(
            "Detected git lock file: "
            f"{lock_file}.\n"
            "Another git process may still be running, or a previous one crashed.\n"
            "If no git process is running, remove this lock file and retry."
        )


def ensure_remote(repo_root: pathlib.Path, remote: str, repo_url: str | None) -> None:
    remote_result = run_cmd(["git", "remote"], cwd=repo_root)
    remotes = {x.strip() for x in remote_result.stdout.splitlines() if x.strip()}

    if remote in remotes:
        if repo_url:
            run_cmd(["git", "remote", "set-url", remote, repo_url], cwd=repo_root)
        return

    if not repo_url:
        raise RuntimeError(
            f"Git remote '{remote}' does not exist. Please provide --repo-url to create it."
        )
    run_cmd(["git", "remote", "add", remote, repo_url], cwd=repo_root)


def has_staged_changes(repo_root: pathlib.Path, pathspec: str = ".") -> bool:
    result = run_cmd(
        ["git", "diff", "--cached", "--quiet", "--", pathspec],
        cwd=repo_root,
        check=False,
    )
    return result.returncode == 1


def sync_to_github(
    repo_root: pathlib.Path,
    remote: str,
    branch: str | None,
    repo_url: str | None,
    commit_message: str,
    allow_parent_repo: bool,
) -> None:
    if not is_git_repo(repo_root):
        raise RuntimeError(f"{repo_root} is not a git repository.")

    git_toplevel = get_git_toplevel(repo_root)
    if git_toplevel != repo_root:
        if not allow_parent_repo:
            raise RuntimeError(
                "Current folder is NOT a standalone git repository.\n"
                f"repo_root: {repo_root}\n"
                f"git_top_level: {git_toplevel}\n\n"
                "This usually means your folder is inside a larger repo, so it will not appear as a separate "
                "GitHub project.\n"
                "Fix:\n"
                "1) cd to this folder\n"
                "2) run: git init\n"
                "3) add your own remote, e.g. git remote add origin git@github.com:<you>/sensor.git\n"
                "4) rerun this script\n\n"
                "If you intentionally want to commit to parent repo, pass --allow-parent-repo."
            )
        print(
            f"[warn] git top-level is {git_toplevel}; only changes under {repo_root} will be staged/committed."
        )
    ensure_no_git_index_lock(git_toplevel)

    readme_changed = update_root_readme(repo_root)
    if readme_changed:
        print("[info] Root README project table updated.")
    else:
        print("[info] Root README project table unchanged.")

    ensure_remote(repo_root, remote=remote, repo_url=repo_url)

    target_branch = branch or get_current_branch(repo_root)
    # Limit operations to current repo_root path to avoid scanning unrelated upper-level files.
    run_cmd(["git", "add", "-A", "--", "."], cwd=repo_root)

    if not has_staged_changes(repo_root, pathspec="."):
        print("[info] No changes to commit. Nothing to push.")
        return

    run_cmd(["git", "commit", "-m", commit_message, "--", "."], cwd=repo_root)
    run_cmd(["git", "push", remote, target_branch], cwd=repo_root)
    print(f"[ok] Pushed to {remote}/{target_branch}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Update root README project table and sync this repository to GitHub."
    )
    parser.add_argument(
        "--repo-root",
        default=".",
        help="Path to sensor repository root. Default: current directory.",
    )
    parser.add_argument(
        "--remote",
        default="origin",
        help="Git remote name. Default: origin",
    )
    parser.add_argument(
        "--branch",
        default=None,
        help="Branch to push. Default: current branch",
    )
    parser.add_argument(
        "--repo-url",
        default=None,
        help="Optional GitHub repo url. If remote does not exist, it will be created.",
    )
    parser.add_argument(
        "--commit-message",
        default=None,
        help="Optional commit message. Default includes current date.",
    )
    parser.add_argument(
        "--allow-parent-repo",
        action="store_true",
        help="Allow running when current folder is inside a parent git repository.",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    repo_root = pathlib.Path(args.repo_root).resolve()
    if not repo_root.exists():
        print(f"[error] repo root does not exist: {repo_root}", file=sys.stderr)
        return 1

    commit_message = args.commit_message or f"chore: sync sensor ({dt.datetime.now().strftime('%Y-%m-%d %H:%M')})"

    try:
        sync_to_github(
            repo_root=repo_root,
            remote=args.remote,
            branch=args.branch,
            repo_url=args.repo_url,
            commit_message=commit_message,
            allow_parent_repo=args.allow_parent_repo,
        )
    except Exception as exc:
        print(f"[error] {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
