if [ "$(uname)" == "Darwin" ]; then
  build/bin/raylib_life
elif [ "$(expr substr $(uname -s) 1 5)" == "MINGW" ]; then
  build/bin/Release/raylib_life
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
  echo "Linux build not yet supported/tested"
fi