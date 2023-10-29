#include <atomic>
#include <bitset>
#include <cmath>
#include <thread>
#include <vector>
#include "concurrentqueue.h"
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "tracy/TracyC.h"

constexpr size_t screenWidth = 1280;
constexpr size_t screenHeight = 800;
constexpr size_t boardW = screenWidth;
constexpr size_t boardH = screenHeight;

// Game state
std::bitset<boardW * boardH> board;
std::bitset<boardW * boardH> nextBoard;

// Threads and synchronization
constexpr size_t numThreads = 4;
std::vector<std::thread> workThreads;
std::atomic_bool appRunning = ATOMIC_VAR_INIT(false);
std::atomic_bool jobsReady = ATOMIC_VAR_INIT(false);
std::atomic_int numJobsFinished = ATOMIC_VAR_INIT(0);
std::atomic_bool allJobsFinished = ATOMIC_VAR_INIT(false);
moodycamel::ConcurrentQueue<Rectangle> workQueue;

// Optimizations for reducing amount of drawing per update
constexpr size_t subDivisionSize = 32;
constexpr size_t numXSubdivisions = boardW / subDivisionSize;
constexpr size_t numYSubdivisions = boardH / subDivisionSize;
std::array<Rectangle, numXSubdivisions * numYSubdivisions> drawRegions;
std::bitset<drawRegions.size()> dirtyRegions;

// Constants for profiling
const char* tracyWorkTag = "GameOfLifeWork";
const char* tracyUpdateTag = "Update";
const char* tracyRenderTag = "Render";
const char* tracyEndDrawingTag = "EndDrawing";

inline size_t GetDrawRegionIndex(const size_t& x, const size_t& y) { return (y * numXSubdivisions) + x; }
inline size_t GetBoardBitsetIndex(const size_t& x, const size_t& y) { return (y * boardW) + x; }

void GameOfLifeLogic() {
  while (appRunning) {
    Rectangle workRegion;
    if (workQueue.try_dequeue(workRegion)) {
      TracyCZoneNS(GameOfLifeLogicThread, tracyWorkTag, 8, true);
      // For each location on the board, count the number of neighbors
      const size_t oX = static_cast<size_t>(workRegion.x);
      const size_t oY = static_cast<size_t>(workRegion.y);
      const size_t width = static_cast<size_t>(workRegion.width);
      const size_t height = static_cast<size_t>(workRegion.height);
      for (size_t x = oX; x < width + oX; ++x) {
        for (size_t y = oY; y < height + oY; ++y) {
          const size_t left = (x > 0) ? x - 1 : boardW - 1;
          const size_t right = (x < boardW - 1) ? x + 1 : 0;
          const size_t above = (y > 0) ? y - 1 : boardH - 1;
          const size_t below = (y < boardH - 1) ? y + 1 : 0;
          size_t neighbors = 0;
          bool aliveNextFrame = false;

          if (board.test(GetBoardBitsetIndex(left, above))) ++neighbors;
          if (board.test(GetBoardBitsetIndex(x, above))) ++neighbors;
          if (board.test(GetBoardBitsetIndex(right, above))) ++neighbors;
          if (board.test(GetBoardBitsetIndex(left, y))) ++neighbors;
          if (board.test(GetBoardBitsetIndex(right, y))) ++neighbors;
          if (board.test(GetBoardBitsetIndex(left, below))) ++neighbors;
          if (board.test(GetBoardBitsetIndex(x, below))) ++neighbors;
          if (board.test(GetBoardBitsetIndex(right, below))) ++neighbors;

          const auto currentlyAlive = board.test(GetBoardBitsetIndex(x, y));
          if (currentlyAlive) {
            // If current cell is alive, it lives next frame if it has 2 or 3 neighbors
            aliveNextFrame = neighbors == 2 || neighbors == 3;
          } else {
            // Dead cells come back to life if they have exactly 3 live neighbors
            aliveNextFrame = neighbors == 3;
          }
          nextBoard.set(GetBoardBitsetIndex(x, y), aliveNextFrame);

          // Remember where we've modified this frame so we know what region to redraw
          if (currentlyAlive != aliveNextFrame) {
            // Using shift by 5 as a quick divide by 32 to avoid division in hot loop
            dirtyRegions.set(GetDrawRegionIndex(x >> 5, y >> 5), true);
          }
        }
      }
      ++numJobsFinished;

      if (numJobsFinished == numThreads) {
        allJobsFinished = true;
        allJobsFinished.notify_all();
      }
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
  constexpr size_t updateRate = 1;  // every N frames
  size_t frameCount = 0;

  // Create our work threads and let them know they can run.
  appRunning = true;
  for (size_t i = 0; i < numThreads; ++i) {
    workThreads.emplace_back(std::thread{GameOfLifeLogic});
  }

  InitWindow(screenWidth, screenHeight, "Raylib Game of Life");

  // The initial board state is loaded as an image, makes it easy to inspect and edit
  Image boardImage = LoadImage("assets/glidergunHD.png");

  // Drawing size/location of the board
  const Vector2 origin{0, 0};
  const Rectangle gameRect{0, 0, static_cast<float>(boardW), static_cast<float>(boardH)};
  const Rectangle screenRect{0, 0, screenWidth, screenHeight};

  // Establish dirty regions which we use to mark where to redraw the board
  for (size_t x = 0; x < numXSubdivisions; ++x) {
    for (size_t y = 0; y < numYSubdivisions; ++y) {
      drawRegions[GetDrawRegionIndex(x, y)].x = x * subDivisionSize;
      drawRegions[GetDrawRegionIndex(x, y)].y = y * subDivisionSize;
      drawRegions[GetDrawRegionIndex(x, y)].width = subDivisionSize;
      drawRegions[GetDrawRegionIndex(x, y)].height = subDivisionSize;
    }
  }

  // Import board image into bitset
  for (size_t x = 0; x < boardW; ++x) {
    for (size_t y = 0; y < boardH; ++y) {
      board.set(GetBoardBitsetIndex(x, y), GetImageColor(boardImage, x, y).a != 0);
    }
  }

  // NOTE: Textures MUST be loaded after Window initialization (OpenGL context is required)
  Texture2D boardTexture = LoadTextureFromImage(boardImage);
  UpdateTexture(boardTexture, boardImage.data);

  SetTargetFPS(60);  // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose())  // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    // Check if its time to run our frame logic
    TracyCZoneNS(Update, tracyUpdateTag, 8, true);
    bool shouldUpdate = false;
    if (frameCount++ > updateRate) {
      shouldUpdate = true;
      frameCount -= updateRate;
    }

    if (shouldUpdate) {
      // Mark no areas as dirty
      dirtyRegions.reset();
      // Reset work counter
      allJobsFinished = false;
      numJobsFinished = 0;
      // Enqueue new work
      for (size_t i = 0; i < numThreads; ++i) {
        // Subdivide the board by height into sets of rows which each are independently processed by a work thread
        float heightSubdivision = static_cast<float>(boardH) / static_cast<float>(numThreads);
        float startY = heightSubdivision * static_cast<float>(i);
        workQueue.enqueue(Rectangle{0.f, startY, static_cast<float>(boardW), heightSubdivision});
      }
      // Start work
      jobsReady = true;
      jobsReady.notify_all();
    }
    TracyCZoneEnd(Update);

    if (shouldUpdate) {
      allJobsFinished.wait(false);
    }

    // Draw
    //----------------------------------------------------------------------------------
    TracyCZoneNS(Rendering, tracyRenderTag, 8, true);
    BeginDrawing();

    ClearBackground(RAYWHITE);

    if (shouldUpdate) {
      // Write new board to image, only redrawing the dirty portions of the screen
      bool imageChanged = false;
      for (size_t regionIdx = 0; regionIdx < drawRegions.size(); ++regionIdx) {
        if (dirtyRegions.test(regionIdx)) {
          auto const& region = drawRegions[regionIdx];
          for (size_t x = region.x; x < region.width + region.x; ++x) {
            for (size_t y = region.y; y < region.height + region.y; ++y) {
              ImageDrawPixel(&boardImage, x, y, board.test(GetBoardBitsetIndex(x, y)) ? PURPLE : BLANK);
            }
          }
          imageChanged = true;
        }
      }
      if (imageChanged) {
        UpdateTexture(boardTexture, boardImage.data);
      }
    }
    DrawTexturePro(boardTexture, gameRect, screenRect, origin, 0.f, WHITE);

    DrawFPS(10, 780);

    if (shouldUpdate) {
      std::swap(board, nextBoard);
    }
    TracyCZoneEnd(Rendering);

    TracyCZoneNS(EndOfDrawing, tracyEndDrawingTag, 8, true);
    EndDrawing();
    TracyCZoneEnd(EndOfDrawing);
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