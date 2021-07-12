#!/bin/bash

echo "Building the Coyote native scheduler..."

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

# Create build directory
rm -r ${THIS_DIR}/../build
mkdir ${THIS_DIR}/../build
pushd ${THIS_DIR}/../build

# Build the project
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
retVal=$?
if [ $retVal -eq 0 ]
then
  ninja
  retVal=$?
fi

popd

if [ $retVal -eq 0 ]
then
  echo "Successfully built the project."
else
  echo "Failed to build the project."
fi
exit $retVal
