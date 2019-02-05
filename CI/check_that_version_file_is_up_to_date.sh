#!/bin/sh

script_dir=$(cd $(dirname "$0") && pwd)
version_file=$(realpath $script_dir/../VERSION)

version_from_file=$(cat $version_file)
short_version_from_git=$(git describe --abbrev=0)   # get version information only from last tag (the plain "git describe" command
                                                    # also includes information about distance to last tag and SHA hash)

if [ "$version_from_file" != "$short_version_from_git" ]; then
    echo "version in file $version_file ($version_from_file) doesn't match the short version from git ($short_version_from_git)."
    echo "Either you forgot to push an updated git tag or you forgot to update the version file $version_file."
    exit 1
fi
