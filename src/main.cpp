#include <thread>
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

inline bool ColorsAreEqual(const Color& c1, const Color& c2) {
  return c1.a == c2.a && c1.b == c2.b && c1.g == c2.g && c1.r == c2.r;
}

inline bool ColorsEqualAlpha(const Color& c1, const Color& c2) { return c1.a == c2.a; }

Image board;
Image nextBoard;
constexpr int numThreads = 8;

void GameOfLifeLogic(Rectangle rect) {
  // Game of life logic here:
  // For each location on the board, count the number of neighbors
  const int16_t oX = static_cast<int16_t>(rect.x);
  const int16_t oY = static_cast<int16_t>(rect.y);
  const int16_t width = static_cast<int16_t>(rect.width);
  const int16_t height = static_cast<int16_t>(rect.height);
  for (int16_t x = oX; x < width + oX; ++x) {
    for (int16_t y = oY; y < height + oY; ++y) {
      const int16_t left = ((x - 1) + 1280) % 1280;
      const int16_t right = ((x + 1) + 1280) % 1280;
      const int16_t above = ((y - 1) + 800) % 800;
      const int16_t below = ((y + 1) + 800) % 800;
      int8_t neighbors = 0;
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
}

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

  board = LoadImage("assets/glidergunHD.png");
  const int gameWidth = board.width;
  const int gameHeight = board.height;
  const Vector2 origin{0, 0};
  const Rectangle gameRect{0, 0, static_cast<float>(gameWidth), static_cast<float>(gameHeight)};
  const Rectangle screenRect{0, 0, screenWidth, screenHeight};
  nextBoard = GenImageColor(gameWidth, gameHeight, BLANK);

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
    // Check if its time to run our frame logic
    bool shouldUpdate = false;
    if (frameCount++ > updateRate) {
      shouldUpdate = true;
      frameCount -= updateRate;
    }

    if (shouldUpdate) {
      std::vector<std::thread> workThreads;
      for (size_t i = 0; i < numThreads; ++i) {
        float heightSubdivision = static_cast<float>(gameHeight) / static_cast<float>(numThreads);
        float startY = heightSubdivision * static_cast<float>(i);
        workThreads.emplace_back(
            std::thread{GameOfLifeLogic, Rectangle{0.f, startY, static_cast<float>(gameWidth), heightSubdivision}});
      }
      for (auto& t : workThreads) {
        t.join();
      }
    }

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    if (shouldUpdate) {
      UpdateTexture(boardTexture, nextBoard.data);
    }
    DrawTexturePro(boardTexture, gameRect, screenRect, origin, 0.0f, WHITE);

    DrawFPS(10, 780);

    if (shouldUpdate) {
      // Swap boards
      auto temp = board;
      board = nextBoard;
      nextBoard = temp;
    }

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow();  // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}