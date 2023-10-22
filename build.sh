if [ "$(uname)" == "Darwin" ]; then
  cmake -B build -DPLATFORM=Desktop -DCMAKE_BUILD_TYPE=Release -DUSE_EXTERNAL_GLFW=ON
  cd build
  make
  cd ..
elif [ "$(expr substr $(uname -s) 1 5)" == "MINGW" ]; then
  cmake -B build -DPLATFORM=Desktop -DCMAKE_BUILD_TYPE=Release -DUSE_EXTERNAL_GLFW=OFF
  cd build
  # You might need to change this if you have a different version of VS installed
  MSBUILD_PATH="/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/"
  "${MSBUILD_PATH}"/MSBuild.exe ALL_BUILD.vcxproj -p:Configuration=Release
  msbuild.exe ALL_BUILD.vcxproj -p:Configuration=Release
  cd ..
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
  echo "Linux build not yet supported/tested"
fi