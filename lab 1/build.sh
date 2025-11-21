#!/bin/bash

set -eu

cleanup() {
    rc=$?
    [ -d "$tmpdir" ] && rm -rf -- "$tmpdir"
    exit $rc
}



if [ $# -ne 1 ]; then
    echo "Usage: $0 <sourcefile>"
    exit 2
fi

src=$1

[ -f "$src" ] || { echo "Source file not found: $src"; exit 3; }

srcname=$(basename -- "$src")
srcdir=$(cd "$(dirname -- "$src")" && pwd)

output=$(
    awk '{
        pos = index($0, "Output:");
        if (pos) {
            s = substr($0, pos + 8)
            sub(/^[ \t]+/, "", s);
            sub(/[ \t\r\n]+$/, "", s);
            if (s ~ /^".*"$/ || s ~ /^'\''.*'\''$/)
                s = substr(s, 2, length(s)-2);
            print s;
            exit
        }
    }
    ' "$src"
)

if [ -z "$output" ]; then
    echo "No 'Output:' comment found in $src"
    exit 4
fi

outname=$(basename -- "$output")

tmpdir=$(mktemp -d) || { echo "mktemp failed"; exit 5; }

trap 'cleanup' EXIT SIGINT SIGTERM SIGHUP SIGQUIT

cp -- "$src" "$tmpdir/"
cd "$tmpdir" || { echo "cd to tmpdir failed"; exit 6; }

case "${srcname##*.}" in
    c) suf=c ;;
    cpp|cc|cxx) suf=cpp ;;
    tex) suf=tex ;;
    *) { echo "Unknown source type: $srcname"; exit 7; } ;;
esac

case $suf in
    c) 
        cc -o "$outname" "$srcname" 2>script.log || { 
            echo "C compilation failed"; 
            cat script.log >&2; 
            exit 8; 
        } 
        ;;
    cpp) 
        c++ -o "$outname" "$srcname" 2>script.log || { 
            echo "C++ compilation failed"; 
            cat script.log >&2; 
            exit 8; 
        } 
        ;; 
    tex) 
        pdflatex -interaction=nonstopmode -halt-on-error -output-directory "$tmpdir" "$srcname" >/dev/null 2>&1 || { 
            echo "TeX compilation failed"; 
            exit 8; 
        } 
        ;;
esac

if [ -f "$outname" ]; then
    mv -- "$outname" "$srcdir/$outname" || { echo "Cannot move output file"; exit 9; }
elif [ "$suf" = "tex" ]; then
    pdfname="${srcname%.tex}.pdf"
    mv -- "$tmpdir/$pdfname" "$srcdir/$outname.pdf" || { echo "Cannot move pdf"; exit 9; }
else
    echo "Output file not produced"
    exit 9
fi

exit 0