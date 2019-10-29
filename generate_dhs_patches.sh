#!/bin/sh
# generate_dhs_patches - generate patches from two different manifests
# shellcheck disable=SC2039
# note: we require POSIX + local

appname=${0##*/}

create_tagged_manifest()
# usage: create_tagged_manifest <file>
# desc: create tagged manifest from repo
{
    repo manifest -r -o "$1"
}

repo_reset()
# usage: repo_reset
# description: reset all project tracked by repo
{
    # Restore tracked files
    repo forall -c git reset --hard HEAD
    # Remove all untracked files (eg. previous created patches)
    repo forall -c git clean -d -x -f
}

generate_patches()
# usage: generate_patches
# description: generate patches the difference of before and after manifest
#              by using git format-patch from every project inside the manifests
{
    local oldifs=$IFS
    IFS='
'
    for repo_str in $(repo diffmanifests \
                      "$before_manifest" "$after_manifest" \
                      --raw --no-color --pretty-format=%H \
                          | grep -e ^C | \
                          sed -e 's/C // # Strip the change indicator' \
                              -e  "s|^\ || # Remove all the left over space in the front" \
                              -e 's|\ $|| # Same for the back' \
                              -e "s,\x1B\[[0-9;]*[a-zA-Z],,g # Remove all the control sequences") ; do

        IFS=$oldifs
        repo=$(echo "$repo_str" | cut -d ' ' -f1)
        commitid_before=$(echo "$repo_str" |cut -d ' ' -f2)
        commitid_after=$(echo "$repo_str" | cut -d ' ' -f3)
        (
            cd "$repo" || exit 1
            patches=$(git format-patch -N --zero-commit --full-index --no-signature "$commitid_before..$commitid_after")
            if [ "$destdir" ] ; then
                mkdir -p "$destdir/$repo"
                # shellcheck disable=SC2086
                # note: We want word splitting here
                cp $patches "$destdir/$repo/".
            fi
        )
        IFS='
'
    done
}

usage()
{
    cat <<EOF
usage: $appname -m <mode> [-d <destdir> -r <script> <before_manifest> <after_manifest>]

-m <diff,vendor> - generate patches eiter by camparing two manifest
                   or by generating a tagged manifest, runnning the vendor script
                   and then creating a manifests and comparing the two.
-r <script>      - vendor script to be run for vendor mode
-d <destdir>     - if given copy all generated patches to destdir.
EOF
}

args=m:r:d:

while getopts $args arg; do
    case $arg in
        m) mode=$OPTARG ;; # run repo_update script and reset or not
        r) repo_script=$OPTARG ;; # repo update script for -m switch
        d) destdir=$OPTARG ;; # destdir for createed patches
        h) usage; exit 0;;
        ?) usage; exit 1 ;;
    esac
done
shift $((${OPTIND:-1}-1))

if [ "$mode" = vendor ] ; then
    if [ -z "$repo_script" ] ; then
        echo "Need repo vendor script, exit" >&2
        exit 1
    fi
    tmp_dir=$(mktemp -d)
    for signal in TERM HUP QUIT; do
        trap 'rm -rf ${tmp_dir}; exit 1' $signal
    done
    unset signal
    trap 'rm -rf ${tmp_dir}; exit 1' INT

    repo_reset
    create_tagged_manifest "$tmp_dir"/before.xml
    "$repo_script"
    create_tagged_manifest "$tmp_dir"/after.xml

    before_manifest="$tmp_dir"/before.xml
    after_manifest="$tmp_dir"/after.xml

elif [  "$mode" = diff ] ; then
    if [ ! $# -eq 2 ] ; then
        echo "Manifests not given, exit" >&2
        exit 1
    fi
    before_manifest="$1"
    after_manifest="$2"
else
    echo "Unkown Mode or no mode, exit" >&2
    exit 1
fi
generate_patches "$before_manifest" "$after_manifest"
rm -rf "${tmp_dir}"
