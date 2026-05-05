#!/bin/sh
set -e
brew cleanup --prune=all attest
brew reinstall attest
