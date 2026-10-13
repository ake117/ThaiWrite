#!/bin/sh
# check-src-pristine.sh — guard สำหรับกฎ "ห้ามแตะต้นฉบับ"
#
# ตรวจว่าทุกไฟล์/โฟลเดอร์ของ source ต้นฉบับ CU Writer 1.41 ยังเหมือนกับ
# upstream/master เป๊ะ (งาน port ของเราต้องเป็น "ไฟล์เพิ่ม" เท่านั้น)
#
#   usage: tools/check-src-pristine.sh [ref]
#          ref = commit/tag ที่ใช้เทียบ (default: upstream/master,
#                ถ้าไม่มี remote นี้จะ fallback เป็น fork/master)
#
# exit 0 = ต้นฉบับสะอาด, exit 1 = มีไฟล์ต้นฉบับถูกแก้ (พร้อมรายการ)

set -e

cd "$(git rev-parse --show-toplevel)"

REF="${1:-}"
if [ -z "$REF" ]; then
    for cand in upstream/master fork/master; do
        if git rev-parse --verify --quiet "$cand" >/dev/null; then
            REF="$cand"
            break
        fi
    done
fi

if [ -z "$REF" ] || ! git rev-parse --verify --quiet "$REF" >/dev/null; then
    echo "!! หา ref สำหรับเทียบไม่เจอ — ลอง: git fetch upstream" >&2
    echo "   ใช้: tools/check-src-pristine.sh <ref>" >&2
    exit 2
fi

echo "== เทียบกับ: $REF =="

# ไฟล์/โฟลเดอร์ที่ถือเป็น "ต้นฉบับ" (นอกนั้นคือของเราที่เพิ่มเข้ามา)
# หมายเหตุ: README.md / QREF.md ไม่อยู่ในลิสต์นี้ — เป็นเอกสาร ไม่ใช่ source code
# (fork ของเรา prepend หัวเรื่องของงาน port ไว้บนสุดของ README.md โดยคงเนื้อหาเดิมไว้ครบ)
ORIGINAL_PATHS="SRC MANUAL ITALIC.FON ITALIC.P24 ITALIC.P9 ITALICS.P24 ITALICS.P9 \
NORMAL.FON NORMAL.P24 NORMAL.P9 NORMALS.P24 NORMALS.P9 \
TAIRUT.DOC TEST.DOC TEST2.DOC TEST3.DOC"

# --diff-filter=MD : เอาเฉพาะ Modified/Deleted ของ path ต้นฉบับ
# (Added คือสิ่งที่เราตั้งใจเพิ่ม, Renamed/Copied ไม่ควรมี)
CHANGED=$(git diff --name-status --diff-filter=MDRC "$REF" HEAD -- $ORIGINAL_PATHS 2>/dev/null || true)

if [ -n "$CHANGED" ]; then
    echo "FAIL: พบไฟล์ต้นฉบับถูกแก้/ลบ:" >&2
    echo "$CHANGED" >&2
    echo "" >&2
    echo "ให้ย้ายการแก้ไปที่ชั้น shim / build-time patch ลงสำเนาใน build/" >&2
    exit 1
fi

echo "OK: ไฟล์ต้นฉบับทั้งหมดยังเหมือน $REF เป๊ะ (งานของเราเป็นไฟล์เพิ่มเท่านั้น)"
