#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

inline bool ColorsAreEqual(const Color& c1, const Color& c2) {
  return c1.a == c2.a && c1.b == c2.b && c1.g == c2.g && c1.r == c2.r;
}

inline bool ColorsEqualAlpha(const Color& c1, const Color& c2) { return c1.a == c2.a; }

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
  // Initialization
  //--------------------------------------------------------------------------------------
  const int screenWidth = 1280;
  const int screenHeight = 800;

  const int updateRate = 1;  // every N frames
  int frameCount = 0;

  InitWindow(screenWidth, screenHeight, "Raylib Game of Life");

  Image board = LoadImage("assets/glidergunHD.png");
  const int gameWidth = board.width;
  const int gameHeight = board.height;
  const Vector2 origin{0, 0};
  const Rectangle gameRect{0, 0, static_cast<float>(gameWidth), static_cast<float>(gameHeight)};
  const Rectangle screenRect{0, 0, screenWidth, screenHeight};
  Image nextBoard = GenImageColor(gameWidth, gameHeight, BLANK);

  // NOTE: Textures MUST be loaded after Window initialization (OpenGL context
  // is required)
  Texture2D boardTexture = LoadTextureFromImage(board);

  SetTargetFPS(60);  // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose())  // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    // Game of life logic here:
    // For each location on the board, count the number of neighbors
    for (size_t x = 0; x < gameWidth; ++x) {
      for (size_t y = 0; y < gameHeight; ++y) {
        const int left = ((x - 1) + gameWidth) % gameWidth;
        const int right = ((x + 1) + gameWidth) % gameWidth;
        const int above = ((y - 1) + gameHeight) % gameHeight;
        const int below = ((y + 1) + gameHeight) % gameHeight;
        int neighbors = 0;
        bool aliveNextFrame = false;

        if (!ColorsEqualAlpha(GetImageColor(board, left, above), BLANK)) ++neighbors;
        if (!ColorsEqualAlpha(GetImageColor(board, x, above), BLANK)) ++neighbors;
        if (!ColorsEqualAlpha(GetImageColor(board, right, above), BLANK)) ++neighbors;
        if (!ColorsEqualAlpha(GetImageColor(board, left, y), BLANK)) ++neighbors;
        if (!ColorsEqualAlpha(GetImageColor(board, right, y), BLANK)) ++neighbors;
        if (!ColorsEqualAlpha(GetImageColor(board, left, below), BLANK)) ++neighbors;
        if (!ColorsEqualAlpha(GetImageColor(board, x, below), BLANK)) ++neighbors;
        if (!ColorsEqualAlpha(GetImageColor(board, right, below), BLANK)) ++neighbors;

        if (!ColorsEqualAlpha(GetImageColor(board, x, y), BLANK)) {
          // If current cell is alive, it lives next frame if it has 2 or 3
          // neighbors
          aliveNextFrame = neighbors == 2 || neighbors == 3;
        } else {
          // Dead cells come back to life if they have exactly 3 live
          // neighbors
          aliveNextFrame = neighbors == 3;
        }
        ImageDrawPixel(&nextBoard, x, y, aliveNextFrame ? PURPLE : BLANK);
      }
    }

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    UpdateTexture(boardTexture, nextBoard.data);
    DrawTexturePro(boardTexture, gameRect, screenRect, origin, 0.0f, WHITE);

    DrawFPS(10, 780);

    // Swap boards
    auto temp = board;
    board = nextBoard;
    nextBoard = temp;

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow();  // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}