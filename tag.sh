#!/bin/bash

# Read version from VERSION file
VERSION=$(cat VERSION | tr -d '[:space:]')

if [ -z "$VERSION" ]; then
    echo "Error: VERSION file is empty or not found"
    exit 1
fi

# Check if version is in Changelist.txt
if ! grep -q "^$VERSION$" Changelist.txt; then
    echo "Error: Version $VERSION not found in Changelist.txt"
    echo "Please add an entry for this version before tagging"
    exit 1
fi

TAG="v$VERSION"

echo "Creating tag: $TAG"

# Check if tag already exists
if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Error: Tag $TAG already exists"
    exit 1
fi

# Create and push tag
git tag "$TAG"
git push origin "$TAG"

echo "Tag $TAG created and pushed"
