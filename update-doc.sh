#!/bin/bash
## Automatically deploy documentation changes to Github gh-pages
## Snippet adapted from https://gist.github.com/domenic/ec8b0fc8ab45f39403dd

## Exit with nonzero exit code if anything fails
set -e

SHA=`git rev-parse --verify HEAD`

rm -rf ./docs
git clone $(git config remote.origin.url) --branch=gh-pages docs
( cd Xcode/ESP/doc/html && tar cf - . ) | (cd docs/ && tar xpf - )
cd docs

# If there are no changes to the compiled out (e.g. this is a README update) then just bail.
if [ -z `git diff --exit-code` ]; then
    echo "No changes to the output on this push; exiting."
    exit 0
fi

git config user.name "Travis CI"
git add .
git commit -m "Deploy to GitHub Pages: ${SHA}"
git push origin gh-pages
