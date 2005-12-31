#!/bin/bash -ex
# Release script run before generating a release tarball.
# Usage: ./.release.sh VERSION

exec ./.update-po.sh
