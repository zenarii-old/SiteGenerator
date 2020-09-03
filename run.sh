#!/bin/bash

pushd . > /dev/null
cd build
./sitegen --header header.html --footer footer.html --author "Alfie Baxter" --title "Sitegen" test.zsl
popd > /dev/null