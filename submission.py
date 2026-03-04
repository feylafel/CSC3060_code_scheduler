import zipfile
import glob
import os
from datetime import datetime
from pathlib import Path

def find_report():
    patterns = ["report.md", "report.pdf", "Report.md", "Report.pdf"]
    for p in patterns:
        matches = glob.glob(p, recursive=False)
        if matches:
            return matches[0]
    for f in os.listdir("."):
        lower = f.lower()
        if lower.startswith("report") and (lower.endswith(".md") or lower.endswith(".pdf")):
            return f
    return None

def main():
    print("Please enter your name (e.g. Xiao Ming): ", end="")
    name = input().strip()
    print("Please enter your student ID (e.g. 123090613): ", end="")
    student_id = input().strip()

    meta = f"student_id: {student_id}\nname: {name}\ndate: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
    meta_file = ".metainfo.txt"
    Path(meta_file).write_text(meta, encoding="utf-8")

    report = find_report()
    if report is None:
        print("Warning: No report file found (report.md / report.pdf etc.)")

    src_files = list(Path("src").rglob("*")) if Path("src").exists() else []
    extra_files = [Path("CMakeLists.txt")] if Path("CMakeLists.txt").exists() else []

    zip_name = f"{student_id}_submission.zip"
    with zipfile.ZipFile(zip_name, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.write(meta_file)
        for f in src_files:
            if f.is_file():
                zf.write(f)
        for f in extra_files:
            zf.write(f)
        if report:
            zf.write(report)

    os.remove(meta_file)
    print(f"Generate to: {zip_name}")

if __name__ == "__main__":
    main()
