#include <atomic>
#include <bitset>
#include <iostream>
#include <thread>
#include "concurrentqueue.h"
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "tracy/TracyC.h"

constexpr int boardW = 1280;
constexpr int boardH = 800;
std::bitset<boardW * boardH> board;
std::bitset<boardW * boardH> nextBoard;
constexpr int numThreads = 4;
std::vector<std::thread> workThreads;
std::atomic_bool appRunning = ATOMIC_VAR_INIT(false);
std::atomic_int jobsFinished = ATOMIC_VAR_INIT(0);
moodycamel::ConcurrentQueue<Rectangle> workQueue;

constexpr char* const tracy_GameOfLife = "GameOfLifeLogicThread";
constexpr char* const tracy_Update = "Update";
constexpr char* const tracy_Waiting = "Waiting";
constexpr char* const tracy_Render = "Render";

inline size_t GetBitsetIndex(const size_t& x, const size_t& y) { return (y * boardW) + x; }

void GameOfLifeLogic() {
  while (appRunning) {
    Rectangle rect;
    if (workQueue.try_dequeue(rect)) {
      TracyCZoneN(GameOfLifeLogicThread, tracy_GameOfLife, true);
      // Game of life logic here:
      // For each location on the board, count the number of neighbors
      const size_t oX = static_cast<size_t>(rect.x);
      const size_t oY = static_cast<size_t>(rect.y);
      const size_t width = static_cast<size_t>(rect.width);
      const size_t height = static_cast<size_t>(rect.height);
      for (size_t x = oX; x < width + oX; ++x) {
        for (size_t y = oY; y < height + oY; ++y) {
          const size_t left = ((x - 1) + 1280) % 1280;
          const size_t right = ((x + 1) + 1280) % 1280;
          const size_t above = ((y - 1) + 800) % 800;
          const size_t below = ((y + 1) + 800) % 800;
          size_t neighbors = 0;
          bool aliveNextFrame = false;

          if (board.test(GetBitsetIndex(left, above))) ++neighbors;
          if (board.test(GetBitsetIndex(x, above))) ++neighbors;
          if (board.test(GetBitsetIndex(right, above))) ++neighbors;
          if (board.test(GetBitsetIndex(left, y))) ++neighbors;
          if (board.test(GetBitsetIndex(right, y))) ++neighbors;
          if (board.test(GetBitsetIndex(left, below))) ++neighbors;
          if (board.test(GetBitsetIndex(x, below))) ++neighbors;
          if (board.test(GetBitsetIndex(right, below))) ++neighbors;

          if (board.test(GetBitsetIndex(x, y))) {
            // If current cell is alive, it lives next frame if it has 2 or 3
            // neighbors
            aliveNextFrame = neighbors == 2 || neighbors == 3;
          } else {
            // Dead cells come back to life if they have exactly 3 live
            // neighbors
            aliveNextFrame = neighbors == 3;
          }
          nextBoard.set(GetBitsetIndex(x, y), aliveNextFrame);
        }
      }
      ++jobsFinished;
      TracyCZoneEnd(GameOfLifeLogicThread);
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

  // Create our work threads and let them know they can run.
  if (ATOMIC_INT_LOCK_FREE && ATOMIC_BOOL_LOCK_FREE) {
    std::cout << "Lock free atomics are available!" << std::endl;
  }
  appRunning = true;
  for (size_t i = 0; i < numThreads; ++i) {
    workThreads.emplace_back(std::thread{GameOfLifeLogic});
  }

  InitWindow(screenWidth, screenHeight, "Raylib Game of Life");

  Image boardImage = LoadImage("assets/glidergunHD.png");
  Image nextImage = GenImageColor(boardW, boardH, BLANK);
  const int gameWidth = boardW;
  const int gameHeight = boardH;
  const Vector2 origin{0, 0};
  const Rectangle gameRect{0, 0, static_cast<float>(gameWidth), static_cast<float>(gameHeight)};
  const Rectangle screenRect{0, 0, screenWidth, screenHeight};

  // Import board image into bitset
  for (size_t x = 0; x < gameWidth; ++x) {
    for (size_t y = 0; y < gameHeight; ++y) {
      board.set(GetBitsetIndex(x, y), GetImageColor(boardImage, x, y).a != 0);
    }
  }

  // NOTE: Textures MUST be loaded after Window initialization (OpenGL context
  // is required)
  Texture2D boardTexture = LoadTextureFromImage(boardImage);

  SetTargetFPS(60);  // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose())  // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    // Check if its time to run our frame logic
    TracyCZoneN(Update, tracy_Update, true);
    bool shouldUpdate = false;
    if (++frameCount > updateRate) {
      shouldUpdate = true;
      frameCount -= updateRate;
    }

    if (shouldUpdate) {
      // Reset work counter
      jobsFinished = 0;
      // Enqueue new work
      for (size_t i = 0; i < numThreads; ++i) {
        float heightSubdivision = static_cast<float>(gameHeight) / static_cast<float>(numThreads);
        float startY = heightSubdivision * static_cast<float>(i);
        workQueue.enqueue(Rectangle{0.f, startY, static_cast<float>(gameWidth), heightSubdivision});
      }
    }
    TracyCZoneEnd(Update);

    TracyCZoneN(Waiting, tracy_Waiting, true);
    while (shouldUpdate && jobsFinished != numThreads) {
      // Wait for jobs to finish
      std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
    TracyCZoneEnd(Waiting);

    // Draw
    //----------------------------------------------------------------------------------
    TracyCZoneN(Rendering, tracy_Render, true);
    BeginDrawing();

    ClearBackground(RAYWHITE);

    if (shouldUpdate) {
      // Write new board to image
      for (size_t x = 0; x < gameWidth; ++x) {
        for (size_t y = 0; y < gameHeight; ++y) {
          ImageDrawPixel(&nextImage, x, y, board.test(GetBitsetIndex(x, y)) ? PURPLE : BLANK);
        }
      }
      UpdateTexture(boardTexture, nextImage.data);
    }
    DrawTexturePro(boardTexture, gameRect, screenRect, origin, 0.0f, WHITE);

    DrawFPS(10, 780);

    if (shouldUpdate) {
      // Swap boards
      auto temp = board;
      board = nextBoard;
      nextBoard = temp;

      // Swap images
      auto temp1 = boardImage;
      boardImage = nextImage;
      nextImage = temp1;
    }
    TracyCZoneEnd(Rendering);

    EndDrawing();
    //----------------------------------------------------------------------------------
    TracyCFrameMark;
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow();  // Close window and OpenGL context

  // Shut down work threads
  appRunning = false;
  for (auto& t : workThreads) {
    t.join();
  }
  //--------------------------------------------------------------------------------------

  return 0;
}